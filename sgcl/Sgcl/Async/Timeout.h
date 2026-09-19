//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A timeout on a task: `co_await Timeout(t, d)` is the task's result as
// an Optional, None when d passed first (true or false for a Task<>);
// `co_await WithDeadline(t, d)` the result itself or the exception
// TimedOut; WithDeadline(t, token) the same with a token's stop as the
// deadline. A loser runs on unseen unless it was made with the token of
// a StopSource given as the third argument, which the timeout stops.
#pragma once

#include "../../async/timeout.h"
#include "../Core/Types.h"
#include "StopToken.h"
#include "Coroutine.h"
#include "Scheduler.h"
#include "Time.h"

#include <utility>

namespace Sgcl {
    // Thrown by WithDeadline when the deadline passed first
    using TimedOut = sgcl::timed_out;

    // `co_await Timeout(t, d)`: the result of t, or None when d passed
    // first (true or false for a Task<>); the loser runs on
    // (The wrappers here return the inner task as a Task: a coroutine over
    // it would add a frame and a suspension per call, measured at about
    // a quarter of the cost of a WhenAll of one task.)
    template<class T>
    Task<typename sgcl::detail::TimeoutResult<T>::type> Timeout(Task<T> t, Duration d) {
        return sgcl::timeout(std::move(t.Inner()), d);
    }

    // The same, with the stop of the source requested when d passed
    // first: for a task made with the source's token, which stops itself
    template<class T>
    Task<typename sgcl::detail::TimeoutResult<T>::type> Timeout(Task<T> t, Duration d, StopSource loser) {
        return sgcl::timeout(std::move(t.Inner()), d, std::move(loser.Inner()));
    }

    // `co_await WithDeadline(t, d)`: the result of t, or TimedOut thrown
    // when d passed first
    template<class T>
    Task<T> WithDeadline(Task<T> t, Duration d) {
        return sgcl::with_deadline(std::move(t.Inner()), d);
    }

    template<class T>
    Task<T> WithDeadline(Task<T> t, Duration d, StopSource loser) {
        return sgcl::with_deadline(std::move(t.Inner()), d, std::move(loser.Inner()));
    }

    // `co_await WithDeadline(t, token)`: the result of t, or TimedOut
    // thrown when the token was stopped first
    template<class T>
    Task<T> WithDeadline(Task<T> t, StopToken token) {
        return sgcl::with_deadline(std::move(t.Inner()), std::move(token.Inner()));
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
