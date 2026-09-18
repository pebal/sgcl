//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Time: `co_await Sleep(d)` suspends a task for d, `co_await SleepUntil(t)`
// until a point; After(d) is a channel that gets one signal after d, then
// is closed, At(t) the same at a point; Tick(d) a channel that gets a
// signal every d until it is closed, its first tick alignable to a point;
// Timeout(d, f) a case of a Select, served after d. Clock::Now() is the
// module's time, the steady clock's unless a test has installed a
// ManualClock, whose Advance(d) fires every timer due with no real waiting.
#pragma once

#include "../../async/timer.h"
#include "../Core/Ptr.h"
#include "Channel.h"

#include <chrono>
#include <utility>

namespace Sgcl {
    using Duration = sgcl::duration;
    using TimePoint = sgcl::time_point;

    // `co_await Sleep(d)`: the task suspended for d, no thread held;
    // `Sleep(d).Wait()` blocks the thread, through the clock
    class Sleep : public sgcl::sleep {
    public:
        using sgcl::sleep::sleep;

        void Wait() const {
            wait();
        }
    };

    // `co_await SleepUntil(t)`: the task suspended until t; `SleepUntil(t).Wait()` blocks the thread
    class SleepUntil : public sgcl::sleep_until {
    public:
        using sgcl::sleep_until::sleep_until;

        void Wait() const {
            wait();
        }
    };

    // The clock of the module: the steady clock's time, or the manual clock's while one is installed
    struct Clock {
        using Rep = sgcl::clock::rep;
        using Period = sgcl::clock::period;
        using DurationType = Duration;
        using TimePointType = TimePoint;
        static constexpr bool IsSteady = sgcl::clock::is_steady;

        static TimePoint Now() noexcept {
            return sgcl::clock::now();
        }
    };

    // The clock of a test: installed, the module's time moves only by
    // Advance(d), which fires every timer due by then and returns when
    // the tasks woken have reached their next waits
    class ManualClock {
    public:
        using InnerType = sgcl::manual_clock;

        ManualClock() = default;
        ManualClock(const ManualClock&) = delete;
        ManualClock& operator=(const ManualClock&) = delete;

        void Install() {
            _c.install();
        }

        void Uninstall() {
            _c.uninstall();
        }

        bool IsInstalled() const noexcept {
            return _c.installed();
        }

        TimePoint Now() const noexcept {
            return _c.now();
        }

        void Advance(Duration d) {
            _c.advance(d);
        }

        void AdvanceTo(TimePoint t) {
            _c.advance_to(t);
        }

        InnerType& Inner() noexcept {
            return _c;
        }

        const InnerType& Inner() const noexcept {
            return _c;
        }

    private:
        InnerType _c;
    };

    // A channel that gets one signal after d and is closed then
    inline Ptr<Channel<void>> After(Duration d) {
        Ptr<Channel<void>> ch = Make<Channel<void>>(1);
        sgcl::detail::add_timer(d, Duration::zero(), ch.Inner(), &ch->Inner());
        return ch;
    }

    // A channel that gets one signal at t (at once, for a t that has passed) and is closed then
    inline Ptr<Channel<void>> At(TimePoint t) {
        Ptr<Channel<void>> ch = Make<Channel<void>>(1);
        sgcl::detail::add_timer(t, Duration::zero(), ch.Inner(), &ch->Inner());
        return ch;
    }

    // A channel that gets a signal every d until it is closed
    inline Ptr<Channel<void>> Tick(Duration d) {
        Ptr<Channel<void>> ch = Make<Channel<void>>(1);
        sgcl::detail::add_timer(d, d, ch.Inner(), &ch->Inner());
        return ch;
    }

    // The same with the first tick at `first`, then every d
    inline Ptr<Channel<void>> Tick(Duration d, TimePoint first) {
        Ptr<Channel<void>> ch = Make<Channel<void>>(1);
        sgcl::detail::add_timer(first, d, ch.Inner(), &ch->Inner());
        return ch;
    }

    // A case of a Select served after d: `Timeout(1s, [&] { ... })`
    template<class F>
    class TimeoutCase : public decltype(std::declval<Channel<void>&>().OnReceive(std::declval<F>())) {
        using Base = decltype(std::declval<Channel<void>&>().OnReceive(std::declval<F>()));
    public:
        TimeoutCase(Ptr<Channel<void>> ch, F f)
        : Base(ch->OnReceive(std::move(f)))
        , _keep(std::move(ch)) {
        }

        TimeoutCase(TimeoutCase&& o) noexcept
        : Base(std::move(o))
        , _keep(std::move(o._keep)) {
            o._keep = nullptr;
        }

        TimeoutCase(const TimeoutCase&) = delete;

        // The case gone before its time: the timer cancelled (sgcl::timeout_case)
        ~TimeoutCase() {
            if (_keep && !_keep->IsClosed()) {
                _keep->Close();
                sgcl::detail::timer_cancelled();
            }
        }

    private:
        Ptr<Channel<void>> _keep;   // the channel lives while the case does
    };

    template<class F>
    auto Timeout(Duration d, F f) {
        return TimeoutCase<F>(After(d), std::move(f));
    }

    // The same at a point: `Timeout(deadline, f)`
    template<class F>
    auto Timeout(TimePoint t, F f) {
        return TimeoutCase<F>(At(t), std::move(f));
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
