//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace {
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;
}

TEST(Timer_Test, SleepSuspendsATask) {
    auto t0 = Clock::now();
    auto t = sgcl::spawn([]() -> sgcl::task<int> {
        co_await sgcl::sleep(30ms);
        co_return 1;
    }());
    EXPECT_EQ(t.join(), 1);
    auto elapsed = Clock::now() - t0;
    EXPECT_GE(elapsed, 30ms);
    EXPECT_LT(elapsed, 2s);
    // a sleep of nothing does not suspend
    auto u = sgcl::spawn([]() -> sgcl::task<int> {
        co_await sgcl::sleep(0ms);
        co_return 2;
    }());
    EXPECT_EQ(u.join(), 2);
    sgcl::scheduler::stop();
}

TEST(Timer_Test, ManySleepersInOrder) {
    std::atomic<int> order = {0};
    std::vector<sgcl::task<int>> tasks;
    for (int i = 4; i >= 0; --i) {
        tasks.push_back(sgcl::spawn([](int i, std::atomic<int>& order) -> sgcl::task<int> {
            co_await sgcl::sleep(std::chrono::milliseconds(10 + 10 * i));
            co_return order++;
        }(i, order)));
    }
    for (int i = 4, k = 0; i >= 0; --i, ++k) {
        EXPECT_EQ(tasks[k].join(), i);   // the shortest sleep woke first
    }
    sgcl::scheduler::stop();
}

TEST(Timer_Test, AfterIsOneSignalThenClosed) {
    auto t0 = Clock::now();
    auto ch = sgcl::after(20ms);
    EXPECT_TRUE(ch->receive());
    EXPECT_GE(Clock::now() - t0, 20ms);
    EXPECT_FALSE(ch->receive());   // closed
    EXPECT_TRUE(ch->closed());
    sgcl::scheduler::stop();
}

TEST(Timer_Test, TimeoutInASelect) {
    sgcl::channel<int> never;
    bool timed = false;
    auto t0 = Clock::now();
    EXPECT_EQ(sgcl::select(never.on_receive([](int) {}), sgcl::timeout(20ms, [&] { timed = true; })), 1u);
    EXPECT_TRUE(timed);
    EXPECT_GE(Clock::now() - t0, 20ms);
    // the data wins when it comes first
    sgcl::channel<int> data(1);
    data.send(5);
    int got = 0;
    EXPECT_EQ(sgcl::select(data.on_receive([&](int v) { got = v; }), sgcl::timeout(30ms, [] {})), 0u);
    EXPECT_EQ(got, 5);
    // in a task
    auto t = sgcl::spawn([](sgcl::channel<int>& never) -> sgcl::task<size_t> {
        co_return co_await sgcl::async_select(never.on_receive([](int) {}), sgcl::timeout(10ms, [] {}));
    }(never));
    EXPECT_EQ(t.join(), 1u);
    std::this_thread::sleep_for(40ms);   // the timer of the timeout that lost fires and lets go of its channel
    sgcl::scheduler::stop();
}

TEST(Timer_Test, TickUntilClosed) {
    auto t0 = Clock::now();
    auto tk = sgcl::tick(5ms);
    int n = 0;
    while (n < 5) {
        if (tk->receive()) {
            ++n;
        }
    }
    EXPECT_GE(Clock::now() - t0, 25ms);
    tk->close();                       // the timer sees the close and lets go
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(tk->receive());
    sgcl::scheduler::stop();
    sgcl::collector::force_collect(true);
}

TEST(Timer_Test, TheTimersStopWithTheScheduler) {
    auto ch = sgcl::after(10ms);
    EXPECT_TRUE(ch->receive());
    sgcl::scheduler::stop();           // the timer thread joined
    auto again = sgcl::after(10ms);    // and started again by the next timer
    EXPECT_TRUE(again->receive());
    sgcl::scheduler::stop();
}

// A timeout case gone before its time cancels its timer, and the heap is
// swept of the cancelled: a select with a long timeout served by its
// channel in a loop keeps a bounded heap, not a timer per iteration
// until the deadline (0.75 KB each, measured, for an hour)
TEST(Timer_Test, ATimeoutCaseGoneCancelsItsTimer) {
    sgcl::channel<int> ch(1);
    auto t = sgcl::spawn([](sgcl::channel<int>& ch) -> sgcl::task<> {
        for (int i = 0; i < 5000; ++i) {
            co_await sgcl::async_select(ch.on_receive([](int) {}), sgcl::timeout(1h, [] {}));
        }
    }(ch));
    for (int i = 0; i < 5000; ++i) {
        ch.send(i);
    }
    t.join();
    EXPECT_LT(sgcl::detail::timers_instance().size(), 200u);   // swept once the cancelled are half the heap: a few dozen live at most
    sgcl::scheduler::stop();
}
