//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../async/channel.h"
#include "../../async/reactor.h"
#include "../../async/timer.h"
#include "../../core/vector.h"
#include "../../core/aliases.h"
#include "../../core/make_tracked.h"
#include "../../core/tracked_ptr.h"
#include "../../core/weak_ptr.h"
#include "../error.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <mutex>
#include <unistd.h>

namespace sgcl::io::detail {
    // How a wait for readiness ended: the descriptor is ready (or the wait
    // was cut short for a reason that is not an error, and the caller
    // looks again), it was closed, its deadline passed, the reactor
    // stopped under it (scheduler::stop), or the reactor cannot watch it
    // (Descriptor::wait_failure says why)
    enum class WaitResult {
        ready,
        closed,
        timed_out,
        cancelled,
        failed
    };

    // A descriptor shared by the tasks and threads that use one file or
    // one socket (io::file, net's sockets):
    // Go's fdMutex and pollDesc in one, written for this reactor.
    //
    // The race it closes: a task reads the number, another closes the
    // descriptor, the kernel gives the number to the next socket opened,
    // and the first task's recv lands on a stranger's connection. So every
    // operation holds the descriptor for as long as it runs (acquire,
    // release: a count in one atomic word), close() only sets the closing
    // bit in that word and wakes the waits in progress, and the ::close
    // itself is made by whoever lets go of the word last, the close or the
    // last operation. After the bit is set no operation starts; the number
    // is the descriptor's own until the ::close.
    //
    // The wait of an operation is on the reactor's slot for the number
    // (reactor.h: PollSlot): registered with the kernel once, at the
    // first wait in a direction, edge-triggered, and dropped from the
    // slot right before the ::close (Reactor::release). An operation
    // tries its call first and waits only on EAGAIN; the wait parks the
    // task (or the thread) in the slot's word for the direction, and an
    // edge that came after the call's try is kept there, so the wait
    // then answers at once and the call is tried again. A wait woken
    // without readiness (a close, the deadline of its direction passing:
    // the direction's timer, one per descriptor, _arm; a deadline changed
    // while it waits)
    // tells the causes apart afterwards: closing is a close, a deadline in
    // the past a timeout, a generation moved (every change of a deadline
    // moves it) a look again, the reactor's incarnation moved its stop;
    // none of these is a readiness.
    //
    // A cause is set first and the waits are woken after it (_wake_waits:
    // the slot's interrupt); a wait publishes itself and looks at the
    // causes after it; both sequentially consistent: either the waker finds
    // the wait parked, or the wait sees the cause and takes itself back.
    //
    // The descriptor lives inside the managed object of its file or
    // socket; its destructor, on the collector's thread after the sweep
    // that finds the owner dead, closes a descriptor nobody closed. A
    // parked task keeps its frame, and so the owner, alive through the
    // slot's roots while it waits. One that is not owned (the standard
    // streams) is never given back to the kernel: a close only ends the
    // operations on it, and its registration stays with the open number.
    class Descriptor {
        using PollSlot = sgcl::async::detail::PollSlot;
        using PollWaiter = sgcl::async::detail::PollWaiter;
        using Timer = sgcl::async::detail::Timer;

    public:
        static constexpr int Read = 0;
        static constexpr int Write = 1;

    private:
        // What a wait saw before it parked, for its look after
        struct Parking {
            uint64_t generation = 0;
            uint64_t incarnation = 0;
            PollSlot* slot = nullptr;
            int dir = Read;
        };

    public:
        explicit Descriptor(int fd, bool owns = true) noexcept
        : _fd(fd)
        , _owns(owns) {
        }

        Descriptor(const Descriptor&) = delete;
        Descriptor& operator=(const Descriptor&) = delete;

        ~Descriptor() {
            _drop_timers();   // an abandoned descriptor's timers: cancelled, swept from the heaps (they held it weakly)
            if (!(_state.load(std::memory_order_acquire) & Closed) && _owns) {
                sgcl::async::detail::reactor_instance().release(_fd);
                ::close(_fd);
            }
        }

        int fd() const noexcept {
            return _fd;
        }

        // An operation begins: false once close() has been called
        bool acquire() noexcept {
            uint64_t s = _state.load(std::memory_order_relaxed);
            do {
                if (s & Closing) {
                    return false;
                }
            } while (!_state.compare_exchange_weak(s, s + 1, std::memory_order_seq_cst, std::memory_order_relaxed));
            return true;
        }

