//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;
}

TEST(Sync_Test, AMutexBetweenTasksAndThreads) {
    sgcl::mutex m;
    int shared = 0;                                        // guarded by m
    std::vector<sgcl::task<>> tasks;
    for (int i = 0; i < 8; ++i) {
        tasks.push_back(sgcl::spawn([](sgcl::mutex& m, int& shared) -> sgcl::task<> {
            for (int k = 0; k < 1000; ++k) {
                auto guard = co_await m.async_scoped_lock();
                ++shared;
            }
        }(m, shared)));
    }
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&] {
            for (int k = 0; k < 1000; ++k) {
                std::lock_guard lock(m);                   // a Lockable
                ++shared;
            }
        });
    }
    for (auto& t : tasks) {
        t.join();
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(shared, 12000);
    EXPECT_TRUE(m.try_lock());
    EXPECT_FALSE(m.try_lock());
    m.unlock();
    // async_lock and unlock by hand; a select case
    auto t = sgcl::spawn([](sgcl::mutex& m) -> sgcl::task<int> {
        co_await m.async_lock();
        m.unlock();
        co_return co_await sgcl::async_select(m.on_lock([] {})) == 0 ? 1 : 0;
    }(m));
    EXPECT_EQ(t.join(), 1);
    m.unlock();
    sgcl::scheduler::stop();
}

TEST(Sync_Test, ASemaphoreBoundsTheConcurrency) {
    sgcl::semaphore sem(3);
    std::atomic<int> inside = {0}, peak = {0};
    std::vector<sgcl::task<>> tasks;
    for (int i = 0; i < 20; ++i) {
        tasks.push_back(sgcl::spawn([](sgcl::semaphore& sem, std::atomic<int>& inside, std::atomic<int>& peak) -> sgcl::task<> {
            co_await sem.async_acquire();
            int now = ++inside;
            int p = peak.load();
            while (now > p && !peak.compare_exchange_weak(p, now)) {
            }
            co_await sgcl::sleep(1ms);
            --inside;
            sem.release();
        }(sem, inside, peak)));
    }
    for (auto& t : tasks) {
        t.join();
    }
    EXPECT_LE(peak, 3);
    EXPECT_GE(peak, 1);
    EXPECT_EQ(sem.available(), 3u);
    EXPECT_TRUE(sem.try_acquire());
    sem.release();
    sgcl::scheduler::stop();
}

TEST(Sync_Test, AnEventReleasesEveryWaiter) {
    sgcl::event ev;
    EXPECT_FALSE(ev.is_set());
    std::vector<sgcl::task<int>> tasks;
    for (int i = 0; i < 10; ++i) {
        tasks.push_back(sgcl::spawn([](sgcl::event& ev, int i) -> sgcl::task<int> {
            co_await ev.async_wait();
            co_return i;
        }(ev, i)));
    }
    std::thread th([&] { ev.wait(); });
    std::this_thread::sleep_for(10ms);
    ev.set();
    int sum = 0;
    for (auto& t : tasks) {
        sum += t.join();
    }
    th.join();
    EXPECT_EQ(sum, 45);
    EXPECT_TRUE(ev.is_set());
    ev.wait();                                             // set already: no wait
    EXPECT_EQ(sgcl::select(ev.on_set([] {})), 0u);
    sgcl::scheduler::stop();
}

TEST(Sync_Test, AWaitGroupCountsTheWork) {
    sgcl::wait_group wg;
    std::atomic<int> done = {0};
    for (int i = 0; i < 50; ++i) {
        wg.add();
        sgcl::go([](sgcl::wait_group& wg, std::atomic<int>& done) -> sgcl::task<> {
            co_await sgcl::sleep(1ms);
            ++done;
            wg.done();
        }(wg, done));
    }
    EXPECT_LE(wg.count(), 50);   // some may be done already: under TSan the fifty go() calls outlast a 1 ms sleep
    wg.wait();
    EXPECT_EQ(done, 50);
    EXPECT_EQ(wg.count(), 0);
    // from a task, and as a case
    auto t = sgcl::spawn([](sgcl::wait_group& wg) -> sgcl::task<int> {
        co_await wg.async_wait();
        co_return co_await sgcl::async_select(wg.on_done([] {})) == 0 ? 1 : 0;
    }(wg));
    EXPECT_EQ(t.join(), 1);
    // a second round: add after zero, the tasks done at once, wait again
    std::atomic<int> quick = {0};
    for (int i = 0; i < 20; ++i) {
        wg.add();
        sgcl::go([](sgcl::wait_group& wg, std::atomic<int>& quick) -> sgcl::task<> {
            ++quick;
            wg.done();
            co_return;
        }(wg, quick));
    }
    wg.wait();
    EXPECT_EQ(quick, 20);
    sgcl::scheduler::stop();
}

TEST(Sync_Test, AOnceRunsOnce) {
    sgcl::once o;
    std::atomic<int> inits = {0};
    std::vector<sgcl::task<>> callers;
    for (int i = 0; i < 8; ++i) {
        callers.push_back(sgcl::spawn([](sgcl::once& o, std::atomic<int>& inits) -> sgcl::task<> {
            co_await o.async_call([](std::atomic<int>& inits) -> sgcl::task<> {
                co_await sgcl::sleep(5ms);
                ++inits;
            }(inits));
            EXPECT_EQ(inits, 1);                           // done before any caller goes on
        }(o, inits)));
    }
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&] { o.call([&] { ++inits; }); EXPECT_EQ(inits, 1); });
    }
    for (auto& c : callers) {
        c.join();
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(inits, 1);
    EXPECT_TRUE(o.called());
    sgcl::scheduler::stop();
}

TEST(Sync_Test, AWaitGroupsNegativeAddReleasesAndAClosedSemaphoreOpens) {
    sgcl::wait_group wg;
    wg.add(2);
    auto t = sgcl::spawn([](sgcl::wait_group& wg) -> sgcl::task<int> {
        co_await wg.async_wait();
        co_return 1;
    }(wg));
    std::this_thread::sleep_for(5ms);
    EXPECT_FALSE(t.done());
    wg.add(-2);                                              // Go's Add(-n): the count to zero releases the waiters
    EXPECT_EQ(t.join(), 1);
    sgcl::semaphore closed(0);                               // made closed: a release opens it (a rendezvous would lose the release)
    closed.release();
    EXPECT_TRUE(closed.try_acquire());
    sgcl::scheduler::stop();
}
