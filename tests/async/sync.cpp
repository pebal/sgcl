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
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;
}

TEST(Sync_Test, AMutexBetweenTasksAndThreads) {
    sgcl::async::mutex m;
    int shared = 0;                                        // guarded by m
    std::vector<sgcl::async::task<>> tasks;
    for (int i = 0; i < 8; ++i) {
        tasks.push_back(sgcl::async::spawn([](sgcl::async::mutex& m, int& shared) -> sgcl::async::task<> {
            for (int k = 0; k < 1000; ++k) {
                auto guard = co_await m.scoped_lock();
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
        t.wait();
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(shared, 12000);
    EXPECT_TRUE(m.try_lock());
    EXPECT_FALSE(m.try_lock());
    m.unlock();
    // scoped_lock and unlock by hand; a select case
    auto t = sgcl::async::spawn([](sgcl::async::mutex& m) -> sgcl::async::task<int> {
        (void)(co_await m.scoped_lock()).release();
        m.unlock();
        co_return co_await sgcl::async::select(m.on_lock([] {})) == 0 ? 1 : 0;
    }(m));
    EXPECT_EQ(t.wait(), 1);
    m.unlock();
    sgcl::async::scheduler::stop();
}

TEST(Sync_Test, ASemaphoreBoundsTheConcurrency) {
    sgcl::async::semaphore sem(3);
    std::atomic<int> inside = {0}, peak = {0};
    std::vector<sgcl::async::task<>> tasks;
    for (int i = 0; i < 20; ++i) {
        tasks.push_back(sgcl::async::spawn([](sgcl::async::semaphore& sem, std::atomic<int>& inside, std::atomic<int>& peak) -> sgcl::async::task<> {
            co_await sem.acquire();
            int now = ++inside;
            int p = peak.load();
            while (now > p && !peak.compare_exchange_weak(p, now)) {
            }
            co_await sgcl::async::sleep(1ms);
            --inside;
            sem.release();
        }(sem, inside, peak)));
    }
    for (auto& t : tasks) {
        t.wait();
    }
    EXPECT_LE(peak, 3);
    EXPECT_GE(peak, 1);
    EXPECT_EQ(sem.available(), 3u);
    EXPECT_TRUE(sem.try_acquire());
    sem.release();
    sgcl::async::scheduler::stop();
}

TEST(Sync_Test, AnEventReleasesEveryWaiter) {
    sgcl::async::event ev;
    EXPECT_FALSE(ev.is_set());
    std::vector<sgcl::async::task<int>> tasks;
    for (int i = 0; i < 10; ++i) {
        tasks.push_back(sgcl::async::spawn([](sgcl::async::event& ev, int i) -> sgcl::async::task<int> {
            co_await ev;
            co_return i;
        }(ev, i)));
    }
    std::thread th([&] { ev.wait(); });
    std::this_thread::sleep_for(10ms);
    ev.set();
    int sum = 0;
    for (auto& t : tasks) {
        sum += t.wait();
    }
    th.join();
    EXPECT_EQ(sum, 45);
    EXPECT_TRUE(ev.is_set());
    ev.wait();                                             // set already: no wait
    EXPECT_EQ(sgcl::async::select(ev.on_set([] {})).wait(), 0u);
    sgcl::async::scheduler::stop();
}

TEST(Sync_Test, AWaitGroupCountsTheWork) {
    sgcl::async::wait_group wg;
    std::atomic<int> done = {0};
    for (int i = 0; i < 50; ++i) {
        wg.add();
        sgcl::async::go([](sgcl::async::wait_group& wg, std::atomic<int>& done) -> sgcl::async::task<> {
            co_await sgcl::async::sleep(1ms);
            ++done;
            wg.done();
        }(wg, done));
    }
    EXPECT_LE(wg.count(), 50);   // some may be done already: under TSan the fifty go() calls outlast a 1 ms sleep
    wg.wait();
    EXPECT_EQ(done, 50);
    EXPECT_EQ(wg.count(), 0);
    // from a task, and as a case
    auto t = sgcl::async::spawn([](sgcl::async::wait_group& wg) -> sgcl::async::task<int> {
        co_await wg;
        co_return co_await sgcl::async::select(wg.on_done([] {})) == 0 ? 1 : 0;
    }(wg));
    EXPECT_EQ(t.wait(), 1);
    // a second round: add after zero, the tasks done at once, wait again
    std::atomic<int> quick = {0};
    for (int i = 0; i < 20; ++i) {
        wg.add();
        sgcl::async::go([](sgcl::async::wait_group& wg, std::atomic<int>& quick) -> sgcl::async::task<> {
            ++quick;
            wg.done();
            co_return;
        }(wg, quick));
    }
    wg.wait();
    EXPECT_EQ(quick, 20);
    sgcl::async::scheduler::stop();
}

TEST(Sync_Test, AOnceRunsOnce) {
    sgcl::async::once o;
    std::atomic<int> inits = {0};
    std::vector<sgcl::async::task<>> callers;
    for (int i = 0; i < 8; ++i) {
        callers.push_back(sgcl::async::spawn([](sgcl::async::once& o, std::atomic<int>& inits) -> sgcl::async::task<> {
            co_await o.call([](std::atomic<int>& inits) -> sgcl::async::task<> {
                co_await sgcl::async::sleep(5ms);
                ++inits;
            }(inits));
            EXPECT_EQ(inits, 1);                           // done before any caller goes on
        }(o, inits)));
    }
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&] { o.call([&] { ++inits; }).wait(); EXPECT_EQ(inits, 1); });
    }
    for (auto& c : callers) {
        c.wait();
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(inits, 1);
    EXPECT_TRUE(o.called());
    sgcl::async::scheduler::stop();
}

// call(f) is an operation: the tasks co_await it, the first runs f and
// the others wait for it holding no worker; a thread waits with .wait()
TEST(Sync_Test, AOnceOfAFunctionIsAnOperation) {
    sgcl::async::once o;
    std::atomic<int> inits = {0};
    std::vector<sgcl::async::task<>> callers;
    for (int i = 0; i < 64; ++i) {                         // more callers than workers
        callers.push_back(sgcl::async::spawn([](sgcl::async::once& o, std::atomic<int>& inits) -> sgcl::async::task<> {
            co_await o.call([&inits] {
                std::this_thread::sleep_for(5ms);
                ++inits;
            });
            EXPECT_EQ(inits, 1);                           // done before any caller goes on
        }(o, inits)));
    }
    o.call([&] { ++inits; }).wait();                       // a thread: the call made already, or its wait
    for (auto& c : callers) {
        c.wait();
    }
    EXPECT_EQ(inits, 1);
    sgcl::async::scheduler::stop();
}

TEST(Sync_Test, AWaitGroupsNegativeAddReleasesAndAClosedSemaphoreOpens) {
    sgcl::async::wait_group wg;
    wg.add(2);
    auto t = sgcl::async::spawn([](sgcl::async::wait_group& wg) -> sgcl::async::task<int> {
        co_await wg;
        co_return 1;
    }(wg));
    std::this_thread::sleep_for(5ms);
    EXPECT_FALSE(t.done());
    wg.add(-2);                                              // Go's Add(-n): the count to zero releases the waiters
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::semaphore closed(0);                               // made closed: a release opens it (a rendezvous would lose the release)
    closed.release();
    EXPECT_TRUE(closed.try_acquire());
    sgcl::async::scheduler::stop();
}

// A call that throws is made all the same, as a promise set with an
// exception: the first caller gets the exception, every later caller gets
// it again, and nobody waits for good
TEST(Sync_Test, AOnceThatThrowsIsDoneAndGivesItsException) {
    sgcl::async::once o;
    int runs = 0;
    EXPECT_THROW(o.call([&] { ++runs; throw std::runtime_error("init"); }).wait(), std::runtime_error);
    EXPECT_TRUE(o.called());
    EXPECT_THROW(o.call([&] { ++runs; }).wait(), std::runtime_error);   // not run again, the first one's exception
    auto t = sgcl::async::spawn([](sgcl::async::once& o) -> sgcl::async::task<> {
        co_await o.call([] {});
    }(o));
    EXPECT_THROW(t.wait(), std::runtime_error);
    EXPECT_EQ(runs, 1);

    // a coroutine function with captures is run as the task it gives
    sgcl::async::once lazy;
    std::atomic<int> ran = {0};
    lazy.call([&ran]() -> sgcl::async::task<> { ++ran; co_return; }).wait();
    lazy.call([&ran]() -> sgcl::async::task<> { ++ran; co_return; }).wait();
    EXPECT_EQ(ran.load(), 1);
    sgcl::async::scheduler::stop();
}
