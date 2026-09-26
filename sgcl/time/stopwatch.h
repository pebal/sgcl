//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/clock.h"
#include "../core/duration.h"

namespace sgcl::time {
    // The time elapsed since a start, on the monotonic clock of the
    // library (sgcl::clock): a change of the system's wall clock does not
    // move it, and a test's manual_clock does, so that code measuring
    // itself is tested with no real waiting. Go keeps such a reading
    // inside every time.Time; here it has a name of its own, and a date
    // and time stays a plain value.
    class stopwatch {
    public:
        // Started at once
        stopwatch() noexcept
        : _start(clock::now()) {
        }

        duration elapsed() const noexcept {
            return clock::now() - _start;
        }

        // What had elapsed, and a new start from now
        duration restart() noexcept {
            time_point now = clock::now();
            duration elapsed = now - _start;
            _start = now;
            return elapsed;
        }

    private:
        time_point _start;
    };
}
