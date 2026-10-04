//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../concurrent/queue.h"
#include "../core/atomic.h"
#include "../core/weak_ptr.h"
#include "channel.h"
#include "timer.h"

#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // Cancellation, the way Go's context has it, under the names of
    // std::stop_source and std::stop_token: a source requests the stop,
    // the tokens handed down see it. A token is a channel of signals
    // closed when the stop is requested, so that a wait is cancelled the
    // way anything else is waited for: a case of a select
    // (`token.on_stop(f)`), or `co_await token.stopped()`. A deadline is a
    // timer that requests the stop (`source.stop_after(d)`). A source made
    // from a token is a child: it stops when the parent stops, and on its
    // own; the parent knows its children through weak pointers, so a
    // child that is gone costs nothing. The state is a managed object; a
    // source or a token is one word, copied freely, alive while any copy
    // is or an armed deadline holds it (a stop cancels the deadline).
    namespace detail {
        struct StopDeadline;

        struct StopState {
            ChannelState<void> signal{ChannelLinked{}, 0};   // closed: the stop requested (linked by stop_source())
            tracked_ptr<StopState> parent;
            concurrent::queue<weak_ptr<StopState>>::first_node children_first;
            concurrent::queue<weak_ptr<StopState>> children{concurrent::detail::QueueUnlinked{}};

            // the channel's lists and the children's queue linked to their
            // first nodes: once, by the code that made the state, right
            // after make_tracked (ChannelState::link)
            SGCL_INLINE_HOT void link() noexcept {
                signal.link();
                children.link(children_first);
            }
            atomic<tracked_ptr<StopDeadline>> deadline;  // the earliest deadline armed, if any: cancelled by the stop

            SGCL_INLINE_HOT bool stopped() const noexcept {
                return signal.closed();
            }

            // The stop; `firing` the deadline whose timer calls it (its
            // timer out of the heap already: nothing to cancel)
            void stop(const StopDeadline* firing = nullptr);
        };

        // A source's deadline: the timer that requests the stop, holding
        // this and not the state, which a cancel lets go of, so that a
        // source stopped by hand is not kept by its timer while the
        // cancelled timer waits in its heap for a sweep (Go's cancel()
        // stops the timer of a WithDeadline the same way)
        struct StopDeadline {
            atomic<tracked_ptr<StopState>> state;        // null: cancelled, the timer's call does nothing
            tracked_ptr<Timer> timer;                    // set before the deadline is published in the state
            time_point when;

            SGCL_INLINE_HOT static void fire(void* p) {
                auto d = static_cast<StopDeadline*>(p);
                if (auto s = d->state.load()) {
                    s->stop(d);
                }
            }

            // The timer cancelled (the flag the timer thread looks at, as a
            // won race does in timeout.h: swept with the other cancelled,
            // nothing done on the stop's path but a store), the state let go of
            SGCL_INLINE_HOT void cancel() noexcept {
                state.store(nullptr);
                timer->cancelled.store(true, std::memory_order_release);
                timer_cancelled(*timer);
            }
        };

        inline void StopState::stop(const StopDeadline* firing) {
            signal.close();
            std::atomic_thread_fence(std::memory_order_seq_cst);   // the close before the look at the children, against a child registering (its push, then its look at the signal): one of the two sees the other (channel.h: _fence); and before the look at the deadline, against one being armed (stop_source::_arm)
            if (auto d = deadline.exchange(nullptr)) {
                if (d.get() != firing) {
                    d->cancel();
                }
            }
            while (auto c = children.try_pop()) {   // the children stopped after the parent
                if (auto child = c->lock()) {
                    child->stop();
                }
            }
        }
    }

    class stop_token {
    public:
        stop_token() noexcept = default;

        SGCL_INLINE_HOT bool stop_requested() const noexcept {
            return _s && _s->stopped();
        }

        SGCL_INLINE_HOT bool stop_possible() const noexcept {
            return (bool)_s;
        }

        // The channel closed by the stop, from its receiving end: a case
        // of a select, or a wait; a handle to the channel inside the
        // stop's state, which it keeps. Receive-only: a close of it would
        // read as a stop that stopped neither the children nor the
        // deadline, and a send would wake a task in stopped() with no stop
        SGCL_INLINE_HOT receive_channel<void> channel() const noexcept {
            assert(_s && "an empty stop_token has no channel: stop_possible() says");
            return detail::ChannelAccess::make(tracked_ptr<detail::ChannelState<void>>(&_s->signal));
        }

        // The case of a select served by the stop: `token.on_stop([&] { running = false; })`
        template<class F>
        SGCL_INLINE_HOT auto on_stop(F f) const noexcept(std::is_nothrow_move_constructible_v<F>) {
            assert(_s && "an empty stop_token has no case: stop_possible() says");
            return _s->signal.on_receive(std::move(f));
        }

        // Waits for the stop, and gives nothing: `co_await token.stopped()`
        // in a task, `token.stopped().wait()` on a thread
        SGCL_INLINE_HOT auto stopped() const noexcept {
            assert(_s && "an empty stop_token has nothing to await: stop_possible() says");
            return detail::make_operation([s = _s](auto how) -> decltype(auto) {
                if constexpr (detail::is_awaited<decltype(how)>) {
                    return stop_wait(s->signal);
                } else {
                    (void)s->signal.receive().wait();
                }
            });
        }

    private:
        // The awaitable of stopped(): the channel's receive, with nothing
        // given back (a receive of a closed channel of signals is false,
        // which says nothing here)
        class stop_wait {
        public:
            SGCL_INLINE_HOT explicit stop_wait(detail::ChannelState<void>& ch) noexcept
            : _op(ch.receive()) {
            }

            SGCL_INLINE_HOT bool await_ready() {
                return _op.await_ready();
            }

            template<class P>
            SGCL_INLINE_HOT bool await_suspend(std::coroutine_handle<P> h) {
                return _op.await_suspend(h);
            }

            SGCL_INLINE_HOT void await_resume() {
                (void)_op.await_resume();
            }

        private:
            decltype(std::declval<detail::ChannelState<void>&>().receive()) _op;
        };

    public:

        SGCL_INLINE_HOT friend bool operator==(const stop_token& a, const stop_token& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend class stop_source;

        SGCL_INLINE_HOT explicit stop_token(const tracked_ptr<detail::StopState>& s) noexcept
        : _s(s) {
        }

        tracked_ptr<detail::StopState> _s;
    };

    class stop_source {
    public:
        SGCL_INLINE_HOT stop_source() noexcept
        : _s(make_tracked<detail::StopState>()) {
            _s->link();   // before the state is given to anyone
        }

        // A child of the source the token belongs to: stopped with it
        explicit stop_source(const stop_token& parent)
        : _s(make_tracked<detail::StopState>()) {
            _s->link();
            if (parent._s) {
                _s->parent = parent._s;
                // the entries of children gone popped from the head first, so
                // that a long-lived source with a child per request holds no
                // more than the live ones (children die roughly in their
                // order; a live head ends the look and goes back to the tail)
                while (auto c = parent._s->children.try_pop()) {
                    if (auto alive = c->lock()) {
                        parent._s->children.push(*c);
                        std::atomic_thread_fence(std::memory_order_seq_cst);
                        if (parent._s->stopped()) {   // the parent's stop may have passed the list meanwhile: this child is its
                            alive->stop();
                        }
                        break;
                    }
                }
                parent._s->children.push(weak_ptr<detail::StopState>(_s));
                std::atomic_thread_fence(std::memory_order_seq_cst);   // the push before the look at the signal (StopState::stop: the close before its look at the list)
                if (parent._s->stopped()) {   // stopped while this child was registering
                    _s->stop();
                }
            }
        }

        SGCL_INLINE_HOT stop_token token() const noexcept {
            return stop_token(_s);
        }

        SGCL_INLINE_HOT bool stop_requested() const noexcept {
            return _s->stopped();
        }

        // The stop: the token's channel closed, every waiter woken, the children stopped
        SGCL_INLINE_HOT void request_stop() {
            _s->stop();
        }

        // The stop requested after d, by a timer (a deadline): the same
        // stop as request_stop, from the timer's thread, the channel
        // closed and the children stopped in one step. The stop, by hand
        // or by the parent's, cancels the timer; a d that reaches the end
        // of time (duration::max()) arms nothing, and a later deadline
        // beside an earlier one neither: the earliest stops first
        SGCL_INLINE_HOT void stop_after(duration d) {
            _arm(clock::now() + d);   // saturated: a span past the end of time is time_point::max()
        }

        // The same at a point of the module's clock
        SGCL_INLINE_HOT void stop_at(time_point when) {
            _arm(when);
        }

    private:
        // The deadline armed and published in the state, unless an earlier
        // one is there; the one it replaces cancelled. A stop that looked at
        // the state before the publication is seen by the look after it
        // (the fences of StopState::stop and of this): the deadline taken
        // back and cancelled, never left armed behind a stop
        void _arm(time_point when) {
            if (when == time_point::max() || _s->stopped()) {   // a point at max() never fires (timer.h)
                return;
            }
            auto cur = _s->deadline.load();
            if (cur && cur->when <= when) {
                return;
            }
            tracked_ptr<detail::StopDeadline> d = make_tracked<detail::StopDeadline>();
            d->state.store(_s, std::memory_order_relaxed);
            d->when = when;
            d->timer = detail::add_timer(when, d, &detail::StopDeadline::fire);
            for (;;) {
                if (cur && cur->when <= when) {   // an earlier one armed meanwhile
                    d->cancel();
                    return;
                }
                if (_s->deadline.compare_exchange_weak(cur, d)) {
                    break;
                }
            }
            if (cur) {
                cur->cancel();
            }
            std::atomic_thread_fence(std::memory_order_seq_cst);   // the publication before the look at the signal (StopState::stop: the close before its look at the deadline)
            if (_s->stopped()) {
                if (auto x = _s->deadline.exchange(nullptr)) {
                    x->cancel();
                }
            }
        }

        tracked_ptr<detail::StopState> _s;
    };
}
