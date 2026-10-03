//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of the scheduler and the blocking pool (DESIGN 408): a
// start and a stop over and over with tasks waiting through the stops,
// the settings at their limits (no workers asked, more than the most,
// a spin and an idle time of zero, below zero and at the end of time).
// Covered elsewhere: a stop twice and a start by the next spawn
// (Scheduler_Tests.StopAndStartAgain), the number of workers changed at
// run time (Scheduler_Tests.SetWorkersAtRunTime), the pool stopped and
// started again (Blocking_Tests.StoppedAndRestarted).
#include "tests/types.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    sgcl::async::task<int> relay(sgcl::async::channel<int> in) {
        auto v = co_await in.receive();
        co_return v ? *v + 1 : -1;
    }
}

// Forty rounds of a start and a stop, the stop made while tasks wait on
// a channel: they stay suspended through it, and the sends that wake
// them start the workers again
TEST(SchedulerBoundary_Tests, StartedAndStoppedOverAndOver) {
    for (int round = 0; round < 40; ++round) {
        sgcl::async::channel<int> in;
        std::vector<sgcl::async::task<int>> waiting;
        for (int i = 0; i < 4; ++i) {
            waiting.push_back(sgcl::async::spawn(relay(in)));
        }
        while (sgcl::async::scheduler::get_statistics().workers == 0 || !in.try_send(round)) {   // the first receiver to wait takes it
            std::this_thread::yield();
        }
        sgcl::async::detail::wait_for_idle_workers();         // the other three suspended on the receive, none queued
        sgcl::async::scheduler::stop();
        EXPECT_EQ(sgcl::async::scheduler::get_statistics().workers, 0u);
        if (round % 2) {
            in.close();                                       // a close from a thread, the workers stopped
        } else {
            for (int i = 0; i < 3; ++i) {
                ASSERT_TRUE(in.send(round).wait());           // sends from a thread, the workers stopped
            }
        }
        int sum = 0;
        for (auto& t : waiting) {
            sum += t.wait();
        }
        EXPECT_EQ(sum, round % 2 ? (round + 1) - 3 : 4 * (round + 1)) << "round " << round;
    }
    sgcl::async::scheduler::stop();
    sgcl::async::scheduler::stop();
    EXPECT_EQ(sgcl::async::scheduler::get_statistics().workers, 0u);
}

// The number of workers at its ends: none asked is one per core, more
// than the most is the most (64), one is one
TEST(SchedulerBoundary_Tests, TheNumberOfWorkersAtItsEnds) {
    const unsigned before = sgcl::async::scheduler::workers();
    const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
    sgcl::async::scheduler::set_workers(0);
    EXPECT_EQ(sgcl::async::scheduler::workers(), std::min(64u, cores));
    sgcl::async::scheduler::set_workers(1000);
    EXPECT_EQ(sgcl::async::scheduler::workers(), 64u);
    EXPECT_EQ(sgcl::async::scheduler::get_statistics().workers, 64u);
    sgcl::async::scheduler::set_workers(1);
    EXPECT_EQ(sgcl::async::scheduler::workers(), 1u);
    sgcl::async::channel<int> one(1);
    EXPECT_TRUE(one.try_send(1));
    EXPECT_EQ(sgcl::async::spawn(relay(one)).wait(), 2);
    sgcl::async::scheduler::set_workers(before);
    EXPECT_EQ(sgcl::async::scheduler::workers(), before);
    sgcl::async::scheduler::stop();
}

// The spin of an idle worker at its ends: below zero and zero are no
// spin, a part of a microsecond is cut to none, the end of time is the
// most the setting holds (2^32 - 1 microseconds); tasks run with each
TEST(SchedulerBoundary_Tests, TheSpinAtItsEnds) {
    const auto before = sgcl::async::scheduler::worker_spin();
    sgcl::async::channel<int> one(1);
    sgcl::async::scheduler::set_worker_spin(sgcl::duration::min());
    EXPECT_EQ(sgcl::async::scheduler::worker_spin(), sgcl::duration::zero());
    EXPECT_TRUE(one.try_send(1));
    EXPECT_EQ(sgcl::async::spawn(relay(one)).wait(), 2);
    sgcl::async::scheduler::set_worker_spin(-1s);
    EXPECT_EQ(sgcl::async::scheduler::worker_spin(), sgcl::duration::zero());
    sgcl::async::scheduler::set_worker_spin(sgcl::duration::zero());
    EXPECT_EQ(sgcl::async::scheduler::worker_spin(), sgcl::duration::zero());
    sgcl::async::scheduler::set_worker_spin(999ns);
    EXPECT_EQ(sgcl::async::scheduler::worker_spin(), sgcl::duration::zero());
    EXPECT_TRUE(one.try_send(2));
    EXPECT_EQ(sgcl::async::spawn(relay(one)).wait(), 3);
    sgcl::async::scheduler::set_worker_spin(sgcl::duration::max());
    EXPECT_EQ(sgcl::async::scheduler::worker_spin(), sgcl::duration(std::chrono::microseconds(0xFFFFFFFFll)));
    sgcl::async::scheduler::set_worker_spin(before);
    EXPECT_EQ(sgcl::async::scheduler::worker_spin(), before);
    sgcl::async::scheduler::stop();
}

// The blocking pool at its ends: no threads asked is the default, an idle
// time of zero or below zero lets a thread go as soon as the queue is
// empty and the jobs still run, the end of time keeps it
TEST(SchedulerBoundary_Tests, TheBlockingPoolAtItsEnds) {
    const unsigned threads = sgcl::async::blocking_pool::max_threads();
    const auto idle = sgcl::async::blocking_pool::idle_time();
    const unsigned cores = std::thread::hardware_concurrency();
    sgcl::async::blocking_pool::set_threads(0);
    EXPECT_EQ(sgcl::async::blocking_pool::max_threads(), std::max(64u, 4 * cores));
    for (sgcl::duration d : {sgcl::duration::zero(), sgcl::duration(-1s), sgcl::duration::min()}) {
        sgcl::async::blocking_pool::set_idle_time(d);
        EXPECT_EQ(sgcl::async::blocking_pool::idle_time(), d);
        for (int i = 0; i < 3; ++i) {
            EXPECT_EQ(sgcl::async::spawn_blocking([i] { return i; }).wait(), i);
        }
        auto end = std::chrono::steady_clock::now() + 5s;
        while (sgcl::async::blocking_pool::get_statistics().threads != 0 && std::chrono::steady_clock::now() < end) {
            std::this_thread::sleep_for(1ms);                 // the thread gone at once
        }
        EXPECT_EQ(sgcl::async::blocking_pool::get_statistics().threads, 0u);
    }
    sgcl::async::blocking_pool::set_idle_time(sgcl::duration::max());
    EXPECT_EQ(sgcl::async::spawn_blocking([] { return 7; }).wait(), 7);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(sgcl::async::blocking_pool::get_statistics().threads, 1u);   // kept, idle
    sgcl::async::blocking_pool::stop();                       // a thread waiting for the end of time is stopped all the same
    EXPECT_EQ(sgcl::async::blocking_pool::get_statistics().threads, 0u);
    sgcl::async::blocking_pool::set_idle_time(idle);
    sgcl::async::blocking_pool::set_threads(threads);
    sgcl::async::scheduler::stop();
}
