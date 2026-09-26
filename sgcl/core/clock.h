//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "duration.h"

#include <atomic>
#include <chrono>

namespace sgcl {
    // The library reads the time in one place, `clock::now()`: the steady
    // clock, unless a test has installed a manual_clock (async/timer.h),
    // whose time moves only when the test advances it, every timer due
    // firing then, in order, with no real waiting (tokio's time::pause and
    // advance, Kotlin's runTest): a test of a thirty-second timeout takes
    // microseconds. The production path pays one relaxed load of the
    // flag per read and nothing else.
    //
    // A span of time is sgcl::duration (duration.h), which every
    // integral std::chrono::duration converts into; a point is the steady
    // clock's, a std::chrono::time_point, since a Clock asks for one.
    using time_point = std::chrono::steady_clock::time_point;

    namespace detail {
        // The manual clock's state, static so that a read is a load of
        // the flag and, when it is set, of the time: the flag relaxed
        // (the reads that matter, the timer thread's, happen under the
        // timers' lock the advance takes after storing the time), the
        // time with the release/acquire pair of advance and the reads
        inline std::atomic<bool> manual_clock_installed = {false};
        inline std::atomic<time_point::rep> manual_clock_now = {0};

        // Where the manual clock started, on the steady clock and on the
        // system's wall clock, so that the wall time moves with it: what
        // time::now() reads while one is installed is the wall time of
        // the install plus how far the manual time has been advanced.
        // Written by install before the flag, read after it
        inline std::atomic<time_point::rep> manual_clock_origin = {0};
        inline std::atomic<int64_t> manual_clock_wall_origin = {0};   // nanoseconds since 1970
    }

    // The clock of the library: a Clock of the standard library, the
    // steady clock's time, or the manual clock's while one is installed
    struct clock {
        using rep = time_point::rep;
        using period = time_point::period;
        using duration = time_point::duration;   // a Clock's duration is a std::chrono::duration; sgcl::duration converts to and from it
        using time_point = sgcl::time_point;
        static constexpr bool is_steady = true;

        static time_point now() noexcept {
            if (detail::manual_clock_installed.load(std::memory_order_relaxed)) [[unlikely]] {
                return time_point(duration(detail::manual_clock_now.load(std::memory_order_acquire)));
            }
            return std::chrono::steady_clock::now();
        }
    };
}
