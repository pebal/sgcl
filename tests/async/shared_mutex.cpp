//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    // What every holder checks: readers see no writer, a writer sees
    // nobody else
    struct Guarded {
        sgcl::shared_mutex m;
        std::atomic<int> readers = {0};
        std::atomic<int> writers = {0};
        std::atomic<int> violations = {0};
        std::atomic<int> peak = {0};

        void reader_in() {
            int now = ++readers;
            if (writers.load() != 0) {
                ++violations;
            }
            int p = peak.load();
            while (now > p && !peak.compare_exchange_weak(p, now)) {
            }
        }

        void reader_out() {
            if (writers.load() != 0) {
                ++violations;
            }
            --readers;
        }

        void writer_in() {
            if (++writers != 1 || readers.load() != 0) {
                ++violations;
            }
        }

        void writer_out() {
            if (writers.load() != 1 || readers.load() != 0) {
                ++violations;
            }
            --writers;
        }
    };
}

TEST(SharedMutex_Test, SixteenReadersHoldTheLockAtOnce) {
    Guarded g;
    sgcl::event all_in;
    std::vector<sgcl::task<>> tasks;
    for (int i = 0; i < 16; ++i) {
        tasks.push_back(sgcl::spawn([](Guarded& g, sgcl::event& all_in) -> sgcl::task<> {
            auto lock = co_await g.m.async_scoped_lock_shared();
            g.reader_in();
            co_await all_in.async_wait();   // held until every reader is in
            g.reader_out();
        }(g, all_in)));
    }
    while (g.readers.load() < 16) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_FALSE(g.m.try_lock());        // a writer cannot get in
    all_in.set();
    for (auto& t : tasks) {
        t.join();
    }
    EXPECT_EQ(g.peak, 16);
    EXPECT_EQ(g.readers, 0);
    EXPECT_EQ(g.violations, 0);
    EXPECT_TRUE(g.m.try_lock());
    EXPECT_FALSE(g.m.try_lock_shared());
    g.m.unlock();
    EXPECT_TRUE(g.m.try_lock_shared());
    EXPECT_TRUE(g.m.try_lock_shared());
    g.m.unlock_shared();
    g.m.unlock_shared();
    sgcl::scheduler::stop();
}

TEST(SharedMutex_Test, AWriterExcludesReadersAndReadersTheWriter) {
    Guarded g;
    // a writer holds: readers wait, a thread's and a task's
    g.m.lock();
    g.writer_in();
    std::atomic<int> in = {0};
    std::thread reader([&] {
        std::shared_lock lock(g.m);      // a SharedLockable
        g.reader_in();
        ++in;
        g.reader_out();
    });
    auto task = sgcl::spawn([](Guarded& g, std::atomic<int>& in) -> sgcl::task<> {
        co_await g.m.async_lock_shared();
        g.reader_in();
        ++in;
        g.reader_out();
        g.m.unlock_shared();
    }(g, in));
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(in, 0);
    g.writer_out();
    g.m.unlock();
    reader.join();
    task.join();
    EXPECT_EQ(in, 2);
    // readers hold: the writer waits, a task's and a thread's
    g.m.lock_shared();
    g.reader_in();
    std::atomic<int> written = {0};
    auto writer = sgcl::spawn([](Guarded& g, std::atomic<int>& written) -> sgcl::task<> {
        auto lock = co_await g.m.async_scoped_lock();
        g.writer_in();
        ++written;
        g.writer_out();
    }(g, written));
    std::thread writer_thread([&] {
        std::lock_guard lock(g.m);       // a Lockable
        g.writer_in();
        ++written;
        g.writer_out();
    });
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(written, 0);
    g.reader_out();
    g.m.unlock_shared();
    writer.join();
    writer_thread.join();
    EXPECT_EQ(written, 2);
    EXPECT_EQ(g.violations, 0);
    sgcl::scheduler::stop();
}

TEST(SharedMutex_Test, AWaitingWriterGoesBeforeTheReadersAfterIt) {
    Guarded g;
    std::vector<int> order;                // guarded by the lock itself: every entry is made with the lock held
    // a reader holds; a writer arrives and waits; a reader arriving after
    // the writer is held back; the writer goes first, then that reader,
    // then a writer that arrived after the held-back reader (which was
    // counted, so the second writer waits for it)
    g.m.lock_shared();
    auto w1 = sgcl::spawn([](Guarded& g, std::vector<int>& order) -> sgcl::task<> {
        auto lock = co_await g.m.async_scoped_lock();
        order.push_back(1);
        co_await sgcl::sleep(10ms);
    }(g, order));
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(g.m.try_lock_shared());   // the writer waits: no new reader
    auto r2 = sgcl::spawn([](Guarded& g, std::vector<int>& order) -> sgcl::task<> {
        auto lock = co_await g.m.async_scoped_lock_shared();
        order.push_back(2);
        co_await sgcl::sleep(10ms);
    }(g, order));
    std::this_thread::sleep_for(20ms);
    auto w3 = sgcl::spawn([](Guarded& g, std::vector<int>& order) -> sgcl::task<> {
        auto lock = co_await g.m.async_scoped_lock();
        order.push_back(3);
    }(g, order));
    std::this_thread::sleep_for(20ms);
    EXPECT_TRUE(order.empty());            // all of them wait for this reader
    g.m.unlock_shared();
    w1.join();
    r2.join();
    w3.join();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
    sgcl::scheduler::stop();
}

TEST(SharedMutex_Test, ThreadsAndTasksMixed) {
    Guarded g;
    std::atomic<long> reads = {0}, writes = {0};
    std::vector<sgcl::task<>> tasks;
    for (int i = 0; i < 8; ++i) {
        tasks.push_back(sgcl::spawn([](Guarded& g, std::atomic<long>& reads) -> sgcl::task<> {
            for (int k = 0; k < 2000; ++k) {
                auto lock = co_await g.m.async_scoped_lock_shared();
                g.reader_in();
                ++reads;
                g.reader_out();
            }
        }(g, reads)));
    }
    for (int i = 0; i < 2; ++i) {
        tasks.push_back(sgcl::spawn([](Guarded& g, std::atomic<long>& writes) -> sgcl::task<> {
            for (int k = 0; k < 500; ++k) {
                co_await g.m.async_lock();
                g.writer_in();
                ++writes;
                g.writer_out();
                g.m.unlock();
                if (k % 50 == 0) {
                    co_await sgcl::yield();
                }
            }
        }(g, writes)));
    }
    std::vector<std::thread> threads;
    for (int i = 0; i < 2; ++i) {
        threads.emplace_back([&] {
            for (int k = 0; k < 2000; ++k) {
                g.m.lock_shared();
                g.reader_in();
                ++reads;
                g.reader_out();
                g.m.unlock_shared();
            }
        });
        threads.emplace_back([&] {
            for (int k = 0; k < 500; ++k) {
                g.m.lock();
                g.writer_in();
                ++writes;
                g.writer_out();
                g.m.unlock();
            }
        });
    }
    for (auto& t : tasks) {
        t.join();
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(reads, 20000);
    EXPECT_EQ(writes, 2000);
    EXPECT_EQ(g.violations, 0);
    EXPECT_GE(g.peak, 2);                  // readers did share
    EXPECT_TRUE(g.m.try_lock());
    g.m.unlock();
    sgcl::scheduler::stop();
}
