//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of the module's time (DESIGN 408): a span of zero, a
// negative one and the most negative one, a point at the start of the
// clock, for every timer, deadline and stop. The far end (duration::max(),
// time_point::max()) is far_deadline.cpp's.
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace {
    using namespace std::chrono_literals;
    using Steady = std::chrono::steady_clock;

    template<class F>
    bool soon(F&& f) {
        auto until = Steady::now() + 5s;
        while (!f()) {
            if (Steady::now() > until) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    const sgcl::duration spans[] = {sgcl::duration::zero(), -1ms, -24h, sgcl::duration::min()};

    sgcl::async::task<int> slow(int v) {
        co_await sgcl::async::sleep(1h);
        co_return v;
    }
}

// A sleep of zero or less returns at once, a task's and a thread's; a
// point at the start of the clock is a point passed
TEST(TimeBoundary_Tests, ASleepOfZeroOrLessReturnsAtOnce) {
    auto t0 = Steady::now();
    for (auto d : spans) {
        sgcl::async::sleep(d).wait();
    }
    sgcl::async::sleep_until(sgcl::time_point::min()).wait();
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        int n = 0;
        for (auto d : spans) {
            co_await sgcl::async::sleep(d);
            ++n;
        }
        co_await sgcl::async::sleep_until(sgcl::time_point::min());
        co_return n + 1;
    }());
    EXPECT_EQ(t.wait(), 5);
    EXPECT_LT(Steady::now() - t0, 1s);
    sgcl::async::scheduler::stop();
}

// An after of zero or less and an at the start of the clock are set at
// once, by the timer thread: closed, one event
TEST(TimeBoundary_Tests, AnAfterOfZeroOrLessIsSetAtOnce) {
    for (auto d : spans) {
        auto e = sgcl::async::after(d);
        EXPECT_TRUE(soon([&] { return e.is_set(); }));
        e.wait();
    }
    auto e = sgcl::async::at(sgcl::time_point::min());
    EXPECT_TRUE(soon([&] { return e.is_set(); }));
    sgcl::async::scheduler::stop();
}

// A tick of a period of zero or less never ticks, as Go's time.Tick of
// such a period gives a nil channel: no signal, no close, and the timer
// thread free for the timers after it (a period of zero was a single
// close; a negative one was due again before the time it fired at, the
// timer thread firing it for good and the timers behind it never)
TEST(TimeBoundary_Tests, ATickOfZeroOrLessNeverTicks) {
    sgcl::async::channel<void> ticks[] = {
        sgcl::async::tick(sgcl::duration::zero()),
        sgcl::async::tick(-1ms),
        sgcl::async::tick(sgcl::duration::min()),
        sgcl::async::tick(sgcl::duration::zero(), sgcl::clock::now()),
        sgcl::async::tick(-1ms, sgcl::clock::now() - 1s),
    };
    auto later = sgcl::async::after(20ms);
    EXPECT_TRUE(soon([&] { return later.is_set(); }));        // a timer behind them still fires
    std::this_thread::sleep_for(10ms);
    for (auto& t : ticks) {
        EXPECT_FALSE(t.closed());
        EXPECT_FALSE(t.try_receive());
        t.close();
    }
    sgcl::async::scheduler::stop();
}

// A timeout case of zero or less is served at once when nothing else is
// ready, as an otherwise would be; a timeout of a task of zero or less
// gives timed_out for a task that is not done, the result for one done
TEST(TimeBoundary_Tests, ATimeoutOfZeroOrLess) {
    for (auto d : spans) {
        bool timed_out = false;
        sgcl::async::channel<int> nothing;
        EXPECT_EQ(sgcl::async::select(nothing.on_receive([](int) {}), sgcl::async::timeout(d, [&] { timed_out = true; })).wait(), 1u);
        EXPECT_TRUE(timed_out);
        auto lost = sgcl::async::with_timeout(slow(1), d).wait();
        ASSERT_FALSE(lost);
        EXPECT_EQ(lost.error(), sgcl::async::timed_out());
    }
    auto at_start = sgcl::async::with_deadline(slow(2), sgcl::time_point::min()).wait();
    EXPECT_FALSE(at_start);
    sgcl::async::scheduler::stop();
}

// A deadline of zero or less, or at the start of the clock, stops the
// source at once (by the timer thread); a later deadline beside it
// changes nothing
TEST(TimeBoundary_Tests, AStopAfterZeroOrLessStopsAtOnce) {
    for (auto d : spans) {
        sgcl::async::stop_source src;
        sgcl::async::stop_source child(src.token());
        src.stop_after(d);
        EXPECT_TRUE(soon([&] { return src.stop_requested(); }));
        EXPECT_TRUE(soon([&] { return child.stop_requested(); }));   // the children after the close (request_stop's page)
        src.stop_after(1h);                                   // after the stop: arms nothing
        src.stop_after(-1s);
        EXPECT_TRUE(src.stop_requested());
    }
    sgcl::async::stop_source at_start;
    at_start.stop_at(sgcl::time_point::min());
    EXPECT_TRUE(soon([&] { return at_start.stop_requested(); }));
    at_start.token().stopped().wait();                        // a wait after the stop returns at once
    sgcl::async::scheduler::stop();
}

// A manual clock moved by nothing, or to its own now, fires nothing that
// is not due and returns; a timer due at the clock's now fires without an
// advance, and an advance of zero returns once it has
TEST(TimeBoundary_Tests, AManualClockAdvancedByNothing) {
    sgcl::async::manual_clock clock;
    clock.install();
    auto start = clock.now();
    auto pending = sgcl::async::after(1s);
    clock.advance(sgcl::duration::zero());
    clock.advance_to(clock.now());
    EXPECT_EQ(clock.now(), start);
    EXPECT_FALSE(pending.is_set());
    auto due = sgcl::async::after(sgcl::duration::zero());
    clock.advance(sgcl::duration::zero());
    EXPECT_TRUE(due.is_set());
    clock.advance(1s);
    EXPECT_TRUE(pending.is_set());
    clock.uninstall();
    clock.uninstall();                                        // a second uninstall does nothing
    EXPECT_FALSE(clock.installed());
    sgcl::async::scheduler::stop();
}