        // The operation ends; the last one after a close closes
        void release() noexcept {
            (void)_release();
        }

        // Whether close() has been called
        bool closing() const noexcept {
            return (_state.load(std::memory_order_seq_cst) & Closing) != 0;
        }

        // The closing bit set and the waits in progress woken; the ::close
        // now when no operation runs, else by the last to end. Holds the
        // word itself meanwhile, so that the number cannot be closed and
        // given to another descriptor under it. The error of the ::close
        // when it was made here (0 when it went well, or was left to an
        // operation, or was made before)
        int close() noexcept {
            uint64_t s = _state.load(std::memory_order_relaxed);
            do {
                if (s & Closing) {
                    return 0;
                }
            } while (!_state.compare_exchange_weak(s, (s | Closing) + 1, std::memory_order_seq_cst, std::memory_order_relaxed));
            _wake_waits();
            _drop_timers();
            return _release();
        }

        // The deadline of a direction, time_point{} for none; a change
        // wakes the waits in progress, which look at it again
        void set_deadline(int dir, time_point t) {
            _deadline[dir].store(_rep(t), std::memory_order_seq_cst);
            _generation.fetch_add(1, std::memory_order_seq_cst);
            _wake_waits();
        }

        // Why a wait answered failed: the errno of the kernel's refusal to
        // register the descriptor, or io::errc::unsupported for a number
        // past the reactor's table. Never a readiness at once: a call
        // that answers EAGAIN on a descriptor the reactor cannot watch
        // would be tried again forever.
        error_code wait_failure() const noexcept {
            int e = _failure.load(std::memory_order_relaxed);
            return e ? error_code(e, std::system_category()) : make_error_code(io::errc::unsupported);
        }

        time_point deadline(int dir) const noexcept {
            return _point(_deadline[dir].load(std::memory_order_seq_cst));
        }

        // Whether the direction's deadline is set and has passed
        bool expired(int dir) const noexcept {
            auto d = _deadline[dir].load(std::memory_order_seq_cst);
            return d != 0 && sgcl::clock::now() >= _point(d);
        }

        // Before every try of a call that may answer EAGAIN: a readiness
        // kept from before the try is dropped, since the try sees it
        // (reactor.h: PollSlot::reset); nothing before the first wait
        void prepare(int dir) noexcept {
            if (auto s = _slot.load(std::memory_order_relaxed)) {
                s->reset(dir);
            }
        }

        // One wait for the direction's readiness, after a call that
        // answered EAGAIN, on this thread. `deadline_looked`: the caller
        // looked at the deadline just before its call (SocketConn's try),
        // so the start of the wait does not read the clock again; a
        // deadline that passes meanwhile is the deadline's timer's, armed
        // by the wait, which wakes it as it wakes every wait past it
        WaitResult wait(int dir, bool deadline_looked = false) {
            Parking p;
            if (auto r = _begin(dir, p, !deadline_looked)) {
                return *r;
            }
            PollSlot& s = *p.slot;
            auto c = s.claim(dir);
            if (c == PollSlot::Claimed::ready) {
                return WaitResult::ready;
            }
            tracked_ptr<PollWaiter> me = make_tracked<PollWaiter>();   // the thread's word, held by this stack while it waits
            bool main = c == PollSlot::Claimed::ours;
            _arm(dir);
            if (main) {
                s.waiter(dir) = me;
                if (!s.publish(dir)) {
                    return WaitResult::ready;
                }
            } else if (!s.push(dir, me)) {
                return WaitResult::ready;
            }
            if (_changed(p) && (main ? s.take_back(dir) : s.remove(dir, me.get()))) {
                return _result(p);
            }
            me->signal.wait(0, std::memory_order_acquire);   // the waker's release store of 1
            return _result(p);
        }

