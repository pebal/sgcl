//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// StopSource, StopToken: cancellation, the way Go's context has it. A
// source requests the stop, the tokens handed down see it: a token is a
// channel of signals closed by the stop, so a wait is cancelled as a case
// of a Select (`token.OnStop(f)`) or with `co_await token.Stopped()`. A
// deadline is a timer that requests the stop; a source made from a token
// is a child, stopped with its parent.
#pragma once

#include "../../async/stop_token.h"
#include "Channel.h"
#include "Time.h"

#include <utility>

namespace Sgcl {
    class StopSource;

    class StopToken {
    public:
        using InnerType = sgcl::stop_token;

        StopToken() noexcept = default;

        explicit StopToken(InnerType t) noexcept
        : _t(std::move(t)) {
        }

        bool IsStopRequested() const noexcept {
            return _t.stop_requested();
        }

        bool IsStopPossible() const noexcept {
            return _t.stop_possible();
        }

        // The case of a Select served by the stop: `token.OnStop([&] { running = false; })`
        template<class F>
        auto OnStop(F f) const {
            return _t.on_stop(std::move(f));
        }

        // `co_await token.Stopped()`: the task suspended until the stop
        auto Stopped() const noexcept {
            return _t.stopped();
        }

        friend bool operator==(const StopToken& a, const StopToken& b) noexcept {
            return a._t == b._t;
        }

        InnerType& Inner() noexcept {
            return _t;
        }

        const InnerType& Inner() const noexcept {
            return _t;
        }

    private:
        InnerType _t;
    };

    class StopSource {
    public:
        using InnerType = sgcl::stop_source;

        StopSource() = default;

        // A child of the source the token belongs to: stopped with it
        explicit StopSource(const StopToken& parent)
        : _s(parent.Inner()) {
        }

        explicit StopSource(InnerType s) noexcept
        : _s(std::move(s)) {
        }

        StopToken Token() const noexcept {
            return StopToken(_s.token());
        }

        bool IsStopRequested() const noexcept {
            return _s.stop_requested();
        }

        // The stop: the token's channel closed, every waiter woken, the children stopped
        void RequestStop() {
            _s.request_stop();
        }

        // The stop requested after d, by a timer (a deadline)
        void StopAfter(Duration d) {
            _s.stop_after(d);
        }

        InnerType& Inner() noexcept {
            return _s;
        }

        const InnerType& Inner() const noexcept {
            return _s;
        }

    private:
        InnerType _s;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

