//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A span too long for the clock: now() + duration::max() saturates at
// time_point::max(), which the timers never fire, where the point used to
// overflow into the past and fire at once. Go's when() cuts the same way.
#include "tests/types.h"

using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <thread>

namespace {
    using namespace std::chrono_literals;
    using Steady = std::chrono::steady_clock;

    // 342 years, saturated to the largest duration
    duration forever() {
        return hour * 3'000'000;
    }
}

TEST(FarDeadline_Test, AfterAndTickNeverFire) {
    auto a = after(forever());
    auto b = after(24 * hour * 365 * 300);   // 300 years: fits in a duration, not after now() on the steady clock of a machine up for a while
    auto t = tick(forever());
    auto near = after(20ms);
    near->receive().wait();                     // a timer behind them still fires
    EXPECT_FALSE(a->try_receive());
    EXPECT_FALSE(b->try_receive());
    EXPECT_FALSE(t->try_receive());
    a->close();
    b->close();
    t->close();
    sgcl::async::scheduler::stop();
}

TEST(FarDeadline_Test, SleepAndTimeoutWaitForever) {
    auto t0 = Steady::now();
    bool timed_out = false;
    select(timeout(forever(), [&] { timed_out = true; }), after(20ms)->on_receive([] {})).wait();
    EXPECT_FALSE(timed_out);
    auto sleeper = spawn([]() -> task<bool> {
        co_return (co_await with_timeout(spawn([]() -> task<> { co_await sgcl::async::sleep(forever()); }()), 30ms)).has_value();
    }());
    EXPECT_FALSE(sleeper.wait());       // the sleep lost to 30 ms: it did not return at once
    auto guarded = spawn([]() -> task<bool> {
        co_return (co_await with_timeout(spawn([]() -> task<> { co_await sgcl::async::sleep(10ms); }()), forever())).has_value();
    }());
    EXPECT_TRUE(guarded.wait());        // a timeout of forever is never the winner
    EXPECT_GE(Steady::now() - t0, 50ms);
    sgcl::async::scheduler::stop();
}

TEST(FarDeadline_Test, StopAfterForeverNeverStops) {
    stop_source s;
    s.stop_after(forever());
    after(20ms)->receive().wait();
    EXPECT_FALSE(s.stop_requested());
    s.request_stop();
    sgcl::async::scheduler::stop();
}

TEST(FarDeadline_Test, TheManualClockGoesToTheEndAndNoFurther) {
    manual_clock clock;
    clock.install();
    auto t0 = sgcl::clock::now();
    auto a = after(forever());
    auto h = after(1h);
    clock.advance(forever());            // saturated: the end of time, not before the start
    EXPECT_EQ(sgcl::clock::now(), time_point::max());
    EXPECT_GT(sgcl::clock::now(), t0);
    EXPECT_TRUE(h->try_receive());       // everything before it fired
    EXPECT_FALSE(a->try_receive());      // a timer at the end never does
    clock.advance(forever());            // and no further
    EXPECT_EQ(sgcl::clock::now(), time_point::max());
    a->close();
}
