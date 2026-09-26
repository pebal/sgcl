//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// spawn_blocking: a blocking call on a pool apart from the workers, its
// result back through a promise; the pool grows, idles out, stops and
// restarts.
#include "tests/types.h"

using namespace sgcl::async;

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    // Blocking sleeps on the pool, all at once, awaited one after another
    sgcl::async::task<int> sleeps(int count, std::chrono::milliseconds each) {
        std::vector<sgcl::async::blocking_task<int>> jobs;
        for (int i = 0; i < count; ++i) {
            jobs.push_back(sgcl::async::spawn_blocking([=] {
                std::this_thread::sleep_for(each);
                return i;
            }));
        }
        int sum = 0;
        for (auto& j : jobs) {
            sum += co_await j;
        }
        co_return sum;
    }

    // A task that makes progress on the workers while the sleeps block
    sgcl::async::task<> ticker(std::atomic<int>& ticks, std::atomic<bool>& stop) {
        while (!stop.load()) {
            co_await sgcl::async::sleep(2ms);
            ++ticks;
        }
    }

    void restore_idle_time() {
        sgcl::async::blocking_pool::set_idle_time(std::chrono::milliseconds(sgcl::config::blocking_idle_milliseconds));
    }
}

TEST(Blocking_Tests, ReturnsAValue) {
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        int r = co_await sgcl::async::spawn_blocking([] {
            EXPECT_FALSE(sgcl::async::scheduler::on_worker());   // on a thread of the pool, not a worker
            std::this_thread::sleep_for(5ms);
            return 21;
        });
        co_return r * 2;
    }());
    EXPECT_EQ(t.wait(), 42);
    // the alias, and a move-only result
    auto u = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        auto p = co_await sgcl::async::blocking([] { return std::make_unique<int>(9); });
        co_return *p;
    }());
    EXPECT_EQ(u.wait(), 9);
    sgcl::async::scheduler::stop();
    EXPECT_EQ(sgcl::async::blocking_pool::get_statistics().threads, 0u);   // stopped with the scheduler
}

TEST(Blocking_Tests, ReturnsNothing) {
    std::atomic<bool> ran = {false};
    auto t = sgcl::async::spawn([](std::atomic<bool>& ran) -> sgcl::async::task<int> {
        co_await sgcl::async::spawn_blocking([&] { ran = true; });
        co_return ran ? 1 : 0;
    }(ran));
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

TEST(Blocking_Tests, ThrowsThrough) {
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<std::string> {
        try {
            co_await sgcl::async::spawn_blocking([]() -> int { throw std::runtime_error("from the pool"); });
        } catch (const std::runtime_error& e) {
            co_return e.what();
        }
        co_return "nothing";
    }());
    EXPECT_EQ(t.wait(), "from the pool");
    // from a thread, through join()
    EXPECT_THROW(sgcl::async::spawn_blocking([] { throw std::logic_error("no"); }).wait(), std::logic_error);
    EXPECT_EQ(sgcl::async::spawn_blocking([] { return 5; }).result(), 5);
    // result() of a job that ran is its value, as a task's; on_done its select case
    auto job = sgcl::async::spawn_blocking([] { return 6; });
    EXPECT_EQ(sgcl::async::select(job.on_done([] {})).wait(), 0u);
    EXPECT_TRUE(job.done());
    EXPECT_EQ(job.result(), 6);
    auto thrower = sgcl::async::spawn_blocking([] { throw std::logic_error("again"); });
    (void)sgcl::async::select(thrower.on_done([] {})).wait();
    EXPECT_THROW(thrower.result(), std::logic_error);
    sgcl::async::scheduler::stop();
}

TEST(Blocking_Tests, TheClosureMayCaptureATrackedPtr) {
    settle();
    const int before = Node::alive.load();
    sgcl::async::blocking_task<int> job;
    off_frame([&] {
        sgcl::tracked_ptr node = sgcl::make_tracked<Node>(8);
        job = sgcl::async::spawn_blocking([node] {                 // the closure lives in the job, a managed object: the node is traced through it
            std::this_thread::sleep_for(50ms);
            return node->value;
        });
    });
    settle();                                                // while the job runs: the node held by the closure alone
    EXPECT_EQ(Node::alive.load(), before + 1);
    off_frame([&] { EXPECT_EQ(job.result(), 8); });        // in a frame of its own: no word of the job left on this one
    job = sgcl::async::blocking_task<int>();
    sgcl::async::blocking_pool::stop();
    settle();
    EXPECT_EQ(Node::alive.load(), before);                   // the job gone, the closure and its node with it
}