        // The same in a task: `co_await d.async_wait(dir)`, the task
        // suspended and the worker given back meanwhile. Nothing of the
        // awaiter (it lives in the task's frame) is touched after the
        // publication, from which the task may be resumed on another
        // worker; what the look after it needs is copied out before.
        class wait_op {
        public:
            bool await_ready() {
                if (auto r = _d->_begin(_p.dir, _p, _look)) {
                    _result = *r;
                    return true;
                }
                return false;
            }

            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) {
                PollSlot& s = *_p.slot;
                int dir = _p.dir;
                auto c = s.claim(dir);
                if (c == PollSlot::Claimed::ready) {
                    _result = WaitResult::ready;
                    return false;
                }
                _d->_arm(dir);
                Descriptor* d = _d;
                Parking p = _p;
                tracked_ptr<PollWaiter> behind;
                _suspended = true;
                if (c == PollSlot::Claimed::ours) {
                    s.frame(dir) = sgcl::detail::frame_of(h);
                    if (!s.publish(dir)) {
                        return _not_parked(WaitResult::ready);
                    }
                } else {
                    behind = make_tracked<PollWaiter>();
                    behind->frame = sgcl::detail::frame_of(h);
                    if (!s.push(dir, behind)) {
                        behind->frame = nullptr;
                        return _not_parked(WaitResult::ready);
                    }
                }
                // published: from here on only the locals
                if (d->_changed(p)) {
                    if (!behind ? s.take_back(dir) : s.remove(dir, behind.get())) {
                        if (behind) {
                            behind->frame = nullptr;
                        }
                        return _not_parked(d->_result(p));   // taken back: the frame is this call's again
                    }
                }
                return true;
            }

            WaitResult await_resume() {
                if (_suspended) {
                    return _d->_result(_p);
                }
                return _result;
            }

        private:
            friend class Descriptor;

            wait_op(Descriptor& d, int dir, bool look) noexcept
            : _d(&d)
            , _look(look) {
                _p.dir = dir;
            }

            bool _not_parked(WaitResult r) {
                _suspended = false;
                _result = r;
                return false;
            }

            Descriptor* _d;
            Parking _p;
            WaitResult _result = WaitResult::ready;
            bool _look = true;
            bool _suspended = false;
        };

        wait_op async_wait(int dir, bool deadline_looked = false) noexcept {
            return wait_op(*this, dir, !deadline_looked);
        }

        // The wait as a channel, for a select with another case (a stop
        // token): begin_wait parks a waiter whose wake closes the channel
        // (signalled first when ready), and answers at once (done) when
        // the descriptor is closing, the deadline has passed, readiness is
        // there already, or a cause came meanwhile; end_wait, after the
        // select, takes the waiter back if nothing woke it, and says why
        // the wait ended.
        //
        //     auto w = d.begin_wait(dir);
        //     if (!w.done) { co_await select(w.channel->on_receive(...), stop.on_stop(...)); }
        //     auto r = w.done ? w.result : d.end_wait(w);
        struct Wait {
            tracked_ptr<async::detail::ChannelState<void>> channel;
            tracked_ptr<PollWaiter> waiter;
            Parking parking;
            bool main = false;
            bool done = false;
            WaitResult result = WaitResult::ready;
        };

        Wait begin_wait(int dir) {
            Wait w;
            if (auto r = _begin(dir, w.parking)) {
                return _early(w, *r);
            }
            PollSlot& s = *w.parking.slot;
            auto c = s.claim(dir);
            if (c == PollSlot::Claimed::ready) {
                return _early(w, WaitResult::ready);
            }
            w.main = c == PollSlot::Claimed::ours;
            w.channel = make_tracked<async::detail::ChannelState<void>>(1);
            w.waiter = make_tracked<PollWaiter>();
            w.waiter->keep = w.channel;
            w.waiter->ch = w.channel.get();
            _arm(dir);
            if (w.main) {
                s.waiter(dir) = w.waiter;
                if (!s.publish(dir)) {
                    return _early(w, WaitResult::ready);
                }
            } else if (!s.push(dir, w.waiter)) {
                return _early(w, WaitResult::ready);
            }
            if (_changed(w.parking) && _take_back(w)) {
                return _early(w, _result(w.parking));
            }
            return w;
        }

        WaitResult end_wait(Wait& w) {
            if (!w.channel->closed()) {
                (void)_take_back(w);   // not woken (the other case won): taken back, or its wake is on the way and closes a channel nobody reads
            }
            return _result(w.parking);
        }

    private:
        static constexpr uint64_t Closing = uint64_t(1) << 63;
        static constexpr uint64_t Closed = uint64_t(1) << 62;
        static constexpr uint64_t CountMask = Closed - 1;

