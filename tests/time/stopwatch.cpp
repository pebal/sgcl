//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// time::stopwatch: the time elapsed on the library's monotonic clock, so a
// test's manual clock moves it
#include "sgcl/time/time.h"
#include "tests/types.h"

using namespace sgcl::async;

#include <chrono>
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