TEST(Blocking_Tests, TheWorkersAreNotTaken) {
    std::atomic<int> ticks = {0};
    std::atomic<bool> stop = {false};
    auto tick = sgcl::async::spawn(ticker(ticks, stop));
    auto start = std::chrono::steady_clock::now();
    auto t = sgcl::async::spawn(sleeps(100, 50ms));                 // 100 blocking sleeps at once: more than the workers
    EXPECT_EQ(t.wait(), 99 * 100 / 2);
    auto took = std::chrono::steady_clock::now() - start;
    EXPECT_LT(took, 100 * 50ms / 4);                         // the sleeps ran in parallel, not one after another
    auto st = sgcl::async::blocking_pool::get_statistics();
    EXPECT_GE(st.threads, std::min(100u, sgcl::async::blocking_pool::max_threads()));   // grown to the jobs, or the cap; parked now
    stop = true;
    tick.wait();
    EXPECT_GE(ticks.load(), 5);                              // the ticker ran on the workers meanwhile
    sgcl::async::scheduler::stop();
}

TEST(Blocking_Tests, ThePoolGrowsAndIdlesOut) {
    sgcl::async::blocking_pool::set_idle_time(50ms);
    EXPECT_EQ(sgcl::async::blocking_pool::idle_time(), 50ms);
    std::vector<sgcl::async::blocking_task<int>> jobs;
    for (int i = 0; i < 8; ++i) {
        jobs.push_back(sgcl::async::spawn_blocking([i] {
            std::this_thread::sleep_for(20ms);
            return i;
        }));
    }
    auto st = sgcl::async::blocking_pool::get_statistics();
    EXPECT_EQ(st.threads, 8u);                               // one thread per job that found none idle
    EXPECT_EQ(st.idle, 0u);
    int sum = 0;
    for (auto& j : jobs) {
        sum += j.wait();
    }
    EXPECT_EQ(sum, 28);
    sgcl::async::blocking_pool::wait_idle();
    st = sgcl::async::blocking_pool::get_statistics();
    EXPECT_EQ(st.threads, 8u);                               // parked
    EXPECT_EQ(st.idle, 8u);
    EXPECT_EQ(st.queued, 0u);
    std::this_thread::sleep_for(200ms);                      // past the idle time
    st = sgcl::async::blocking_pool::get_statistics();
    EXPECT_EQ(st.threads, 0u);                               // gone
    // a job after the exit starts a thread again, and an idle thread is reused
    EXPECT_EQ(sgcl::async::spawn_blocking([] { return 1; }).wait(), 1);
    sgcl::async::blocking_pool::wait_idle();                        // the thread parked (a join returns at the set, before the park)
    EXPECT_EQ(sgcl::async::spawn_blocking([] { return 2; }).wait(), 2);
    st = sgcl::async::blocking_pool::get_statistics();
    EXPECT_EQ(st.threads, 1u);
    restore_idle_time();
    sgcl::async::blocking_pool::stop();
}

TEST(Blocking_Tests, StoppedAndRestarted) {
    std::atomic<int> ran = {0};
    for (int i = 0; i < 4; ++i) {
        sgcl::async::spawn_blocking([&] {
            std::this_thread::sleep_for(10ms);
            ++ran;
        });                                                  // the handle dropped: the job runs all the same
    }
    sgcl::async::blocking_pool::stop();                             // the jobs queued run to the end first
    EXPECT_EQ(ran.load(), 4);
    EXPECT_EQ(sgcl::async::blocking_pool::get_statistics().threads, 0u);
    sgcl::async::blocking_pool::stop();                             // twice is nothing
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        co_return co_await sgcl::async::spawn_blocking([] { return 3; });   // the pool started again
    }());
    EXPECT_EQ(t.wait(), 3);
    EXPECT_EQ(sgcl::async::blocking_pool::get_statistics().threads, 1u);
    sgcl::async::scheduler::stop();
    EXPECT_EQ(sgcl::async::blocking_pool::get_statistics().threads, 0u);
}

TEST(Blocking_Tests, ManyQuickJobs) {
    std::atomic<int> count = {0};
    std::vector<sgcl::async::task<>> tasks;
    for (int i = 0; i < 8; ++i) {
        tasks.push_back(sgcl::async::spawn([](std::atomic<int>& count) -> sgcl::async::task<> {
            for (int k = 0; k < 200; ++k) {
                co_await sgcl::async::spawn_blocking([&] { ++count; });
            }
        }(count)));
    }
    for (auto& t : tasks) {
        t.wait();
    }
    EXPECT_EQ(count.load(), 1600);
    EXPECT_LE(sgcl::async::blocking_pool::get_statistics().threads, sgcl::async::blocking_pool::max_threads());
    sgcl::async::scheduler::stop();
}