        // Before a park: the answers at once (a deadline passed, a close,
        // no reactor, a number the reactor has no slot for or cannot register: failed, wait_failure), else the
        // slot, registered for the direction
        optional<WaitResult> _begin(int dir, Parking& p, bool look_deadline = true) {
            p.dir = dir;
            p.generation = _generation.load(std::memory_order_seq_cst);
            auto dl = _deadline[dir].load(std::memory_order_seq_cst);
            if (look_deadline && dl != 0 && sgcl::clock::now() >= _point(dl)) {
                return WaitResult::timed_out;
            }
            if (closing()) {
                return WaitResult::closed;
            }
            auto& reactor = sgcl::async::detail::reactor_instance();
            p.incarnation = reactor.running();
            if (!p.incarnation) {
                return WaitResult::cancelled;   // no queue: the wait ends with nothing, and the next one tries again
            }
            p.slot = reactor.slot(_fd);
            if (!p.slot) {
                _failure.store(0, std::memory_order_relaxed);
                return WaitResult::failed;
            }
            _slot.store(p.slot, std::memory_order_relaxed);   // the slot's memory is never given back: any thread may read it through a stale copy
            if (int e = reactor.arm(_fd, *p.slot, dir, p.incarnation)) {
                _failure.store(e, std::memory_order_relaxed);   // read by the same thread, from the result
                return WaitResult::failed;
            }
            return nullopt;
        }

        // The look after the publication: a cause set since the look before
        bool _changed(const Parking& p) const noexcept {
            return closing() || _generation.load(std::memory_order_seq_cst) != p.generation || sgcl::async::detail::reactor_instance().live() != p.incarnation;
        }

        // Why a wait ended, woken or taken back
        WaitResult _result(const Parking& p) const noexcept {
            if (closing()) {
                return WaitResult::closed;
            }
            if (expired(p.dir)) {
                return WaitResult::timed_out;
            }
            if (_generation.load(std::memory_order_seq_cst) != p.generation) {
                return WaitResult::ready;   // a deadline changed: look again
            }
            if (sgcl::async::detail::reactor_instance().live() != p.incarnation) {
                return WaitResult::cancelled;   // the reactor stopped (scheduler::stop)
            }
            return WaitResult::ready;
        }

        bool _take_back(Wait& w) {
            PollSlot& s = *w.parking.slot;
            return w.main ? s.take_back(w.parking.dir) : s.remove(w.parking.dir, w.waiter.get());
        }

        // The timer of the direction's deadline, one per descriptor and
        // direction, as Go's pollDesc has its rt and wt: armed by the first
        // wait under a deadline, and kept while the deadline only moves
        // later, which is what a kept connection does (the idle deadline
        // set again at every request: a timer made per wait was an
        // allocation and a heap's add per request). A timer armed at or
        // before the deadline serves it: when it fires before the deadline
        // it arms the rest (_expire), as Go's modtimer moves a timer
        // later lazily. A deadline moved earlier than the armed timer gets
        // a timer of its own at the next wait, the old one cancelled. No
        // timer while nobody waits: a deadline set and never waited under
        // costs nothing.
        void _arm(int dir) {
            auto dl = _deadline[dir].load(std::memory_order_seq_cst);
            if (dl == 0) {
                return;
            }
            auto a = _armed_when[dir].load(std::memory_order_acquire);
            if (a != 0 && a <= dl) [[likely]] {
                return;
            }
            std::lock_guard lock(_timers);
            a = _armed_when[dir].load(std::memory_order_relaxed);
            if (a == 0 || a > dl) {
                _arm_at(dir, dl);
            }
        }

        // Under _timers: a timer at dl in place of the armed one. The timer
        // holds the descriptor weakly: a connection abandoned without a
        // close is collected, and its number closed by the destructor, as
        // before, not kept until its deadline; the timer then runs out into
        // nothing. The weak pointer made once, at the first arming.
        void _arm_at(int dir, int64_t dl) {
            _drop_timer(dir);
            if (_self.expired()) {
                _self = tracked_ptr<void>(this);
            }
            _armed[dir] = sgcl::async::detail::add_weak_timer(_point(dl), _self, dir == Read ? &Descriptor::_expire_read : &Descriptor::_expire_write);
            _armed_when[dir].store(dl, std::memory_order_release);   // after the timer is in: a wait that sees it relies on a timer that exists
        }

        // Under _timers: the armed timer cancelled and forgotten
        void _drop_timer(int dir) noexcept {
            if (auto& t = _armed[dir]) {
                t->cancelled.store(true, std::memory_order_release);
                sgcl::async::detail::timer_cancelled(*t);
                t = nullptr;
            }
            _armed_when[dir].store(0, std::memory_order_release);
        }

