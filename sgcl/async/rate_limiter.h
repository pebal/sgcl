//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/atomic.h"
#include "../core/clock.h"
#include "../core/detail/backoff.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "coroutine.h"
#include "operation.h"
#include "select.h"
#include "stop_token.h"
#include "timer.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <thread>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // A token bucket, Go's golang.org/x/time/rate.Limiter: `burst` tokens,
    // refilled at `limit` a second, an event taking one (or n). allow()
    // takes them now or says no, reserve() takes them for the time they
    // come and says when that is, acquire() waits for them in a task or on a
    // thread, a stop_token ending the wait.
    //
    // The bucket is the generic cell rate algorithm, its equivalent in one
    // word: the theoretical arrival time `tat`, the point at which the
    // bucket would be full again. n tokens at `now` are
    // `new = max(tat, now) + n*T` (T the time of one token), granted when
    // `new - now <= burst*T`; the tokens there are
    // `(now + burst*T - max(tat, now)) / T`. So allow() is a load of the
    // clock and one compare-exchange, no lock (Go's Limiter takes a mutex
    // around a float of tokens and the time of the last event).
    //
    // The word counts ticks of 2^-10 ns from a base (a rate of 150 million
    // a second, bytes say, keeps its rate to 1e-4 where whole nanoseconds
    // would be 5% off), the tick widened to whole nanoseconds for a slow
    // rate whose burst*T would not fit. The constants of the count (T, the
    // burst, the base, the tick) are an epoch, immutable, beside the word:
    // a change of the limit or the burst, and a new base every ~26 days of
    // the clock, freeze the old epoch's word with a compare-exchange to a
    // sentinel, carry the tokens of the moment into a new epoch and publish
    // it, one change at a time; a call that meets the sentinel waits for
    // the publication (a few instructions away) and goes on in the new
    // epoch. A limit of inf is a mode with no word at all; a limit of zero
    // (or less, or NaN) a count of the tokens left, which nothing refills.
    class rate_error {
    public:
        enum class reason : uint8_t {
            stopped,    // the token was stopped, before the wait or during it
            deadline,   // the tokens come after the token's deadline, never (a limit of zero), or further than the limiter counts
            burst       // more tokens than the burst: never at once at a finite rate
        };

        SGCL_INLINE_HOT constexpr explicit rate_error(reason r) noexcept
        : _reason(r) {
        }

        SGCL_INLINE_HOT constexpr reason why() const noexcept {
            return _reason;
        }

        SGCL_INLINE_HOT string message() const noexcept {
            switch (_reason) {
                case reason::stopped: return "stopped";
                case reason::deadline: return "the tokens would come after the deadline";
                case reason::burst: return "more tokens than the burst";
            }
            return "stopped";
        }

        friend bool operator==(const rate_error&, const rate_error&) noexcept = default;

    private:
        reason _reason;
    };

    class rate_limiter;

    namespace detail {
        inline constexpr int64_t RateFrozen = std::numeric_limits<int64_t>::min();   // the word of an epoch replaced
        inline constexpr int64_t RateCapacityMax = int64_t(1) << 58;                  // burst*T at most
        inline constexpr int64_t RateNowMax = int64_t(1) << 61;                       // ticks from the base before a new base
        inline constexpr int64_t RateFar = int64_t(1) << 62;                          // a tat past this is refused

        enum class RateMode : uint8_t { finite, unlimited, zero };

        // The constants of the count and the word: tat in ticks from the
        // base (finite), the tokens left (zero), unused (unlimited)
        // The constants first and the word a line of its own (two, for the
        // adjacent-line prefetch), so that the compare-exchanges of many
        // threads do not take the constants' line from the readers (8
        // threads on one bucket: 413 ns a call with the word beside them)
        struct RateEpoch {
            int64_t base = 0;        // the clock's nanoseconds at the start of the epoch
            int64_t horizon = 0;     // nanoseconds from the base after which the epoch is renewed
            int64_t interval = 1;    // ticks of one token: T
            int64_t capacity = 0;    // burst*T, at most RateCapacityMax
            double limit = 0;        // tokens a second, as given (inf, 0)
            size_t burst = 0;
            int up = 0;              // a tick is 2^-up nanoseconds
            RateMode mode = RateMode::finite;
            char before[128];
            std::atomic<int64_t> word{0};
            char after[128 - sizeof(std::atomic<int64_t>)];

            SGCL_INLINE_HOT static int64_t ns_of(time_point t) noexcept {
                return std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count();
            }

            // The ticks of now; -1 past the horizon (a new base due)
            SGCL_INLINE_HOT int64_t ticks(time_point now) const noexcept {
                int64_t ns = ns_of(now) - base;
                if (ns > horizon) [[unlikely]] {
                    return -1;
                }
                return ns <= 0 ? 0 : ns << up;
            }

            // A count of ticks as a point of the clock, rounded up: never
            // before the tokens are there
            SGCL_INLINE_HOT time_point at(int64_t t) const noexcept {
                int64_t ns = t <= 0 ? 0 : (t + (int64_t(1) << up) - 1) >> up;
                return time_point(std::chrono::duration_cast<time_point::duration>(std::chrono::nanoseconds(base + ns)));
            }

            // The ticks of n tokens, or -1 when they are more than the
            // bucket holds (n > burst, or burst*T saturated)
            SGCL_INLINE_HOT int64_t need(size_t n) const noexcept {
                int64_t r;
                if (n > burst || n > (size_t)std::numeric_limits<int64_t>::max() || __builtin_mul_overflow((int64_t)n, interval, &r) || r > capacity) {
                    return -1;
                }
                return r;
            }

            // The tokens at now, given the word w (not frozen)
            double tokens(int64_t w, int64_t now_t) const noexcept {
                switch (mode) {
                    case RateMode::unlimited: return (double)burst;
                    case RateMode::zero: return (double)w;
                    case RateMode::finite: break;
                }
                return (double)(now_t + capacity - (w > now_t ? w : now_t)) / (double)interval;
            }

            // A new epoch at `now` with the given constants and `tokens`
            static tracked_ptr<RateEpoch> make(double limit, size_t burst, time_point now, double tokens) {
                tracked_ptr<RateEpoch> e = make_tracked<RateEpoch>();
                e->limit = limit;
                e->burst = burst;
                e->base = ns_of(now);
                if (std::isinf(limit) && limit > 0) {
                    e->mode = RateMode::unlimited;
                    e->horizon = std::numeric_limits<int64_t>::max();
                    return e;
                }
                if (!(limit > 0)) {   // zero, less, NaN: the burst once, never refilled
                    e->mode = RateMode::zero;
                    e->horizon = std::numeric_limits<int64_t>::max();
                    double k = std::floor(tokens);
                    e->word.store(k >= 9e18 ? std::numeric_limits<int64_t>::max() / 2 : k <= -9e18 ? -std::numeric_limits<int64_t>::max() / 2 : (int64_t)k, std::memory_order_relaxed);
                    return e;
                }
                // the finest tick under which burst*T fits; whole nanoseconds at the least
                double t_ns = 1e9 / limit;
                int up = 10;
                for (; up > 0; --up) {
                    double t = t_ns * (double)(int64_t(1) << up);
                    if (t * (double)burst <= (double)RateCapacityMax && t <= (double)RateCapacityMax) {
                        break;
                    }
                }
                double t = t_ns * (double)(int64_t(1) << up);
                e->up = up;
                e->interval = t >= (double)RateCapacityMax ? RateCapacityMax : t < 1 ? 1 : (int64_t)std::llround(t);
                double cap = (double)e->interval * (double)burst;
                e->capacity = cap >= (double)RateCapacityMax ? RateCapacityMax : (int64_t)cap;
                e->horizon = RateNowMax >> up;
                // tat = burst*T - tokens*T (now is tick 0): a full bucket at or below 0
                double w = (double)e->capacity - tokens * (double)e->interval;
                e->word.store(w >= (double)RateFar ? RateFar : w <= -(double)RateFar ? -RateFar : (int64_t)std::llround(w), std::memory_order_relaxed);
                return e;
            }
        };

        // What a reservation took: the epoch, the word it left (end), the
        // ticks it took (need) and the point the caller may act at
        struct RateTaken {
            tracked_ptr<RateEpoch> epoch;
            int64_t end = 0;
            int64_t need = 0;
            time_point act = {};
            bool ok = false;
        };

        // The outcome of an attempt in one epoch: done (taken or refused),
        // or the epoch is frozen or past its horizon
        enum class RateTry : uint8_t { taken, refused, burst, renew };

        struct RateState {
            atomic<tracked_ptr<RateEpoch>> epoch;
            std::atomic<bool> changing{false};   // one change of epoch at a time

            SGCL_INLINE_HOT tracked_ptr<RateEpoch> current() const noexcept {
                return epoch.load(std::memory_order_acquire);
            }

            // The tokens of a reservation in epoch e: taken when they come
            // within max_wait ticks of now, else nothing changed
            static RateTry take(RateEpoch& e, size_t n, int64_t now_t, int64_t max_wait, int64_t& end, int64_t& need) noexcept {
                switch (e.mode) {
                    case RateMode::unlimited:
                        end = now_t;
                        need = 0;
                        return RateTry::taken;
                    case RateMode::zero: {
                        if (n > (size_t)std::numeric_limits<int64_t>::max()) {
                            return RateTry::refused;
                        }
                        int64_t w = e.word.load(std::memory_order_relaxed);
                        for (;;) {
                            if (w == RateFrozen) {
                                return RateTry::renew;
                            }
                            if (w < (int64_t)n) {
                                return RateTry::refused;
                            }
                            if (e.word.compare_exchange_weak(w, w - (int64_t)n, std::memory_order_relaxed)) {
                                end = now_t;
                                need = 0;
                                return RateTry::taken;
                            }
                        }
                    }
                    case RateMode::finite:
                        break;
                }
                int64_t k = e.need(n);
                if (k < 0) {
                    return RateTry::burst;
                }
                int64_t w = e.word.load(std::memory_order_relaxed);
                Backoff<> backoff;   // after a lost exchange, as at every contended word of the module: 8 threads on one bucket 405 ns a call without it, 25 with it
                for (;;) {
                    if (w == RateFrozen) {
                        return RateTry::renew;
                    }
                    int64_t nw = (w > now_t ? w : now_t) + k;
                    if (nw - now_t - e.capacity > max_wait || nw > RateFar) {
                        return RateTry::refused;
                    }
                    if (e.word.compare_exchange_weak(w, nw, std::memory_order_relaxed)) {
                        end = nw;
                        need = k;
                        return RateTry::taken;
                    }
                    backoff();
                }
            }

            // A new epoch in place of e: the constants changed (the
            // limit, the burst) or the same ones from a new base. The word
            // of e frozen with a compare-exchange, the tokens it held at
            // now carried over, the new epoch published. One change at a
            // time; another caller's change of the same epoch done first
            // makes this one a change of the epoch it published
            template<class F>
            void change(F constants) {
                while (changing.exchange(true, std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                tracked_ptr<RateEpoch> e = current();
                time_point now = clock::now();
                int64_t w = e->word.load(std::memory_order_relaxed);
                while (!e->word.compare_exchange_weak(w, RateFrozen, std::memory_order_relaxed)) {
                }
                int64_t ns = RateEpoch::ns_of(now) - e->base;
                int64_t now_t = ns <= 0 ? 0 : ns > e->horizon ? RateNowMax : ns << e->up;
                double tokens = e->tokens(w, now_t);
                double limit = e->limit;
                size_t burst = e->burst;
                constants(limit, burst);
                if (tokens > (double)burst) {   // the new burst caps them, a count of zero mode's too
                    tokens = (double)burst;
                }
                epoch.store(RateEpoch::make(limit, burst, now, tokens), std::memory_order_release);
                changing.store(false, std::memory_order_release);
            }

            // An epoch met frozen or past its horizon: renewed by this
            // caller unless another one is at it, then the new one read
            void renew(const tracked_ptr<RateEpoch>& e) {
                if (e->word.load(std::memory_order_relaxed) == RateFrozen) {
                    while (current() == e) {   // its change is a few instructions from the publication
                        std::this_thread::yield();
                    }
                    return;
                }
                change([](double&, size_t&) {});
            }

            // The reservation of n at now, renewing the epoch as needed
            RateTry reserve(size_t n, time_point now, duration max_wait, RateTaken& r) {
                for (;;) {
                    tracked_ptr<RateEpoch> e = current();
                    int64_t now_t = e->ticks(now);
                    RateTry t = RateTry::renew;
                    if (now_t >= 0) {
                        int64_t wait_ns = max_wait.nanoseconds();
                        int64_t wait_t = wait_ns <= 0 ? 0 : wait_ns >= (RateFar >> e->up) ? RateFar : wait_ns << e->up;
                        t = take(*e, n, now_t, wait_t, r.end, r.need);
                    }
                    if (t == RateTry::renew) {
                        renew(e);
                        continue;
                    }
                    if (t == RateTry::taken) {
                        r.ok = true;
                        r.act = e->mode == RateMode::finite && r.end - e->capacity > now_t ? e->at(r.end - e->capacity) : now;
                        r.epoch = std::move(e);
                    }
                    return t;
                }
            }

            // The tokens of a reservation given back, by Go's rule: the
            // ones a later reservation has not counted on, and only before
            // the reservation's time; nothing once its epoch is replaced
            static void cancel(RateTaken& r) noexcept {
                if (!r.ok || r.need == 0 || !r.epoch || r.epoch->mode != RateMode::finite) {
                    r.ok = false;
                    return;
                }
                r.ok = false;
                if (clock::now() >= r.act) {
                    return;
                }
                RateEpoch& e = *r.epoch;
                int64_t w = e.word.load(std::memory_order_relaxed);
                for (;;) {
                    if (w == RateFrozen) {
                        return;
                    }
                    int64_t later = w - r.end;
                    int64_t restore = r.need - (later > 0 ? later : 0);
                    if (restore <= 0) {
                        return;
                    }
                    if (e.word.compare_exchange_weak(w, w - restore, std::memory_order_relaxed)) {
                        return;
                    }
                }
            }
        };

        struct RateAccess;
    }

    // A token bucket of `burst` tokens refilled at a limit a second: a
    // handle, one tracked word, its copies sharing the bucket
    class rate_limiter {
    public:
        static constexpr double inf = std::numeric_limits<double>::infinity();

        // Tokens taken ahead (Go's Reservation): when the caller may act,
        // and cancel() to give the tokens back. Move-only, so that the
        // tokens are given back once
        class reservation {
        public:
            reservation() noexcept = default;

            SGCL_INLINE_HOT reservation(reservation&& o) noexcept
            : _r(std::move(o._r)) {
                o._r.ok = false;
            }

            SGCL_INLINE_HOT reservation& operator=(reservation&& o) noexcept {
                _r = std::move(o._r);
                o._r.ok = false;
                return *this;
            }

            reservation(const reservation&) = delete;
            reservation& operator=(const reservation&) = delete;

            // Whether the tokens were taken: false for more than the
            // burst, a limit of zero with too few left, a reservation
            // cancelled or moved from
            SGCL_INLINE_HOT bool ok() const noexcept {
                return _r.ok;
            }

            // When the caller may act: a point of the module's clock (now
            // or before for tokens that were there); time_point::max() when
            // not ok
            SGCL_INLINE_HOT time_point time() const noexcept {
                return _r.ok ? _r.act : time_point::max();
            }

            // How long until then from now; zero when it has come,
            // duration::max() when not ok
            SGCL_INLINE_HOT duration delay() const noexcept {
                if (!_r.ok) {
                    return duration::max();
                }
                time_point now = clock::now();
                return _r.act > now ? duration(_r.act - now) : duration::zero();
            }

            // The tokens given back, as Go gives them: those no later
            // reservation has counted on, and none once the time has come
            // or the limit has changed. A second cancel does nothing
            SGCL_INLINE_HOT void cancel() noexcept {
                detail::RateState::cancel(_r);
            }

        private:
            friend class rate_limiter;
            friend struct detail::RateAccess;

            detail::RateTaken _r;
        };

        // A bucket of `burst` tokens, full, refilled at `per_second`;
        // rate_limiter::inf allows everything, zero (or less) the burst
        // once
        rate_limiter(double per_second, size_t burst)
        : _s(make_tracked<detail::RateState>()) {
            _s->epoch.store(detail::RateEpoch::make(per_second, burst, clock::now(), (double)burst));
        }

        // One token every `interval` (Go's rate.Every): zero or less is
        // inf
        rate_limiter(duration interval, size_t burst)
        : rate_limiter(_per_second(interval), burst) {
        }

        // n tokens now, or false and nothing taken: one compare-exchange
        SGCL_INLINE_HOT bool allow(size_t n = 1) noexcept {
            time_point now = clock::now();
            for (;;) {
                tracked_ptr<detail::RateEpoch> e = _s->current();
                int64_t now_t = e->ticks(now);
                if (now_t >= 0) [[likely]] {
                    int64_t end, need;
                    switch (detail::RateState::take(*e, n, now_t, 0, end, need)) {
                        case detail::RateTry::taken: return true;
                        case detail::RateTry::refused:
                        case detail::RateTry::burst: return false;
                        case detail::RateTry::renew: break;
                    }
                }
                _renew(e);
            }
        }

        // n tokens taken now for the time they come: the reservation says
        // when that is, ok() false (nothing taken) for more than the
        // burst, or for too few left at a limit of zero
        SGCL_INLINE_HOT reservation reserve(size_t n = 1) noexcept {
            reservation r;
            _reserve(n, clock::now(), duration::max(), r._r);
            return r;
        }

        // Waits for n tokens, and gives nothing or why it gave up:
        // `co_await lim.acquire()` in a task, `lim.acquire().wait()` on a
        // thread (Go's Wait, named as semaphore names its wait).
        // The stop of `stop` ends the wait and gives the tokens back; a
        // wait whose tokens come after the token's deadline (an armed
        // stop_after or stop_at, the source's or an ancestor's) fails at
        // once, taking nothing, as Go's Wait does
        SGCL_INLINE_HOT auto acquire(size_t n = 1, const stop_token& stop = {}) const noexcept {
            return detail::make_operation([s = _s, n, stop](auto how) -> decltype(auto) {
                if constexpr (detail::is_awaited<decltype(how)>) {
                    return waiter(s, n, stop);
                } else {
                    return waiter::block(s, n, stop);
                }
            });
        }

        // A new limit, the tokens of now kept; waits already reserved keep
        // their times
        void set_limit(double per_second) {
            _s->change([per_second](double& limit, size_t&) { limit = per_second; });
        }

        void set_limit(duration interval) {
            set_limit(_per_second(interval));
        }

        // A new burst, the tokens of now kept up to it
        void set_burst(size_t burst) {
            _s->change([burst](double&, size_t& b) { b = burst; });
        }

        // Tokens a second: inf, or 0 for a bucket never refilled
        SGCL_INLINE_HOT double limit() const noexcept {
            double l = _s->current()->limit;
            return l > 0 ? l : 0;
        }

        SGCL_INLINE_HOT size_t burst() const noexcept {
            return _s->current()->burst;
        }

        // The tokens now, fractional; negative while reservations are
        // ahead of the refill
        double tokens() const noexcept {
            time_point now = clock::now();
            for (;;) {
                tracked_ptr<detail::RateEpoch> e = _s->current();
                int64_t w = e->word.load(std::memory_order_relaxed);
                if (w == detail::RateFrozen) {
                    std::this_thread::yield();
                    continue;
                }
                int64_t now_t = e->ticks(now);
                if (now_t < 0) {
                    now_t = detail::RateNowMax;
                }
                double k = e->tokens(w, now_t);
                return k > (double)e->burst ? (double)e->burst : k;
            }
        }

        SGCL_INLINE_HOT friend bool operator==(const rate_limiter& a, const rate_limiter& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend struct detail::RateAccess;

        SGCL_INLINE_HOT static double _per_second(duration interval) noexcept {
            return interval <= duration::zero() ? inf : 1e9 / (double)interval.nanoseconds();
        }

        // renew is not noexcept-safe only through make_tracked, which ends
        // the program rather than throw (the collector's limit)
        SGCL_INLINE_HOT void _renew(const tracked_ptr<detail::RateEpoch>& e) noexcept {
            _s->renew(e);
        }

        SGCL_INLINE_HOT detail::RateTry _reserve(size_t n, time_point now, duration max_wait, detail::RateTaken& r) const noexcept {
            return _s->reserve(n, now, max_wait, r);
        }

        // The wait of acquire(): the reservation in await_ready (no frame for
        // tokens that are there, or a wait that fails), a task of its own
        // for one that sleeps
        class waiter {
        public:
            using result_type = expected<void, rate_error>;

            SGCL_INLINE_HOT waiter(tracked_ptr<detail::RateState> s, size_t n, stop_token stop) noexcept
            : _s(std::move(s))
            , _n(n)
            , _stop(std::move(stop)) {
            }

            SGCL_INLINE_HOT bool await_ready() {
                reservation r;
                if (_start(_s, _n, _stop, r, _result)) {
                    return true;
                }
                _slow.emplace(_sleep(std::move(r), _stop));
                return false;
            }

            template<class P>
            SGCL_INLINE_HOT bool await_suspend(std::coroutine_handle<P> h) {
                return _slow->operator co_await().await_suspend(h);
            }

            SGCL_INLINE_HOT result_type await_resume() {
                if (_slow) {
                    return _slow->operator co_await().await_resume();
                }
                return _result;
            }

            // The thread's wait
            static result_type block(const tracked_ptr<detail::RateState>& s, size_t n, const stop_token& stop) {
                reservation r;
                result_type result;
                if (_start(s, n, stop, r, result)) {
                    return result;
                }
                if (!stop.stop_possible()) {
                    sleep_until(r.time()).wait();
                    return {};
                }
                if (select(timeout(r.time(), [] {}), stop.on_stop([] {})).wait() == 1) {
                    r.cancel();
                    return unexpected(rate_error(rate_error::reason::stopped));
                }
                return {};
            }

        private:
            // The reservation, and true when the wait is over without a
            // sleep: the tokens there, or a failure in `result`
            static bool _start(const tracked_ptr<detail::RateState>& s, size_t n, const stop_token& stop, reservation& r, result_type& result) {
                duration max_wait = duration::max();
                time_point now = clock::now();
                if (stop.stop_possible()) {
                    if (stop.stop_requested()) {
                        result = unexpected(rate_error(rate_error::reason::stopped));
                        return true;
                    }
                    time_point d = detail::StopAccess::deadline(stop);
                    if (d != time_point::max()) {
                        max_wait = d > now ? duration(d - now) : duration::zero();
                    }
                }
                switch (s->reserve(n, now, max_wait, r._r)) {
                    case detail::RateTry::taken:
                        break;
                    case detail::RateTry::burst:
                        result = unexpected(rate_error(rate_error::reason::burst));
                        return true;
                    default:
                        result = unexpected(rate_error(rate_error::reason::deadline));
                        return true;
                }
                return r.time() <= now;
            }

            static task<result_type> _sleep(reservation r, stop_token stop) {
                if (!stop.stop_possible()) {
                    co_await sleep_until(r.time());
                    co_return result_type();
                }
                if (co_await select(timeout(r.time(), [] {}), stop.on_stop([] {})) == 1) {
                    r.cancel();
                    co_return unexpected(rate_error(rate_error::reason::stopped));
                }
                co_return result_type();
            }

            tracked_ptr<detail::RateState> _s;
            size_t _n;
            stop_token _stop;
            result_type _result;
            optional<task<result_type>> _slow;
        };

        tracked_ptr<detail::RateState> _s;
    };
}
