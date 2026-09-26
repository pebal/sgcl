//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../concurrent/queue.h"
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
    // is or a timer holds it.
    namespace detail {
        struct StopState {
            channel<void> signal;                        // closed: the stop requested
            tracked_ptr<StopState> parent;
            concurrent::queue<weak_ptr<StopState>> children;

            bool stopped() const noexcept {
                return signal.closed();
            }

            void stop() {
                signal.close();
                std::atomic_thread_fence(std::memory_order_seq_cst);   // the close before the look at the children, against a child registering (its push, then its look at the signal): one of the two sees the other (channel.h: _fence)
                while (auto c = children.try_pop()) {   // the children stopped after the parent
                    if (auto child = c->lock()) {
                        child->stop();
                    }
                }
            }
        };
    }

    class stop_token {
    public:
        stop_token() noexcept = default;

        bool stop_requested() const noexcept {
            return _s && _s->stopped();
        }

        bool stop_possible() const noexcept {
            return (bool)_s;
        }

        // The channel closed by the stop: a case of a select, or a wait
        channel<void>& channel() const noexcept {
            assert(_s && "an empty stop_token has no channel: stop_possible() says");
            return _s->signal;
        }

        // The case of a select served by the stop: `token.on_stop([&] { running = false; })`
        template<class F>
        auto on_stop(F f) const {
            assert(_s && "an empty stop_token has no case: stop_possible() says");
            return _s->signal.on_receive(std::move(f));
        }

        // Waits for the stop, and gives nothing: `co_await token.stopped()`
        // in a task, `token.stopped().wait()` on a thread
        auto stopped() const {
            assert(_s && "an empty stop_token has nothing to await: stop_possible() says");
            return operation([s = _s](auto how) -> decltype(auto) {
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
            explicit stop_wait(async::channel<void>& ch) noexcept
            : _op(ch.receive()) {
            }

            bool await_ready() {
                return _op.await_ready();
            }

            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) {
                return _op.await_suspend(h);
            }

            void await_resume() {
                (void)_op.await_resume();
            }

        private:
            decltype(std::declval<async::channel<void>&>().receive()) _op;
        };

    public:

        friend bool operator==(const stop_token& a, const stop_token& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend class stop_source;

        explicit stop_token(const tracked_ptr<detail::StopState>& s) noexcept
        : _s(s) {
        }

        tracked_ptr<detail::StopState> _s;
    };

    class stop_source {
    public:
        stop_source()
        : _s(make_tracked<detail::StopState>()) {
        }

        // A child of the source the token belongs to: stopped with it
        explicit stop_source(const stop_token& parent)
        : _s(make_tracked<detail::StopState>()) {
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

        stop_token token() const noexcept {
            return stop_token(_s);
        }

        bool stop_requested() const noexcept {
            return _s->stopped();
        }

        // The stop: the token's channel closed, every waiter woken, the children stopped
        void request_stop() {
            _s->stop();
        }

        // The stop requested after d, by a timer (a deadline): the same
        // stop as request_stop, from the timer's thread, the channel
        // closed and the children stopped in one step
        void stop_after(duration d) {
            detail::add_timer(d, _s, [](void* s) { static_cast<detail::StopState*>(s)->stop(); });
        }

        // The same at a point of the module's clock
        void stop_at(time_point when) {
            detail::add_timer(when, _s, [](void* s) { static_cast<detail::StopState*>(s)->stop(); });
        }

    private:
        tracked_ptr<detail::StopState> _s;
    };
}