        // Both timers cancelled: the close, which ends every wait and
        // every deadline
        void _drop_timers() noexcept {
            std::lock_guard lock(_timers);
            _drop_timer(Read);
            _drop_timer(Write);
        }

        int _release() noexcept {
            uint64_t s = _state.fetch_sub(1, std::memory_order_seq_cst) - 1;
            if ((s & Closing) && (s & CountMask) == 0) {
                return _close_now();
            }
            return 0;
        }

        // Once: the slot dropped by the reactor and the number given back
        // to the kernel, in that order (reactor.h: Reactor::release); the
        // slot of a number not owned stays registered with it
        int _close_now() noexcept {
            if (_state.fetch_or(Closed, std::memory_order_acq_rel) & Closed) {
                return 0;
            }
            if (!_owns) {
                return 0;
            }
            sgcl::async::detail::reactor_instance().release(_fd);
            return ::close(_fd) == 0 ? 0 : errno;
        }

        static Wait _early(Wait& w, WaitResult r) noexcept {
            w.done = true;
            w.result = r;
            return w;
        }

        // The waits in progress woken without readiness, their cause set
        // before this (a close, a deadline changed or passed)
        void _wake_waits() noexcept {
            if (auto s = sgcl::async::detail::reactor_instance().find(_fd)) {
                s->interrupt(Read);
                s->interrupt(Write);
            }
        }

        // The timer of a direction's deadline, on the timer's thread. The
        // armed one is due (a fire that finds another armed, later than
        // now, is of a timer replaced as it fired: nothing to do). The
        // deadline passed: the waits woken, and they see it (a wait that
        // began after the deadline moved sees the deadline it has, and
        // takes no wake as a timeout: _result reads the deadline, not the
        // wake). The deadline moved later meanwhile: the rest armed,
        // nobody woken. No deadline any more: nothing.
        void _expire(int dir) {
            auto now = _rep(sgcl::clock::now());
            int64_t dl;
            {
                std::lock_guard lock(_timers);
                auto a = _armed_when[dir].load(std::memory_order_relaxed);
                if (a == 0 || a > now) {
                    return;
                }
                _armed[dir] = nullptr;
                _armed_when[dir].store(0, std::memory_order_release);
                dl = _deadline[dir].load(std::memory_order_seq_cst);
                if (dl > now && !closing()) {
                    _arm_at(dir, dl);
                    return;
                }
            }
            if (dl != 0) {
                _generation.fetch_add(1, std::memory_order_seq_cst);
                _wake_waits();
            }
        }

        static void _expire_read(void* self) {
            static_cast<Descriptor*>(self)->_expire(Read);
        }

        static void _expire_write(void* self) {
            static_cast<Descriptor*>(self)->_expire(Write);
        }

        static int64_t _rep(time_point t) noexcept {
            return std::chrono::nanoseconds(t.time_since_epoch()).count();
        }

        static time_point _point(int64_t rep) noexcept {
            return time_point() + std::chrono::nanoseconds(rep);
        }

        std::atomic<uint64_t> _state = {0};         // closing, closed, and the operations in progress
        std::atomic<uint64_t> _generation = {0};    // moved by a deadline changed or passed
        std::atomic<int64_t> _deadline[2] = {{0}, {0}};
        std::atomic<PollSlot*> _slot = {nullptr};   // the reactor's slot for the number, once a wait has found it (prepare)
        std::atomic<int> _failure = {0};            // the errno of the last failed registration, 0 for a number past the table
        std::atomic<int64_t> _armed_when[2] = {{0}, {0}};   // the point of the armed timer of a direction, 0 for none (_arm)
        tracked_ptr<Timer> _armed[2];                // the armed timers (under _timers)
        weak_ptr<void> _self;                        // this descriptor, for its timers (under _timers)
        std::mutex _timers;                          // the arming, the fire and the close of the timers: none of them on a wait's usual path
        const int _fd;
        const bool _owns;
    };

    // An operation's hold on the descriptor, for a scope: false (and no
    // hold) when the descriptor is closing
    class Operation {
    public:
        explicit Operation(Descriptor& d) noexcept
        : _d(&d)
        , _held(d.acquire()) {
        }

        Operation(const Operation&) = delete;
        Operation& operator=(const Operation&) = delete;

        ~Operation() {
            if (_held) {
                _d->release();
            }
        }

        explicit operator bool() const noexcept {
            return _held;
        }

    private:
        Descriptor* _d;
        bool _held;
    };
}
