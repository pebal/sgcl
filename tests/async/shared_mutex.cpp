//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

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
        sgcl::async::shared_mutex m;
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
    sgcl::async::event all_in;
    std::vector<sgcl::async::task<>> tasks;
    for (int i = 0; i < 16; ++i) {
        tasks.push_back(sgcl::async::spawn([](Guarded& g, sgcl::async::event& all_in) -> sgcl::async::task<> {
            auto lock = co_await g.m.scoped_lock_shared();
            g.reader_in();
            co_await all_in;   // held until every reader is in
            g.reader_out();
        }(g, all_in)));
    }
    while (g.readers.load() < 16) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_FALSE(g.m.try_lock());        // a writer cannot get in
    all_in.set();
    for (auto& t : tasks) {
        t.wait();
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
    sgcl::async::scheduler::stop();
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
    auto task = sgcl::async::spawn([](Guarded& g, std::atomic<int>& in) -> sgcl::async::task<> {
        (void)(co_await g.m.scoped_lock_shared()).release();
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
    task.wait();
    EXPECT_EQ(in, 2);
    // readers hold: the writer waits, a task's and a thread's
    g.m.lock_shared();
    g.reader_in();
    std::atomic<int> written = {0};
    auto writer = sgcl::async::spawn([](Guarded& g, std::atomic<int>& written) -> sgcl::async::task<> {
        auto lock = co_await g.m.scoped_lock();
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
    writer.wait();
    writer_thread.join();
    EXPECT_EQ(written, 2);
    EXPECT_EQ(g.violations, 0);
    sgcl::async::scheduler::stop();
}

TEST(SharedMutex_Test, AWaitingWriterGoesBeforeTheReadersAfterIt) {
    Guarded g;
    std::vector<int> order;                // guarded by the lock itself: every entry is made with the lock held
    // a reader holds; a writer arrives and waits; a reader arriving after
    // the writer is held back; the writer goes first, then that reader,
    // then a writer that arrived after the held-back reader (which was
    // counted, so the second writer waits for it)
    g.m.lock_shared();
    auto w1 = sgcl::async::spawn([](Guarded& g, std::vector<int>& order) -> sgcl::async::task<> {
        auto lock = co_await g.m.scoped_lock();
        order.push_back(1);
        co_await sgcl::async::sleep(10ms);
    }(g, order));
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(g.m.try_lock_shared());   // the writer waits: no new reader
    auto r2 = sgcl::async::spawn([](Guarded& g, std::vector<int>& order) -> sgcl::async::task<> {
        auto lock = co_await g.m.scoped_lock_shared();
        order.push_back(2);
        co_await sgcl::async::sleep(10ms);
    }(g, order));
    std::this_thread::sleep_for(20ms);
    auto w3 = sgcl::async::spawn([](Guarded& g, std::vector<int>& order) -> sgcl::async::task<> {
        auto lock = co_await g.m.scoped_lock();
        order.push_back(3);
    }(g, order));
    std::this_thread::sleep_for(20ms);
    EXPECT_TRUE(order.empty());            // all of them wait for this reader
    g.m.unlock_shared();
    w1.wait();
    r2.wait();
    w3.wait();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
    sgcl::async::scheduler::stop();
}

TEST(SharedMutex_Test, ThreadsAndTasksMixed) {
    Guarded g;
    std::atomic<long> reads = {0}, writes = {0};
    std::vector<sgcl::async::task<>> tasks;
    for (int i = 0; i < 8; ++i) {
        tasks.push_back(sgcl::async::spawn([](Guarded& g, std::atomic<long>& reads) -> sgcl::async::task<> {
            for (int k = 0; k < 2000; ++k) {
                auto lock = co_await g.m.scoped_lock_shared();
                g.reader_in();
                ++reads;
                g.reader_out();
            }
        }(g, reads)));
    }
    for (int i = 0; i < 2; ++i) {
        tasks.push_back(sgcl::async::spawn([](Guarded& g, std::atomic<long>& writes) -> sgcl::async::task<> {
            for (int k = 0; k < 500; ++k) {
                (void)(co_await g.m.scoped_lock()).release();
                g.writer_in();
                ++writes;
                g.writer_out();
                g.m.unlock();
                if (k % 50 == 0) {
                    co_await sgcl::async::yield();
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
        t.wait();
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
    sgcl::async::scheduler::stop();
}
