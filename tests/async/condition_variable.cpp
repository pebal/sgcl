//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;
}

TEST(ConditionVariable_Test, AWaitWithAPredicate) {
    sgcl::async::mutex m;
    sgcl::async::condition_variable cv;
    bool ready = false;                    // guarded by m
    auto waiter = sgcl::async::spawn([](sgcl::async::mutex& m, sgcl::async::condition_variable& cv, bool& ready) -> sgcl::async::task<int> {
        auto guard = co_await m.scoped_lock();
        co_await cv.wait(guard, [&] { return ready; });
        co_return ready ? 1 : 0;           // the mutex is held again here
    }(m, cv, ready));
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(waiter.done());
    {
        std::lock_guard lock(m);
        ready = true;
    }
    cv.notify_one();
    EXPECT_EQ(waiter.wait(), 1);
    // a thread's wait, with std::unique_lock
    ready = false;
    std::thread th([&] {
        std::unique_lock lock(m);
        cv.wait(lock, [&] { return ready; });
    });
    std::this_thread::sleep_for(10ms);
    {
        std::lock_guard lock(m);
        ready = true;
    }
    cv.notify_all();
    th.join();
    sgcl::async::scheduler::stop();
}

TEST(ConditionVariable_Test, NotifyOneWakesExactlyOne) {
    sgcl::async::mutex m;
    sgcl::async::condition_variable cv;
    std::atomic<int> waiting = {0}, woken = {0};
    std::vector<sgcl::async::task<>> tasks;
    for (int i = 0; i < 5; ++i) {
        tasks.push_back(sgcl::async::spawn([](sgcl::async::mutex& m, sgcl::async::condition_variable& cv, std::atomic<int>& waiting, std::atomic<int>& woken) -> sgcl::async::task<> {
            auto guard = co_await m.scoped_lock();
            ++waiting;
            co_await cv.wait(guard);
            ++woken;
        }(m, cv, waiting, woken)));
    }
    while (waiting.load() < 5) {
        std::this_thread::sleep_for(1ms);
    }
    std::this_thread::sleep_for(10ms);     // every one of the five is on the queue
    cv.notify_one();
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(woken, 1);
    cv.notify_one();
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(woken, 2);
    cv.notify_all();
    for (auto& t : tasks) {
        t.wait();
    }
    EXPECT_EQ(woken, 5);
    cv.notify_one();                       // nobody waits: nothing happens
    cv.notify_all();
    sgcl::async::scheduler::stop();
}

TEST(ConditionVariable_Test, ANotifyBeforeTheWaitIsLostSoThePredicateIsChecked) {
    sgcl::async::mutex m;
    sgcl::async::condition_variable cv;
    bool ready = false;                    // guarded by m
    // the notify comes first: the predicate, checked under the mutex
    // before the wait, is what saves the waiter
    {
        std::lock_guard lock(m);
        ready = true;
    }
    cv.notify_all();
    auto t = sgcl::async::spawn([](sgcl::async::mutex& m, sgcl::async::condition_variable& cv, bool& ready) -> sgcl::async::task<int> {
        auto guard = co_await m.scoped_lock();
        co_await cv.wait(guard, [&] { return ready; });   // true at once: no wait
        co_return 1;
    }(m, cv, ready));
    EXPECT_EQ(t.wait(), 1);
    // and a wait that began under the mutex before the notify is not lost,
    // however the notifier races it: the waiter is on the queue before the
    // mutex is let go of
    ready = false;
    std::atomic<int> woke = {0};
    for (int round = 0; round < 50; ++round) {
        auto w = sgcl::async::spawn([](sgcl::async::mutex& m, sgcl::async::condition_variable& cv, bool& ready, std::atomic<int>& woke) -> sgcl::async::task<> {
            auto guard = co_await m.scoped_lock();
            while (!ready) {
                co_await cv.wait(guard);
            }
            ready = false;
            ++woke;
        }(m, cv, ready, woke));
        {
            std::lock_guard lock(m);       // either before the waiter's look at `ready` or after its registration
            ready = true;
        }
        cv.notify_one();
        w.wait();
    }
    EXPECT_EQ(woke, 50);
    sgcl::async::scheduler::stop();
}

TEST(ConditionVariable_Test, ProducersAndConsumers) {
    sgcl::async::mutex m;
    sgcl::async::condition_variable not_empty;
    std::deque<int> queue;                 // guarded by m
    bool done = false;                     // guarded by m
    std::atomic<long> consumed = {0}, sum = {0};
    std::vector<sgcl::async::task<>> consumers;
    for (int i = 0; i < 4; ++i) {
        consumers.push_back(sgcl::async::spawn([](sgcl::async::mutex& m, sgcl::async::condition_variable& cv, std::deque<int>& queue, bool& done, std::atomic<long>& consumed, std::atomic<long>& sum) -> sgcl::async::task<> {
            for (;;) {
                auto guard = co_await m.scoped_lock();
                co_await cv.wait(guard, [&] { return !queue.empty() || done; });
                if (queue.empty()) {
                    co_return;
                }
                sum += queue.front();
                queue.pop_front();
                ++consumed;
            }
        }(m, not_empty, queue, done, consumed, sum)));
    }
    std::vector<std::thread> producers;
    for (int p = 0; p < 4; ++p) {
        producers.emplace_back([&, p] {
            for (int k = 0; k < 1000; ++k) {
                {
                    std::lock_guard lock(m);
                    queue.push_back(p * 1000 + k);
                }
                not_empty.notify_one();
            }
        });
    }
    for (auto& t : producers) {
        t.join();
    }
    {
        std::lock_guard lock(m);
        done = true;
    }
    not_empty.notify_all();
    for (auto& t : consumers) {
        t.wait();
    }
    EXPECT_EQ(consumed, 4000);
    EXPECT_EQ(sum, 4L * (0 + 999) * 1000 / 2 + 1000L * (0 + 1 + 2 + 3) * 1000);
    EXPECT_TRUE(queue.empty());
    sgcl::async::scheduler::stop();
}
