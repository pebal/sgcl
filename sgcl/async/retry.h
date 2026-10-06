//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/clock.h"
#include "../core/expected.h"
#include "coroutine.h"
#include "operation.h"
#include "select.h"
#include "stop_token.h"
#include "timer.h"

#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // An operation retried with exponential backoff and jitter: f is
    // called until it gives a value, the policy gives up, or a stop
    // comes. f returns an expected<T, E>, the library's way of failing (or
    // is a coroutine function whose task gives one); its error is retried,
    // and the last one returned when the retry gives up. A predicate given
    // takes the whole result instead and says whether to try again, so a
    // value is retried too (an HTTP response of 503), and f may return any
    // type. What f throws is not retried and comes out as it is. The waits are AWS's "Exponential
    // Backoff And Jitter": the n-th is drawn below
    // v = min(max_delay, initial * multiplier^(n-1)), uniformly in [0, v]
    // by default (full jitter), which spreads the callers a failure made
    // synchronous. This is the retry of an I/O operation, not the backoff
    // of a lock-free loop (core/detail/backoff.h).

    // How a wait is drawn from v, the exponential's value
    enum class jitter : uint8_t {
        none,           // v
        full,           // uniform in [0, v]
        equal,          // v/2 + uniform in [0, v/2]
        decorrelated    // min(max_delay, uniform in [initial, 3 * the previous wait])
    };

    // The policy: a plain value (a constant of the program may be one)
    struct retry_policy {
        size_t attempts = 5;                        // attempts in all, the first included; 0: no limit
        duration max_elapsed = duration::zero();    // no attempt starts later than this after the first; 0: no limit
        duration initial = std::chrono::milliseconds(100);   // the first wait, before the jitter
        duration max_delay = std::chrono::seconds(10);       // the longest wait
        double multiplier = 2;                      // the growth of the waits
        async::jitter jitter = jitter::full;
    };

    namespace detail {
        // A wait of ns nanoseconds, the cap itself (an exact count) when
        // ns reaches it: a cap near the end of time is 2^63 as a double,
        // which no int64_t holds (UBSan, the fuzz harness)
        SGCL_INLINE_HOT duration retry_capped(const retry_policy& p, double ns, double cap) noexcept {
            if (!(ns < cap)) {
                return p.max_delay > duration::zero() ? p.max_delay : duration::zero();
            }
            return duration(std::chrono::nanoseconds((int64_t)ns));
        }

        // The n-th wait of the policy (n from 1), given the previous one
        // (decorrelated jitter's), drawn with u uniform in [0, 1)
        inline duration retry_delay(const retry_policy& p, size_t n, duration previous, double u) noexcept {
            double cap = (double)p.max_delay.nanoseconds();
            double first = (double)p.initial.nanoseconds();
            if (cap < 0) {
                cap = 0;
            }
            if (first < 0) {
                first = 0;
            }
            double ns;
            if (p.jitter == async::jitter::decorrelated) {
                double hi = 3 * (double)previous.nanoseconds();
                ns = hi > first ? first + u * (hi - first) : first;
            } else {
                double m = p.multiplier > 1 ? p.multiplier : 1;
                double v = first > 0 ? first * std::pow(m, (double)(n - 1)) : 0;   // inf past the double's range: capped below (0 * inf would be NaN)
                if (!(v < cap)) {
                    v = cap;
                }
                switch (p.jitter) {
                    case async::jitter::none: ns = v; break;
                    case async::jitter::full: ns = u * v; break;
                    case async::jitter::equal: ns = v / 2 + u * (v / 2); break;
                    default: ns = v; break;
                }
            }
            return retry_capped(p, ns, cap);
        }

        // The draws of the jitter: a per-thread splitmix64 seeded from its
        // own address and the clock (spread, not secrecy, as select's)
        inline double retry_uniform() noexcept {
            static thread_local uint64_t x = (uint64_t)(uintptr_t)&x ^ (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
            uint64_t z = (x += 0x9e3779b97f4a7c15ull);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
            z ^= z >> 31;
            return (double)(z >> 11) * 0x1.0p-53;
        }

        // What f gives: the value of its call, or of its task's
        template<class F>
        struct RetryResultOf {
            using type = std::remove_cvref_t<std::invoke_result_t<F&>>;
        };

        template<class X>
        struct RetryTaskValue {
            using type = X;
        };

        template<class T>
        struct RetryTaskValue<task<T>> {
            using type = T;
        };

        template<TaskFactory F>
        struct RetryResultOf<F> {
            using type = typename RetryTaskValue<std::remove_cvref_t<std::invoke_result_t<F&>>>::type;
        };

        template<class F>
        using RetryResult = typename RetryResultOf<F>::type;

        // The predicate of a retry made without one: an attempt is retried
        // when its expected holds an error
        struct RetryOnError {
            template<class R>
            SGCL_INLINE_HOT bool operator()(const R& r) const noexcept {
                return !r.has_value();
            }
        };

        // The token of a retry made without one: never stopped, nothing
        // copied (a default stop_token is a tracked word to make, copy and
        // let go of: 2 ns of a retry's 5 that succeeds at once)
        struct RetryNoStop {
            static constexpr bool stop_requested() noexcept {
                return false;
            }
        };

        template<class S>
        inline constexpr bool retry_has_stop = std::is_same_v<S, stop_token>;

        // Where a retry is between two attempts: the attempts made, the
        // exponential's value for the next wait (multiplied as it goes,
        // retry_delay's pow without the pow), the previous wait
        // (decorrelated jitter's), and the start (read only for a policy
        // with max_elapsed)
        struct RetryState {
            size_t n = 1;
            double v = -1;          // set at the first failure
            duration previous;
            time_point start;

            SGCL_INLINE_HOT explicit RetryState(const retry_policy& p) noexcept {
                if (p.max_elapsed > duration::zero()) {
                    start = clock::now();
                }
            }

            // After failed attempt n (this->n): the wait before the next,
            // or nothing when the policy gives up (the attempts, the time,
            // the predicate)
            SGCL_INLINE_HOT optional<duration> next(const retry_policy& p) {
                if (p.attempts != 0 && n >= p.attempts) {
                    return nullopt;
                }
                if (v < 0) {
                    v = p.initial.nanoseconds() > 0 ? (double)p.initial.nanoseconds() : 0;
                    previous = p.initial;
                }
                double cap = p.max_delay.nanoseconds() > 0 ? (double)p.max_delay.nanoseconds() : 0;
                double ns;
                if (p.jitter == async::jitter::decorrelated) {
                    double first = p.initial.nanoseconds() > 0 ? (double)p.initial.nanoseconds() : 0;
                    double hi = 3 * (double)previous.nanoseconds();
                    ns = hi > first ? first + retry_uniform() * (hi - first) : first;
                } else {
                    double e = v < cap ? v : cap;
                    switch (p.jitter) {
                        case async::jitter::full: ns = e > 0 ? retry_uniform() * e : 0; break;
                        case async::jitter::equal: ns = e > 0 ? e / 2 + retry_uniform() * (e / 2) : 0; break;
                        default: ns = e; break;
                    }
                    if (v < cap) {
                        v *= p.multiplier > 1 ? p.multiplier : 1;
                    }
                }
                duration d = retry_capped(p, ns, cap);
                previous = d;
                if (p.max_elapsed > duration::zero() && duration(clock::now() - start) + d > p.max_elapsed) {
                    return nullopt;
                }
                return d;
            }
        };

        // The attempts of a task: from the state, after the wait d when
        // the first attempt (made by the awaiter) failed, else from the start
        template<class R, class F, class P, class S>
        task<R> retry_task(F f, retry_policy p, P retry_if, S stop, RetryState st, optional<R> last, optional<duration> d) {
            for (;;) {
                if (d) {
                    if constexpr (retry_has_stop<S>) {
                        if (stop.stop_requested()) {
                            co_return std::move(*last);
                        }
                        if (stop.stop_possible()) {
                            if (co_await select(timeout(*d, [] {}), stop.on_stop([] {})) == 1) {
                                co_return std::move(*last);
                            }
                        } else {
                            co_await sleep(*d);
                        }
                    } else {
                        co_await sleep(*d);
                    }
                }
                if (d) {
                    ++st.n;
                }
                last.reset();
                if constexpr (TaskFactory<F>) {
                    last.emplace(co_await f());
                } else {
                    last.emplace(f());
                }
                if (!retry_if(std::as_const(*last))) {
                    co_return std::move(*last);
                }
                d = st.next(p);
                if (!d) {
                    co_return std::move(*last);
                }
            }
        }

        template<class R, class F>
        SGCL_INLINE_HOT R retry_attempt(F& f) {
            if constexpr (TaskFactory<F>) {
                return std::move(f().wait());
            } else {
                return f();
            }
        }

        template<class R, class F, class P, class S>
        R retry_block(F& f, const retry_policy& p, P& retry_if, const S& stop) {
            R r = retry_attempt<R>(f);
            if (!retry_if(std::as_const(r))) [[likely]] {
                return r;
            }
            RetryState st(p);
            for (;;) {
                optional<duration> d = st.next(p);
                if (!d || stop.stop_requested()) {
                    return r;
                }
                if constexpr (retry_has_stop<S>) {
                    if (stop.stop_possible()) {
                        if (select(timeout(*d, [] {}), stop.on_stop([] {})).wait() == 1) {
                            return r;
                        }
                    } else {
                        sleep(*d).wait();
                    }
                } else {
                    sleep(*d).wait();
                }
                ++st.n;
                r = retry_attempt<R>(f);
                if (!retry_if(std::as_const(r))) {
                    return r;
                }
            }
        }

        // What retry() returns: carried out by `co_await` (an awaitable
        // itself: the first attempt of a function made in await_ready, so
        // that a value at once, or an error not retried, costs no frame; a
        // task of its own for the waits and the attempts after them, and
        // for a coroutine function) or by `.wait()` on a thread. Its own
        // type rather than an operation<F> of a closure, as select's is
        template<class R, class F, class P, class S>
        class [[nodiscard]] RetryRun {
        public:
            SGCL_INLINE_HOT RetryRun(F f, const retry_policy& p, P retry_if, const S& stop) noexcept(std::is_nothrow_move_constructible_v<F> && std::is_nothrow_move_constructible_v<P>)
            : _f(std::move(f))
            , _p(p)
            , _retry_if(std::move(retry_if))
            , _stop(stop) {
            }

            RetryRun(const RetryRun&) = delete;
            RetryRun& operator=(const RetryRun&) = delete;

            SGCL_INLINE_HOT ~RetryRun() {
                if (_slow) [[unlikely]] {
                    std::destroy_at(_rest());
                }
            }

            // The thread's way: the attempts and the waits on this thread
            SGCL_INLINE_HOT R wait() && {
                return retry_block<R>(_f, _p, _retry_if, _stop);
            }

            bool await_ready() {
                if constexpr (TaskFactory<F>) {
                    _start(retry_task<R>(std::move(_f), _p, std::move(_retry_if), std::move(_stop), RetryState(_p), nullopt, nullopt));
                    return false;
                } else {
                    _first.emplace(_f());
                    if (!_retry_if(std::as_const(*_first))) [[likely]] {
                        return true;
                    }
                    RetryState st(_p);
                    optional<duration> d = st.next(_p);
                    if (!d || _stop.stop_requested()) {
                        return true;
                    }
                    _start(retry_task<R>(std::move(_f), _p, std::move(_retry_if), std::move(_stop), st, std::move(_first), d));
                    _first.reset();
                    return false;
                }
            }

            template<class H>
            SGCL_INLINE_HOT bool await_suspend(H h) {
                return _rest()->operator co_await().await_suspend(h);
            }

            // The result by value: an R&& into the awaitable would save
            // making and destroying a second R (1.5 ns of a retry that
            // succeeds at once: the destructor of core's expected is an
            // indirect call), but `const auto& r = co_await retry(...)`
            // would bind to the awaitable dying at the semicolon
            SGCL_INLINE_HOT R await_resume() {
                if (_slow) {
                    return std::move(_rest()->operator co_await().await_resume());
                }
                return std::move(*_first);
            }

        private:
            F _f;
            retry_policy _p;
            P _retry_if;
            [[no_unique_address]] S _stop;
            optional<R> _first;
            // The task of the waits, in storage of its own and destroyed by
            // hand: an optional<task<R>>'s destructor was a call on every
            // retry that succeeded at once, an empty task's a cell (1 and 4
            // ns of a 4 ns retry)
            bool _slow = false;
            alignas(task<R>) unsigned char _rest_bytes[sizeof(task<R>)];

            SGCL_INLINE_HOT task<R>* _rest() noexcept {
                return std::launder(reinterpret_cast<task<R>*>(_rest_bytes));
            }

            SGCL_INLINE_HOT void _start(task<R> t) noexcept {
                ::new (static_cast<void*>(_rest_bytes)) task<R>(std::move(t));
                _slow = true;
            }
        };

        template<class F, class P, class S>
        SGCL_INLINE_HOT auto retry_run(F f, const retry_policy& policy, P retry_if, const S& stop) {
            using R = RetryResult<F>;
            static_assert(!std::is_same_v<P, RetryOnError> || IsExpected<R>::value, "sgcl::async::retry: f returns an expected<T, E> (or a task of one), whose error is retried; another result needs a predicate");
            static_assert(std::is_invocable_r_v<bool, P&, const R&>, "sgcl::async::retry: the predicate takes f's result and says whether to try again");
            return RetryRun<R, F, P, S>(std::move(f), policy, std::move(retry_if), stop);
        }
    }

    // f called until it gives a value or the policy gives up, and its
    // last result: `co_await async::retry(f)` in a task,
    // `async::retry(f).wait()` on a thread. The first attempt always runs
    template<class F>
    requires std::invocable<F&>
    SGCL_INLINE_HOT auto retry(F f, const retry_policy& policy = {}) {
        return detail::retry_run(std::move(f), policy, detail::RetryOnError{}, detail::RetryNoStop{});
    }

    // The same, ended also by the stop of the token: a stop during a wait
    // ends the retry with the last error, a stop before the first attempt
    // after it
    template<class F>
    requires std::invocable<F&>
    SGCL_INLINE_HOT auto retry(F f, const retry_policy& policy, const stop_token& stop) {
        return detail::retry_run(std::move(f), policy, detail::RetryOnError{}, stop);
    }

    // The same, an attempt tried again when retry_if(its result) says so,
    // a value as well as an error: a 503 is a response, and retried, a 404
    // an answer; f's result may be any type then
    template<class F, class P>
    requires std::invocable<F&> && (!std::is_same_v<std::remove_cvref_t<P>, stop_token>)
    SGCL_INLINE_HOT auto retry(F f, const retry_policy& policy, P retry_if) {
        return detail::retry_run(std::move(f), policy, std::move(retry_if), detail::RetryNoStop{});
    }

    template<class F, class P>
    requires std::invocable<F&> && (!std::is_same_v<std::remove_cvref_t<P>, stop_token>)
    SGCL_INLINE_HOT auto retry(F f, const retry_policy& policy, P retry_if, const stop_token& stop) {
        return detail::retry_run(std::move(f), policy, std::move(retry_if), stop);
    }
}
