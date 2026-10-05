//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/clock.h"
#include "../core/duration.h"
#include "../core/root_ptr.h"
#include "../core/weak_ptr.h"
#include "channel.h"
#include "event.h"
#include "scheduler.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <limits>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
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
    // The time is read in one place, `clock::now()` (core/clock.h): the
    // steady clock, unless a test has installed a manual_clock (below),
    // whose time moves only when the test advances it, every timer due
    // firing then, in order, with no real waiting.
    //
    // A span of time is sgcl::duration (core/duration.h), a point
    // sgcl::time_point (core/clock.h), the steady clock's.

    namespace detail {
        class Timers;
        inline Timers& timers_instance();
        inline void timer_cancelled() noexcept;

        struct Timer {
            time_point when;
            duration period = duration::zero();         // a tick: fired again every period
            tracked_ptr<FrameWord> frame;               // a sleep: the coroutine to resume
            tracked_ptr<void> keep;                     // an after, a tick or a call: the object that holds the channel or is called, alive while the timer is
            ChannelState<void>* ch = nullptr;                // the channel to signal, inside it
            void (*fire)(void*) = nullptr;              // a call: run on the timer's thread with `keep` (a stop_source's deadline, a timeout's), or with what `weak` holds
            weak_ptr<void> weak;                        // a call on an object the timer does not keep alive (a descriptor's deadline): skipped once the object is gone
            bool weakly = false;                        // the call is on `weak`
            atomic<bool> cancelled = {false};           // by its owner (a race its task won): not fired, swept out of the heap with the closed ones
            uint8_t shard = 0;                          // the heap it lives in (Timers), for the count of the cancelled
        };

        // Called by the timer thread between its pass over the heaps and
        // its sleep, when set: a test's way to make an add at that moment
        inline std::atomic<void (*)()> timers_test_hook = {nullptr};

        // The timers in shards, one per worker and one for every other
        // thread (a thread blocked in a call, the reactor's, the main
        // thread; a 64th worker shares it): a heap under a lock of its
        // own each, so that a worker's add takes a lock nobody else
        // takes but the timer thread's pass (a timer per wait with a
        // deadline, the HTTP server's: one mutex for all of them was 4
        // per cent of the workers' busy time at 24 workers, measured).
        // One thread still fires them all: nothing in the scheduler
        // changes (Go keeps a heap per P, fired in the P's loop, which
        // takes a park with a timeout and a look at the others' heaps
        // when stealing; the timers of waits that end in time never
        // fire, and what costs is the add and the cancel).
        //
        // A bit per shard says it may hold timers: set by an add after
        // its push, under the shard's lock; cleared by the thread only
        // under the same lock, after finding the heap empty; so an add
        // that has just pushed is never left behind a cleared bit. The
        // thread's pass looks at the shards whose bits are set.
        //
        // The wake: _next is the point the thread sleeps to. An add
        // lowers it with a compare-exchange when its timer is earlier,
        // and then notifies under the thread's lock. The thread, before
        // a pass, raises it to Never; after the pass, under its lock, it
        // takes the lower of what the pass found and what adds lowered it
        // to meanwhile, publishes that, and sleeps to it, the lock held
        // from the look to the wait. An add whose push came before the
        // pass looked at its shard is in the pass; one whose push came
        // after lowered _next after the thread raised it (the shard's
        // lock orders the push after the pass, and the pass after the
        // raise); one that lowers it after the look notifies under the
        // lock, which the thread holds until it waits. Sequentially
        // consistent throughout: _next is the one word both sides meet
        // at.
        class Timers {
        public:
            static constexpr unsigned ShardCount = Scheduler::MaxWorkers;   // the last one every thread that is no worker shares

            SGCL_INLINE_HOT Timers() {
                scheduler_instance();   // made after the scheduler, so destroyed before it: a timer firing at exit finds the scheduler, not a destroyed one
            }

            SGCL_INLINE_HOT ~Timers() {
                stop();
            }

            void add(root_ptr<Timer> t) {
                if (!_started.load(std::memory_order_acquire)) [[unlikely]] {
                    if (!_start()) {
                        _refused(t);
                        return;
                    }
                }
                unsigned s = std::min(Scheduler::worker_index(), ShardCount - 1);
                t->shard = uint8_t(s);
                auto when = t->when;
                Shard& sh = _shards[s];
                {
                    std::lock_guard lock(sh.m);
                    _push(sh, s, std::move(t));
                }
                _lower(when);
            }

            // The thread joined; the timers not yet due stay in the heaps and
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
                std::lock_guard lock(_m);
                _running = false;   // a timer added during the join stays in its heap, as one not yet due does, and fires when the next add starts the thread again
                _started.store(false, std::memory_order_release);
                _next.store(Never, std::memory_order_seq_cst);
                for (auto& sh : _shards) {   // the ticks whose channel was closed: gone with the thread, their channels with them
                    std::lock_guard shard(sh.m);
                    _sweep(sh);
                }
                _settled = _epoch;          // an advance waiting for the thread: nothing more will fire
                _settled_cv.notify_all();
            }

            // A timer cancelled by its owner: counted in its shard, for the sweep in add()
            SGCL_INLINE_HOT void cancelled(const Timer& t) noexcept {
                _shards[t.shard].dead.fetch_add(1, std::memory_order_relaxed);
            }

            // One cancelled without its timer at hand (a timeout case's
            // channel closed): counted in the calling thread's shard, an
            // estimate; a heap is swept anyway once it doubles (_push)
            SGCL_INLINE_HOT void cancelled() noexcept {
                _shards[std::min(Scheduler::worker_index(), ShardCount - 1)].dead.fetch_add(1, std::memory_order_relaxed);
            }

            // The timers in the heaps, for the tests
            size_t size() {
                size_t n = 0;
                for (auto& sh : _shards) {
                    std::lock_guard lock(sh.m);
                    n += sh.heap.size();
                }
                return n;
            }

            // The timers in each heap, for the tests: a shard sweeps its
            // cancelled once it holds sweep_at timers (64 at least, twice
            // what was live at its last sweep), so a loop of won races
            // leaves each shard it went through below max(64, twice its
            // live timers and one); the live ones may be other tests' (a
            // deadline an hour away), which is why a test compares each
            // shard with what it held before, not with a number
            std::vector<size_t> shard_sizes() {
                std::vector<size_t> sizes;
                for (auto& sh : _shards) {
                    std::lock_guard lock(sh.m);
                    sizes.push_back(sh.heap.size());
                }
                return sizes;
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
            static constexpr time_point::rep Never = std::numeric_limits<time_point::rep>::max();

            struct alignas(64) Shard {
                std::mutex m;
                std::vector<root_ptr<Timer>> heap;
                std::atomic<size_t> dead = {0};   // the timers cancelled since the last sweep (an estimate: a case gone after its timer fired counts too)
                size_t sweep_at = 64;             // the size at which the heap is swept whatever the count says
            };

            SGCL_INLINE_HOT static bool _later(const root_ptr<Timer>& a, const root_ptr<Timer>& b) noexcept {
                return a->when > b->when;
            }

            SGCL_INLINE_HOT static time_point::rep _rep(time_point t) noexcept {
                return t.time_since_epoch().count();
            }

            // Under the shard's lock: the cancelled swept out once they are
            // half the heap, or once the heap doubled since the last sweep
            // (amortized nothing per add); then the timer in, and the bit set
            SGCL_INLINE_HOT void _push(Shard& sh, unsigned s, root_ptr<Timer> t) noexcept {
                if (sh.heap.size() >= sh.sweep_at || (sh.heap.size() > 64 && sh.dead.load(std::memory_order_relaxed) > sh.heap.size() / 2)) {
                    _sweep(sh);
                }
                sh.heap.push_back(std::move(t));
                std::push_heap(sh.heap.begin(), sh.heap.end(), _later);
                _nonempty.fetch_or(uint64_t(1) << s, std::memory_order_release);
            }

            SGCL_INLINE_HOT static void _sweep(Shard& sh) noexcept {
                std::erase_if(sh.heap, [](const root_ptr<Timer>& t) { return t->cancelled.load(std::memory_order_acquire) || (t->ch && t->ch->closed()); });
                std::make_heap(sh.heap.begin(), sh.heap.end(), _later);
                sh.dead.store(0, std::memory_order_relaxed);
                sh.sweep_at = std::max<size_t>(64, 2 * sh.heap.size());
            }

            // _next lowered to the timer's point when that is earlier, and
            // the thread told
            void _lower(time_point when) {
                auto w = _rep(when);
                auto n = _next.load(std::memory_order_seq_cst);
                while (w < n) {
                    if (_next.compare_exchange_weak(n, w, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                        std::lock_guard lock(_m);   // the thread reads _next under this lock right before it sleeps
                        _cv.notify_one();
                        return;
                    }
                }
            }

            // The thread started, unless the program is ending (false:
            // scheduler.h, runtime_exit)
            bool _start() {
                RuntimeStart starting;
                if (!starting) {
                    return false;
                }
                std::lock_guard lock(_m);
                if (!_running) {
                    _stop = false;
                    _running = true;
                    _thread = std::thread([this] { _run(); });
                    scheduler_stop_hook.store([] { timers_instance().stop(); }, std::memory_order_release);   // scheduler::stop() stops the timers too
                }
                _started.store(true, std::memory_order_release);
                return true;
            }

            // A timer added once the program is ending, with no thread to
            // fire it: never armed. A task waiting on it stays parked, as a
            // task parked at exit does; a thread's wait on a channel (a
            // sleep, an after, a timeout) ends at once, the channel closed,
            // rather than never
            static void _refused(const root_ptr<Timer>& t) {
                if (!t->frame && !t->fire && t->ch && !Scheduler::on_worker()) {
                    t->ch->close();
                }
            }

            void _run() {
                const uintptr_t floor = dead_stack_floor();
                std::unique_lock lock(_m);
                for (;;) {
                    if (_stop) {
                        return;
                    }
                    uint64_t epoch = _epoch;
                    lock.unlock();
                    _next.store(Never, std::memory_order_seq_cst);   // from here an add lowers it, whatever the pass below misses
                    auto now = clock::now();
                    auto earliest = _pass(now);
                    if (auto hook = timers_test_hook.load(std::memory_order_relaxed)) [[unlikely]] {
                        hook();   // tests/async/timer.cpp: an add between the pass and the sleep
                    }
                    lock.lock();
                    if (_stop) {
                        return;
                    }
                    auto target = _rep(earliest);
                    auto seen = _next.load(std::memory_order_seq_cst);
                    while (!_next.compare_exchange_weak(seen, std::min(target, seen), std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                    }
                    target = std::min(target, seen);   // published: an add later than it leaves the thread asleep, an earlier one waits for this lock to notify
                    if ((target <= _rep(now) && target != Never) || _epoch != epoch) {   // a point at max() never fires, not even at the end of time
                        continue;   // due already (an add of a point passed, a tick behind), or the clock moved under the pass
                    }
                    if (_settled != epoch) {   // nothing due at this time: whoever changed the clock may go on
                        _settled = epoch;
                        _settled_cv.notify_all();
                    }
                    clear_dead_stack(floor);   // the words the pass's frames left (a timer, what it kept, a frame it woke): scheduler.h
                    if (target == Never || manual_clock_installed.load(std::memory_order_relaxed)) {
                        _cv.wait(lock);         // under a manual clock time moves only by an advance, which wakes the thread
                    } else {
                        _cv.wait_until(lock, time_point(time_point::duration(target)));
                    }
                }
            }

            // Every timer due at `now` fired, in their order (the due ones
            // gathered from the shards into a heap of the pass's own, a
            // tick due again before `now` back into it), the shards found
            // empty cleared from the mask; the earliest point left
            SGCL_NOINLINE time_point _pass(time_point now) {   // a frame of its own below _run's, in the dead stack clear_dead_stack zeroes
                time_point earliest = time_point::max();
                uint64_t mask = _nonempty.load(std::memory_order_acquire);
                while (mask) {
                    unsigned s = unsigned(std::countr_zero(mask));
                    mask &= mask - 1;
                    Shard& sh = _shards[s];
                    std::lock_guard lock(sh.m);
                    while (!sh.heap.empty() && sh.heap.front()->when <= now && sh.heap.front()->when != time_point::max()) {   // a timer at max() never fires: now() + duration::max() saturates to it (Go's when() cuts the same way)
                        std::pop_heap(sh.heap.begin(), sh.heap.end(), _later);
                        _due.push_back(std::move(sh.heap.back()));
                        std::push_heap(_due.begin(), _due.end(), _later);
                        sh.heap.pop_back();
                    }
                    if (sh.heap.empty()) {
                        _nonempty.fetch_and(~(uint64_t(1) << s), std::memory_order_relaxed);   // under the lock, the heap found empty: an add pushes under it too
                    } else {
                        earliest = std::min(earliest, sh.heap.front()->when);
                    }
                }
                while (!_due.empty()) {
                    std::pop_heap(_due.begin(), _due.end(), _later);
                    root_ptr<Timer> t = std::move(_due.back());
                    _due.pop_back();
                    _fire(t);
                    if (t->period != duration::zero() && !t->ch->closed()) {
                        t->when = t->when + t->period;   // saturated: a tick past the end of time stops at max(), which never fires
                        if (t->when <= now && t->when != time_point::max()) {
                            _due.push_back(std::move(t));
                            std::push_heap(_due.begin(), _due.end(), _later);
                        } else {
                            earliest = std::min(earliest, t->when);
                            unsigned s = t->shard;
                            Shard& sh = _shards[s];
                            std::lock_guard lock(sh.m);
                            _push(sh, s, std::move(t));
                        }
                    }
                }
                return earliest;
            }

            static void _fire(const root_ptr<Timer>& t) {
                if (t->cancelled.load(std::memory_order_acquire)) {
                    return;
                }
                if (t->frame) {
                    enqueue(t->frame, false);
                } else if (t->fire) {
                    if (!t->weakly) {
                        t->fire(t->keep.get());
                    } else if (auto held = t->weak.lock()) {   // held through the call; gone: the timer ran out into nothing
                        t->fire(held.get());
                    }
                } else if (t->period != duration::zero()) {
                    t->ch->try_send();   // a tick nobody took is dropped
                } else {
                    t->ch->close();   // after, at: the event set: set is the close, one event; a wait woken by it finds it set
                }
            }

            Shard _shards[ShardCount];
            std::atomic<uint64_t> _nonempty = {0};         // a bit per shard that may hold timers
            std::atomic<time_point::rep> _next = {Never};  // the point the thread sleeps to
            std::atomic<bool> _started = {false};          // the thread started (an add's look without the lock)
            std::vector<root_ptr<Timer>> _due;             // the pass's heap of the due timers (the thread's alone)
            std::mutex _m;
            std::condition_variable _cv;
            std::condition_variable _settled_cv;
            std::thread _thread;
            bool _stop = false;
            bool _running = false;   // the thread started and not yet joined (under _m: the thread object itself is not looked at by two threads)
            uint64_t _epoch = 0;     // bumped by every change of the clock
            uint64_t _settled = 0;   // the epoch the thread last found nothing due at
        };

        // The singleton: its destruction at exit ends the runtime first
        // (scheduler.h: runtime_exit)
        inline Timers& timers_instance() {
            struct Instance : Timers {
                ~Instance() {
                    runtime_exit();
                }
            };
            static Instance timers;
            return timers;
        }

        SGCL_INLINE_HOT void timer_cancelled() noexcept {
            timers_instance().cancelled();
        }

        SGCL_INLINE_HOT void timer_cancelled(const Timer& t) noexcept {
            timers_instance().cancelled(t);
        }

        // A timer on a channel: one signal at `when` and the close (period
        // zero), or a signal at `when` and every period after until the
        // channel is closed
        inline void add_timer(time_point when, duration period, const tracked_ptr<void>& keep, ChannelState<void>* ch) {
            root_ptr<Timer> t = make_tracked<Timer>();
            t->when = when;
            t->period = period;
            t->keep = keep;
            t->ch = ch;
            timers_instance().add(std::move(t));
        }

        SGCL_INLINE_HOT void add_timer(duration d, duration period, const tracked_ptr<void>& keep, ChannelState<void>* ch) {
            add_timer(clock::now() + d, period, keep, ch);
        }

        // A call at `when`, on the timer's thread, with the object kept:
        // what does more than a channel can (a stop_source's deadline:
        // the close and the children in one step, so that the stop is
        // requested before anyone woken by it looks)
        inline tracked_ptr<Timer> add_timer(time_point when, const tracked_ptr<void>& keep, void (*fire)(void*)) {
            root_ptr<Timer> t = make_tracked<Timer>();
            t->when = when;
            t->keep = keep;
            t->fire = fire;
            tracked_ptr<Timer> handle(t);   // for the owner that may cancel it
            timers_instance().add(std::move(t));
            return handle;
        }

        // The same on an object the timer does not keep alive: the call made
        // only if the object is still there when the time comes (a
        // descriptor's deadline, whose timer outlives its waits: a timer
        // that kept it would keep an abandoned connection, and its
        // descriptor number, until the deadline)
        inline tracked_ptr<Timer> add_weak_timer(time_point when, const weak_ptr<void>& target, void (*fire)(void*)) {
            root_ptr<Timer> t = make_tracked<Timer>();
            t->when = when;
            t->weak = target;
            t->weakly = true;
            t->fire = fire;
            tracked_ptr<Timer> handle(t);
            timers_instance().add(std::move(t));
            return handle;
        }

        SGCL_INLINE_HOT tracked_ptr<Timer> add_timer(duration d, const tracked_ptr<void>& keep, void (*fire)(void*)) {
            return add_timer(clock::now() + d, keep, fire);
        }

        // A sleep: the frame resumed at `when`
        SGCL_INLINE_HOT void add_sleep(time_point when, const tracked_ptr<FrameWord>& frame) {
            root_ptr<Timer> t = make_tracked<Timer>();
            t->when = when;
            t->frame = frame;
            timers_instance().add(std::move(t));
        }

        // The workers idle: no task ready, none running, every worker
        // asleep (or no scheduler): every task has reached its next wait.
        // What manual_clock::advance waits for on either side of the move,
        // so that a task woken by a timer arms its next one before the
        // advance returns; a task that waits by spinning, or a thread
        // that blocks a worker, holds the advance
        inline void wait_for_idle_workers() noexcept {
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
    // steady clock's now (and time::now(), the wall time, with it), and
    // moves it only by `advance(d)`, which fires
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

        SGCL_INLINE_HOT ~manual_clock() {
            uninstall();
        }

        // The module's time this clock's, from the steady clock's now
        // noexcept, as the rest: a std::mutex and a std::condition_variable
        // of the timer thread fail only when misused (a lock taken twice),
        // which the module never does
        void install() noexcept {
            assert(!detail::manual_clock_installed.load(std::memory_order_relaxed) && "one manual clock at a time");
            auto start = std::chrono::steady_clock::now().time_since_epoch().count();
            detail::manual_clock_origin.store(start, std::memory_order_relaxed);
            detail::manual_clock_wall_origin.store(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count(), std::memory_order_relaxed);
            detail::manual_clock_now.store(start, std::memory_order_release);
            detail::manual_clock_installed.store(true, std::memory_order_release);
            detail::timers_instance().clock_changed();
            _installed = true;
        }

        // The steady clock the time again
        SGCL_INLINE_HOT void uninstall() noexcept {
            if (_installed) {
                _installed = false;
                detail::manual_clock_installed.store(false, std::memory_order_release);
                detail::timers_instance().clock_changed();
            }
        }

        SGCL_INLINE_HOT bool installed() const noexcept {
            return _installed;
        }

        // The time as this clock has it
        SGCL_INLINE_HOT time_point now() const noexcept {
            assert(_installed);
            return time_point(time_point::duration(detail::manual_clock_now.load(std::memory_order_acquire)));
        }

        // The time forward by d: the timers due by then fired, the tasks
        // they woke run to their next waits; from a thread that is not a
        // worker, since it waits for the workers to be idle
        SGCL_INLINE_HOT void advance(duration d) noexcept {
            advance_to(now() + d);
        }

        // The time forward to t (never back)
        void advance_to(time_point t) noexcept {
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

    // `co_await sgcl::async::sleep(d)`: the task suspended for d, no thread held;
    // `sgcl::async::sleep(d).wait()` blocks the thread, through the clock
    class [[nodiscard]] sleep {
    public:
        SGCL_INLINE_HOT explicit sleep(duration d) noexcept
        : _d(d) {
        }

        SGCL_INLINE_HOT bool await_ready() const noexcept {
            return _d <= duration::zero();
        }

        template<class P>
        SGCL_INLINE_HOT void await_suspend(std::coroutine_handle<P> h) {
            detail::add_sleep(clock::now() + _d, detail::frame_of(h));
        }

        SGCL_INLINE_HOT void await_resume() const noexcept {
        }

        void wait() const;

    private:
        duration _d;
    };

    // `co_await sgcl::async::sleep_until(t)`: the task suspended until t, a point
    // of the module's clock; `sgcl::async::sleep_until(t).wait()` blocks the thread
    class [[nodiscard]] sleep_until {
    public:
        SGCL_INLINE_HOT explicit sleep_until(time_point t) noexcept
        : _t(t) {
        }

        SGCL_INLINE_HOT bool await_ready() const noexcept {
            return _t <= clock::now();
        }

        template<class P>
        SGCL_INLINE_HOT void await_suspend(std::coroutine_handle<P> h) {
            detail::add_sleep(_t, detail::frame_of(h));
        }

        SGCL_INLINE_HOT void await_resume() const noexcept {
        }

        void wait() const;

    private:
        time_point _t;
    };

    namespace detail {
        // The channels of signals under after, at and tick: one signal (or
        // one per period) and, for a single one, the close
        SGCL_INLINE_HOT tracked_ptr<ChannelState<void>> after_state(duration d) {
            tracked_ptr<ChannelState<void>> ch = make_linked_state<void>(1);
            add_timer(d, duration::zero(), ch, ch.get());
            return ch;
        }

        SGCL_INLINE_HOT tracked_ptr<ChannelState<void>> at_state(time_point t) {
            tracked_ptr<ChannelState<void>> ch = make_linked_state<void>(1);
            add_timer(t, duration::zero(), ch, ch.get());
            return ch;
        }
    }

    // An event set after d (event.h: `co_await async::after(d)`,
    // `async::after(d).wait()`, `.on_set(f)` in a select)
    SGCL_INLINE_HOT event after(duration d) {
        return detail::EventAccess::make(detail::after_state(d));
    }

    // An event set at t (at once, for a t that has passed)
    SGCL_INLINE_HOT event at(time_point t) {
        return detail::EventAccess::make(detail::at_state(t));
    }

    // A channel that gets a signal every d until it is closed; a tick
    // nobody has taken yet is dropped (the channel holds one). A period
    // of zero or less never ticks, as Go's time.Tick gives a nil channel
    // for it: no timer is armed (a timer of period zero is a single
    // signal and the close, and one of a negative period would be due
    // again before the time it fired at, the timer thread firing it for
    // good)
    SGCL_INLINE_HOT channel<void> tick(duration d) {
        tracked_ptr<detail::ChannelState<void>> ch = detail::make_linked_state<void>(1);
        if (d > duration::zero()) {
            detail::add_timer(d, d, ch, ch.get());
        }
        return detail::ChannelAccess::make(std::move(ch));
    }

    // The same with the first tick at `first` (a whole second, say), then every d
    SGCL_INLINE_HOT channel<void> tick(duration d, time_point first) {
        tracked_ptr<detail::ChannelState<void>> ch = detail::make_linked_state<void>(1);
        if (d > duration::zero()) {
            detail::add_timer(first, d, ch, ch.get());
        }
        return detail::ChannelAccess::make(std::move(ch));
    }

    // A thread's sleep goes through a timer, so that the manual clock
    // serves it as it serves a task's; a d of zero or less, or a point
    // that has passed, does not block
    SGCL_INLINE_HOT void sleep::wait() const {
        if (_d > duration::zero()) {
            (void)detail::after_state(_d)->receive().wait();
        }
    }

    SGCL_INLINE_HOT void sleep_until::wait() const {
        if (_t > clock::now()) {
            (void)detail::at_state(_t)->receive().wait();
        }
    }

    template<class F>
    class timeout_case;

    template<class F>
    auto timeout(duration d, F f);

    template<class F>
    auto timeout(time_point t, F f);

    // A case of a select served after d: `timeout(1s, [&] { ... })`. Made
    // by timeout() alone, over the timer's channel it keeps
    template<class F>
    class timeout_case
    : public decltype(std::declval<detail::ChannelState<void>&>().on_receive(std::declval<F>())) {
        using Base = decltype(std::declval<detail::ChannelState<void>&>().on_receive(std::declval<F>()));
    public:
        SGCL_INLINE_HOT timeout_case(timeout_case&& o) noexcept
        : Base(std::move(o))
        , _keep(std::move(o._keep)) {
            o._keep = nullptr;   // a tracked_ptr moved is copied: the source let go of, or its destructor would cancel the timer
        }

        timeout_case(const timeout_case&) = delete;

        // The case gone before its time (the select served by another
        // case, the usual end of a select with a timeout in a loop): the
        // timer cancelled, its channel closed, so that the heap does not
        // keep a timer per select until the deadline (a select loop with
        // a timeout of an hour retained 0.75 KB per iteration, measured).
        // noexcept, as a destructor is, and the close cannot throw here:
        // its one throw is a wake's start of the workers, and it wakes
        // nobody. The channel is the case's own (timeout() made it), and
        // its one waiter is the select the case was in, which has ended
        // before the case goes (served or cancelled: no claim succeeds);
        // the timer only sends on it
        SGCL_INLINE_HOT ~timeout_case() {
            if (_keep && !_keep->closed()) {
                _keep->close();
                detail::timer_cancelled();
            }
        }

    private:
        template<class G>
        friend auto timeout(duration d, G f);

        template<class G>
        friend auto timeout(time_point t, G f);

        SGCL_INLINE_HOT timeout_case(tracked_ptr<detail::ChannelState<void>> ch, F f) noexcept(std::is_nothrow_move_constructible_v<F>)
        : Base(ch->on_receive(std::move(f)))
        , _keep(std::move(ch)) {
        }

        tracked_ptr<detail::ChannelState<void>> _keep;   // the channel lives while the case does
    };

    template<class F>
    SGCL_INLINE_HOT auto timeout(duration d, F f) {
        return timeout_case<F>(detail::after_state(d), std::move(f));
    }

    // The same at a point: `timeout(deadline, f)`
    template<class F>
    SGCL_INLINE_HOT auto timeout(time_point t, F f) {
        return timeout_case<F>(detail::at_state(t), std::move(f));
    }
}
