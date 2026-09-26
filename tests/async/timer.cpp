//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <thread>

namespace {
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;
}

TEST(Timer_Test, SleepSuspendsATask) {
    auto t0 = Clock::now();
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        co_await sgcl::async::sleep(30ms);
        co_return 1;
    }());
    EXPECT_EQ(t.wait(), 1);
    auto elapsed = Clock::now() - t0;
    EXPECT_GE(elapsed, 30ms);
    EXPECT_LT(elapsed, 2s);
    // a sleep of nothing does not suspend
    auto u = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        co_await sgcl::async::sleep(0ms);
        co_return 2;
    }());
    EXPECT_EQ(u.wait(), 2);
    sgcl::async::scheduler::stop();
}

TEST(Timer_Test, ManySleepersInOrder) {
    std::atomic<int> order = {0};
    std::vector<sgcl::async::task<int>> tasks;
    for (int i = 4; i >= 0; --i) {
        tasks.push_back(sgcl::async::spawn([](int i, std::atomic<int>& order) -> sgcl::async::task<int> {
            co_await sgcl::async::sleep(std::chrono::milliseconds(10 + 10 * i));
            co_return order++;
        }(i, order)));
    }
    for (int i = 4, k = 0; i >= 0; --i, ++k) {
        EXPECT_EQ(tasks[k].wait(), i);   // the shortest sleep woke first
    }
    sgcl::async::scheduler::stop();
}

TEST(Timer_Test, AfterIsOneSignalThenClosed) {
    auto t0 = Clock::now();
    auto ch = sgcl::async::after(20ms);
    EXPECT_TRUE(ch->receive().wait());
    EXPECT_GE(Clock::now() - t0, 20ms);
    EXPECT_FALSE(ch->receive().wait());   // closed
    EXPECT_TRUE(ch->closed());
    sgcl::async::scheduler::stop();
}

TEST(Timer_Test, TimeoutInASelect) {
    sgcl::async::channel<int> never;
    bool timed = false;
    auto t0 = Clock::now();
    EXPECT_EQ(sgcl::async::select(never.on_receive([](int) {}), sgcl::async::timeout(20ms, [&] { timed = true; })).wait(), 1u);
    EXPECT_TRUE(timed);
    EXPECT_GE(Clock::now() - t0, 20ms);
    // the data wins when it comes first
    sgcl::async::channel<int> data(1);
    data.send(5).wait();
    int got = 0;
    EXPECT_EQ(sgcl::async::select(data.on_receive([&](int v) { got = v; }), sgcl::async::timeout(30ms, [] {})).wait(), 0u);
    EXPECT_EQ(got, 5);
    // in a task
    auto t = sgcl::async::spawn([](sgcl::async::channel<int>& never) -> sgcl::async::task<size_t> {
        co_return co_await sgcl::async::select(never.on_receive([](int) {}), sgcl::async::timeout(10ms, [] {}));
    }(never));
    EXPECT_EQ(t.wait(), 1u);
    std::this_thread::sleep_for(40ms);   // the timer of the timeout that lost fires and lets go of its channel
    sgcl::async::scheduler::stop();
}

TEST(Timer_Test, TickUntilClosed) {
    auto t0 = Clock::now();
    auto tk = sgcl::async::tick(5ms);
    int n = 0;
    while (n < 5) {
        if (tk->receive().wait()) {
            ++n;
        }
    }
    EXPECT_GE(Clock::now() - t0, 25ms);
    tk->close();                       // the timer sees the close and lets go
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(tk->receive().wait());
    sgcl::async::scheduler::stop();
    sgcl::collector::force_collect(true);
}

TEST(Timer_Test, TheTimersStopWithTheScheduler) {
    auto ch = sgcl::async::after(10ms);
    EXPECT_TRUE(ch->receive().wait());
    sgcl::async::scheduler::stop();           // the timer thread joined
    auto again = sgcl::async::after(10ms);    // and started again by the next timer
    EXPECT_TRUE(again->receive().wait());
    sgcl::async::scheduler::stop();
}

// A timeout case gone before its time cancels its timer, and the heap is
// swept of the cancelled: a select with a long timeout served by its
// channel in a loop keeps a bounded heap, not a timer per iteration
// until the deadline (0.75 KB each, measured, for an hour)
TEST(Timer_Test, ATimeoutCaseGoneCancelsItsTimer) {
    sgcl::async::channel<int> ch(1);
    auto t = sgcl::async::spawn([](sgcl::async::channel<int>& ch) -> sgcl::async::task<> {
        for (int i = 0; i < 5000; ++i) {
            co_await sgcl::async::select(ch.on_receive([](int) {}), sgcl::async::timeout(1h, [] {}));
        }
    }(ch));
    for (int i = 0; i < 5000; ++i) {
        ch.send(i).wait();
    }
    t.wait();
    EXPECT_LT(sgcl::async::detail::timers_instance().size(), 200u);   // swept once the cancelled are half the heap: a few dozen live at most
    sgcl::async::scheduler::stop();
}
