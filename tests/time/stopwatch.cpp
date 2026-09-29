//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// time::stopwatch: the time elapsed on the library's monotonic clock, so a
// test's manual clock moves it
#include "sgcl/time/time.h"
#include "tests/types.h"

using namespace sgcl::async;

#include <chrono>
#include <stdexcept>
#include <thread>

namespace {
    using namespace std::chrono_literals;
}

TEST(Stopwatch_Tests, FollowsTheManualClock) {
    manual_clock clock;
    clock.install();
    time::stopwatch sw;
    EXPECT_EQ(sw.elapsed(), duration());
    std::this_thread::sleep_for(2ms);
    EXPECT_EQ(sw.elapsed(), duration());     // the manual clock stands still
    clock.advance(1500ms);
    EXPECT_EQ(sw.elapsed(), 1500ms);
    EXPECT_EQ(sw.elapsed().to_string(), "1.5s");
    EXPECT_EQ(sw.restart(), 1500ms);
    EXPECT_EQ(sw.elapsed(), duration());
    clock.advance(2h);
    EXPECT_EQ(sw.elapsed(), 2 * hour);
}

// measure(f): the time f took, on the same clock; f's value dropped, its
// exception passed through
TEST(Stopwatch_Tests, MeasureOneCall) {
    manual_clock clock;
    clock.install();
    int calls = 0;
    EXPECT_EQ(time::stopwatch::measure([&] { clock.advance(250ms); ++calls; }), 250ms);
    EXPECT_EQ(time::stopwatch::measure([&] { ++calls; return 7; }), duration());   // a value, dropped
    auto named = [&] { clock.advance(1s); };
    EXPECT_EQ(time::stopwatch::measure(named), 1s);                                // an lvalue
    EXPECT_EQ(calls, 2);
    EXPECT_THROW(time::stopwatch::measure([] { throw std::runtime_error("f's error"); }), std::runtime_error);
    clock.uninstall();
    EXPECT_GE(time::stopwatch::measure([] { std::this_thread::sleep_for(2ms); }), 2ms);   // the steady clock
}

TEST(Stopwatch_Tests, MeasuresRealTimeOtherwise) {
    time::stopwatch sw;
    std::this_thread::sleep_for(5ms);
    duration first = sw.elapsed();
    EXPECT_GE(first, 5ms);
    EXPECT_GE(sw.elapsed(), first);          // monotonic
    duration lap = sw.restart();
    EXPECT_GE(lap, first);
    EXPECT_LT(sw.elapsed(), lap + 1s);
}
