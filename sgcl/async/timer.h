//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/root_ptr.h"
#include "channel.h"
#include "scheduler.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace sgcl {
    // Time, the way Go has it: `co_await sleep(d)` suspends a task for d,
    // `co_await sleep_until(t)` until a point; `after(d)` is a channel
    // that gets one signal after d, then is closed, `at(t)` the same at
    // a point; `tick(d)` a channel that gets a signal every d until it is
    // closed (a tick nobody has taken yet is dropped, not queued), its
    // first tick alignable to a point; and `timeout(d, f)` is a case of a
    // select (select.h), served after d. Under them one timer thread with
    // a heap of timers, asleep until the earliest is due; a timer is a
    // managed object held by a root_ptr in the heap, so what it points to
    // (a frame, a channel) lives while the timer does.
    //
    // The module reads the time in one place, `clock::now()`: the steady
    // clock, unless a test has installed a manual_clock, whose time moves
    // only when the test advances it, every timer due firing then, in
    // order, with no real waiting (tokio's time::pause and advance,
    // Kotlin's runTest): a test of a thirty-second timeout takes
    // microseconds. The production path pays one relaxed load of the
    // flag per read and nothing else.
    using duration = std::chrono::steady_clock::duration;
    using time_point = std::chrono::steady_clock::time_point;

    namespace detail {
        // The manual clock's state, static so that a read is a load of
        // the flag and, when it is set, of the time: the flag relaxed
        // (the reads that matter, the timer thread's, happen under the
        // timers' lock the advance takes after storing the time), the
        // time with the release/acquire pair of advance and the reads
        inline std::atomic<bool> manual_clock_installed = {false};
        inline std::atomic<time_point::rep> manual_clock_now = {0};
    }

    // The clock of the module: a Clock of the standard library, the
    // steady clock's time, or the manual clock's while one is installed
    struct clock {
        using rep = time_point::rep;
        using period = time_point::period;
        using duration = sgcl::duration;
        using time_point = sgcl::time_point;
        static constexpr bool is_steady = true;

        static time_point now() noexcept {
            if (detail::manual_clock_installed.load(std::memory_order_relaxed)) [[unlikely]] {
                return time_point(duration(detail::manual_clock_now.load(std::memory_order_acquire)));
            }
            return std::chrono::steady_clock::now();
        }
    };

    namespace detail {
        class Timers;
        inline Timers& timers_instance();
        inline void timer_cancelled() noexcept;

        struct Timer {
            time_point when;
            duration period = duration::zero();         // a tick: fired again every period
            tracked_ptr<FrameWord> frame;               // a sleep: the coroutine to resume
            tracked_ptr<void> keep;                     // an after, a tick or a call: the object that holds the channel or is called, alive while the timer is
            channel<void>* ch = nullptr;                // the channel to signal, inside it
            void (*fire)(void*) = nullptr;              // a call: run on the timer's thread with `keep` (a stop_source's deadline, a timeout's)
            atomic<bool> cancelled = {false};           // by its owner (a race its task won): not fired, swept out of the heap with the closed ones
        };

        class Timers {
        public:
            Timers() {
                scheduler_instance();   // made after the scheduler, so destroyed before it: a timer firing at exit finds the scheduler, not a destroyed one
            }

            ~Timers() {
                stop();
            }

            void add(root_ptr<Timer> t) {
                std::unique_lock lock(_m);
                if (!_running) {
                    _stop = false;
                    _running = true;
                    _thread = std::thread([this] { _run(); });
                    scheduler_stop_hook.store([] { timers_instance().stop(); }, std::memory_order_release);   // scheduler::stop() stops the timers too
                }
                if (_dead.load(std::memory_order_relaxed) > _heap.size() / 2 && _heap.size() > 64) {   // the timers cancelled (their channels closed: timeout_case, or an after channel closed by hand) swept out once they are half the heap: amortized nothing per add
                    std::erase_if(_heap, [](const root_ptr<Timer>& t) { return t->cancelled.load(std::memory_order_acquire) || (t->ch && t->ch->closed()); });
                    std::make_heap(_heap.begin(), _heap.end(), _later);
                    _dead.store(0, std::memory_order_relaxed);
                }
                auto added = t.get();
                _heap.push_back(std::move(t));
                std::push_heap(_heap.begin(), _heap.end(), _later);
                bool first = _heap.front().get() == added;
                lock.unlock();
                if (first) {
                    _cv.notify_one();   // the earliest changed: the thread looks again (a timer behind the front changes nothing the thread waits for, and a wake of a parked thread costs a microsecond or two)
                }
            }

            // The thread joined; the timers not yet due stay in the heap and
            // fire when the next timer starts the thread again
            void stop() {
                {
                    std::lock_guard lock(_m);
                    if (!_running) {
                        return;
                    }
                    _stop = true;
                }
                _cv.notify_one();
                _thread.join();
                std::lock_guard lock(_m);   // the ticks whose channel was closed: gone with the thread, their channels with them
                _running = false;   // a timer added during the join stays in the heap, as one not yet due does, and fires when the next add starts the thread again
                std::erase_if(_heap, [](const root_ptr<Timer>& t) { return t->cancelled.load(std::memory_order_acquire) || (t->ch && t->ch->closed()); });
                std::make_heap(_heap.begin(), _heap.end(), _later);
                _settled = _epoch;          // an advance waiting for the thread: nothing more will fire
                _settled_cv.notify_all();
            }

            // A timer on a channel cancelled by its owner (timeout_case): counted, for the sweep in add()
            void cancelled() noexcept {
                _dead.fetch_add(1, std::memory_order_relaxed);
            }

            // The timers in the heap, for the tests
            size_t size() {
                std::lock_guard lock(_m);
                return _heap.size();
            }

            // The clock changed (a manual clock installed, uninstalled or
            // advanced): the thread reads the time again. After an advance
            // the call returns once every timer due at the new time has
            // fired and the thread waits for the next: what makes a test
            // deterministic
            void clock_changed() {
                std::unique_lock lock(_m);
                if (!_running) {
                    return;
                }
                ++_epoch;
                _cv.notify_one();
                _settled_cv.wait(lock, [this] { return _settled == _epoch; });
            }

        private:
            static bool _later(const root_ptr<Timer>& a, const root_ptr<Timer>& b) noexcept {
                return a->when > b->when;
            }

            void _run() {
                std::unique_lock lock(_m);
                for (;;) {
                    if (_stop) {
                        return;
                    }
                    auto now = clock::now();
                    if (_heap.empty() || _heap.front()->when > now) {
                        if (_settled != _epoch) {   // nothing due at this time: whoever changed the clock may go on
                            _settled = _epoch;
                            _settled_cv.notify_all();
                        }
                        if (_heap.empty() || manual_clock_installed.load(std::memory_order_relaxed)) {
                            _cv.wait(lock);         // under a manual clock time moves only by an advance, which wakes the thread
                        } else {
                            _cv.wait_until(lock, _heap.front()->when);
                        }
                        continue;
                    }
                    std::pop_heap(_heap.begin(), _heap.end(), _later);
                    root_ptr<Timer> t = std::move(_heap.back());
                    _heap.pop_back();
                    lock.unlock();
                    _fire(t);
                    lock.lock();
                    if (t->period != duration::zero() && !t->ch->closed()) {
                        t->when += t->period;
                        _heap.push_back(std::move(t));
                        std::push_heap(_heap.begin(), _heap.end(), _later);
                    }
                }
            }

            static void _fire(const root_ptr<Timer>& t) {
                if (t->cancelled.load(std::memory_order_acquire)) {
                    return;
                }
                if (t->frame) {
                    enqueue(t->frame, false);
                } else if (t->fire) {
                    t->fire(t->keep.get());
                } else if (t->period != duration::zero()) {
                    t->ch->try_send();   // a tick nobody took is dropped
                } else {
                    t->ch->try_send();   // after: one signal, then the close
                    t->ch->close();
                }
            }

            std::mutex _m;
            std::condition_variable _cv;
            std::condition_variable _settled_cv;
            std::vector<root_ptr<Timer>> _heap;
            std::thread _thread;
            bool _stop = false;
            bool _running = false;   // the thread started and not yet joined (under _m: the thread object itself is not looked at by two threads)
            std::atomic<size_t> _dead = {0};   // the timers cancelled since the last sweep (an estimate: a case gone after its timer fired counts too)
            uint64_t _epoch = 0;     // bumped by every change of the clock
            uint64_t _settled = 0;   // the epoch the thread last found nothing due at
        };

        inline Timers& timers_instance() {
            static Timers timers;
            return timers;
        }

        inline void timer_cancelled() noexcept {
            timers_instance().cancelled();
        }

        // A timer on a channel: one signal at `when` and the close (period
        // zero), or a signal at `when` and every period after until the
        // channel is closed
        inline void add_timer(time_point when, duration period, tracked_ptr<void> keep, channel<void>* ch) {
            root_ptr<Timer> t = make_tracked<Timer>();
            t->when = when;
            t->period = period;
            t->keep = std::move(keep);
            t->ch = ch;
            timers_instance().add(std::move(t));
        }

        inline void add_timer(duration d, duration period, tracked_ptr<void> keep, channel<void>* ch) {
            add_timer(clock::now() + d, period, std::move(keep), ch);
        }

        // A call at `when`, on the timer's thread, with the object kept:
        // what does more than a channel can (a stop_source's deadline:
        // the close and the children in one step, so that the stop is
        // requested before anyone woken by it looks)
        inline tracked_ptr<Timer> add_timer(time_point when, tracked_ptr<void> keep, void (*fire)(void*)) {
            root_ptr<Timer> t = make_tracked<Timer>();
            t->when = when;
            t->keep = std::move(keep);
            t->fire = fire;
            tracked_ptr<Timer> handle(t);   // for the owner that may cancel it
            timers_instance().add(std::move(t));
            return handle;
        }

        inline tracked_ptr<Timer> add_timer(duration d, tracked_ptr<void> keep, void (*fire)(void*)) {
            return add_timer(clock::now() + d, std::move(keep), fire);
        }

        // A sleep: the frame resumed at `when`
        inline void add_sleep(time_point when, tracked_ptr<FrameWord> frame) {
            root_ptr<Timer> t = make_tracked<Timer>();
            t->when = when;
            t->frame = std::move(frame);
            timers_instance().add(std::move(t));
        }

        // The workers idle: no task ready, none running, every worker
        // asleep (or no scheduler): every task has reached its next wait.
        // What manual_clock::advance waits for on either side of the move,
        // so that a task woken by a timer arms its next one before the
        // advance returns; a task that waits by spinning, or a thread
        // that blocks a worker, holds the advance
        inline void wait_for_idle_workers() {
            for (;;) {
                auto st = scheduler::get_statistics();
                if (st.workers == 0 || (st.global_queued == 0 && st.local_queued == 0 && st.spinning == 0 && st.sleeping == st.workers)) {
                    return;
                }
                std::this_thread::yield();
            }
        }
    }

    // The clock of a test: installed, it stops the module's time at the
    // steady clock's now, and moves it only by `advance(d)`, which fires
    // every timer due by then, in order, and returns when the tasks they
    // woke have run to their next waits. A sleep of thirty seconds
    // completes in microseconds of wall time, a tick fires the number of
    // times the advance covers (coalesced to the one the channel holds
    // when nobody receives in between), a timeout case and a deadline
    // fire when the time comes. One installed at a time; the destructor
    // uninstalls, and the steady clock is the time again: a timer armed
    // under the manual clock and not yet due keeps its point, which the
    // steady clock reaches later, since the manual time started from it.
    class manual_clock {
    public:
        manual_clock() = default;
        manual_clock(const manual_clock&) = delete;
        manual_clock& operator=(const manual_clock&) = delete;

        ~manual_clock() {
            uninstall();
        }

        // The module's time this clock's, from the steady clock's now
        void install() {
            assert(!detail::manual_clock_installed.load(std::memory_order_relaxed) && "one manual clock at a time");
            detail::manual_clock_now.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_release);
            detail::manual_clock_installed.store(true, std::memory_order_release);
            detail::timers_instance().clock_changed();
            _installed = true;
        }

        // The steady clock the time again
        void uninstall() {
            if (_installed) {
                _installed = false;
                detail::manual_clock_installed.store(false, std::memory_order_release);
                detail::timers_instance().clock_changed();
            }
        }

        bool installed() const noexcept {
            return _installed;
        }

        // The time as this clock has it
        time_point now() const noexcept {
            assert(_installed);
            return time_point(duration(detail::manual_clock_now.load(std::memory_order_acquire)));
        }

        // The time forward by d: the timers due by then fired, the tasks
        // they woke run to their next waits; from a thread that is not a
        // worker, since it waits for the workers to be idle
        void advance(duration d) {
            advance_to(now() + d);
        }

        // The time forward to t (never back)
        void advance_to(time_point t) {
            assert(_installed);
            assert(t >= now());
            detail::wait_for_idle_workers();   // the tasks already running reach their waits: their timers are armed
            detail::manual_clock_now.store(t.time_since_epoch().count(), std::memory_order_release);
            detail::timers_instance().clock_changed();
            detail::wait_for_idle_workers();   // the tasks woken reach theirs
        }

    private:
        bool _installed = false;
    };

    // `co_await sgcl::sleep(d)`: the task suspended for d, no thread held;
    // `sgcl::sleep(d).wait()` blocks the thread, through the clock
    class sleep {
    public:
        explicit sleep(duration d) noexcept
        : _d(d) {
        }

        bool await_ready() const noexcept {
            return _d <= duration::zero();
        }

        template<class P>
        void await_suspend(std::coroutine_handle<P> h) {
            detail::add_sleep(clock::now() + _d, detail::frame_of(h));
        }

        void await_resume() const noexcept {
        }

        void wait() const;

    private:
        duration _d;
    };

    // `co_await sgcl::sleep_until(t)`: the task suspended until t, a point
    // of the module's clock; `sgcl::sleep_until(t).wait()` blocks the thread
    class sleep_until {
    public:
        explicit sleep_until(time_point t) noexcept
        : _t(t) {
        }

        bool await_ready() const noexcept {
            return _t <= clock::now();
        }

        template<class P>
        void await_suspend(std::coroutine_handle<P> h) {
            detail::add_sleep(_t, detail::frame_of(h));
        }

        void await_resume() const noexcept {
        }

        void wait() const;

    private:
        time_point _t;
    };

    // A channel that gets one signal after d and is closed then
    inline tracked_ptr<channel<void>> after(duration d) {
        tracked_ptr<channel<void>> ch = make_tracked<channel<void>>(1);
        detail::add_timer(d, duration::zero(), ch, ch.get());
        return ch;
    }

    // A channel that gets one signal at t (at once, for a t that has passed) and is closed then
    inline tracked_ptr<channel<void>> at(time_point t) {
        tracked_ptr<channel<void>> ch = make_tracked<channel<void>>(1);
        detail::add_timer(t, duration::zero(), ch, ch.get());
        return ch;
    }

    // A channel that gets a signal every d until it is closed
    inline tracked_ptr<channel<void>> tick(duration d) {
        tracked_ptr<channel<void>> ch = make_tracked<channel<void>>(1);
        detail::add_timer(d, d, ch, ch.get());
        return ch;
    }

    // The same with the first tick at `first` (a whole second, say), then every d
    inline tracked_ptr<channel<void>> tick(duration d, time_point first) {
        tracked_ptr<channel<void>> ch = make_tracked<channel<void>>(1);
        detail::add_timer(first, d, ch, ch.get());
        return ch;
    }

    // A thread's sleep goes through a timer, so that the manual clock
    // serves it as it serves a task's; a d of zero or less, or a point
    // that has passed, does not block
    inline void sleep::wait() const {
        if (_d > duration::zero()) {
            after(_d)->receive();
        }
    }

    inline void sleep_until::wait() const {
        if (_t > clock::now()) {
            at(_t)->receive();
        }
    }

    // A case of a select served after d: `timeout(1s, [&] { ... })`
    template<class F>
    class timeout_case
    : public decltype(std::declval<channel<void>&>().on_receive(std::declval<F>())) {
        using Base = decltype(std::declval<channel<void>&>().on_receive(std::declval<F>()));
    public:
        timeout_case(tracked_ptr<channel<void>> ch, F f)
        : Base(ch->on_receive(std::move(f)))
        , _keep(std::move(ch)) {
        }

        timeout_case(timeout_case&& o) noexcept
        : Base(std::move(o))
        , _keep(std::move(o._keep)) {
            o._keep = nullptr;   // a tracked_ptr moved is copied: the source let go of, or its destructor would cancel the timer
        }

        timeout_case(const timeout_case&) = delete;

        // The case gone before its time (the select served by another
        // case, the usual end of a select with a timeout in a loop): the
        // timer cancelled, its channel closed, so that the heap does not
        // keep a timer per select until the deadline (a select loop with
        // a timeout of an hour retained 0.75 KB per iteration, measured)
        ~timeout_case() {
            if (_keep && !_keep->closed()) {
                _keep->close();
                detail::timer_cancelled();
            }
        }

    private:
        tracked_ptr<channel<void>> _keep;   // the channel lives while the case does
    };

    template<class F>
    auto timeout(duration d, F f) {
        return timeout_case<F>(after(d), std::move(f));
    }

    // The same at a point: `timeout(deadline, f)`
    template<class F>
    auto timeout(time_point t, F f) {
        return timeout_case<F>(at(t), std::move(f));
    }
}
