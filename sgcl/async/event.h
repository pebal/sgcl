//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/handle_word.h"
#include "channel.h"
#include "operation.h"
#include "coroutine.h"

#include <cassert>
#include <coroutine>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }

    class event;

    namespace detail {
        struct EventAccess;
    }

    // An event: set once, waited for by any number; a wait after the set
    // does not wait. Under it a channel of signals closed by the set. A
    // handle: one word, a tracked_ptr to that channel, which copies share —
    // passed by value into a task, a member of a managed object, a local;
    // in a global or a std container, a rooted<async::event>. What the
    // reactor and the timers give (readable, writable, exited, after, at)
    // is an event too, set when the moment comes or when the wait is ended
    // with nothing: a wait woken by one looks at its source again. An event
    // happens once, and it is set by the close of its channel alone: after
    // wait(), co_await or a select's on_set, is_set() is true.
    class event {
        using State = detail::ChannelState<void>;

    public:
        SGCL_INLINE_HOT event() noexcept
        : _s(detail::make_linked_state<void>()) {
        }

        event(const event&) noexcept = default;
        event(event&&) noexcept = default;
        event& operator=(const event&) noexcept = default;
        event& operator=(event&&) noexcept = default;

        SGCL_INLINE_HOT void set() const {
            _s->close();
        }

        SGCL_INLINE_HOT bool is_set() const noexcept {
            return _s->closed();
        }

        // Waits for the set: `e.wait()` on a thread, `co_await e` in a task,
        // as a task is waited for (DESIGN 221: a completion is waited for
        // by itself)
        // noexcept: nobody sends on the channel, the set closes it, so the
        // receive wakes no sender (mutex.h: MutexState::lock)
        SGCL_INLINE_HOT void wait() const noexcept {
            assert(!detail::on_worker() && "wait() blocks the worker: co_await the event from a task");
            (void)_s->receive().wait();
        }

        class wait_op;
        wait_op operator co_await() const noexcept;

        // The awaiter of `co_await e`: the task resumed by the set
        class wait_op {
        public:
            SGCL_INLINE_HOT bool await_ready() {
                return _op.await_ready();
            }

            template<class P>
            SGCL_INLINE_HOT bool await_suspend(std::coroutine_handle<P> h) {
                return _op.await_suspend(h);
            }

            SGCL_INLINE_HOT void await_resume() {
                _op.await_resume();
            }

        private:
            friend class event;

            SGCL_INLINE_HOT explicit wait_op(State& ch) noexcept
            : _op(ch.receive()) {
            }

            decltype(std::declval<State&>().receive()) _op;
        };

        template<class F>
        SGCL_INLINE_HOT auto on_set(F f) const noexcept(std::is_nothrow_move_constructible_v<F>) {
            return _s->on_receive(std::move(f));
        }

        // The same event: the same state
        SGCL_INLINE_HOT friend bool operator==(const event& a, const event& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend struct detail::EventAccess;

        SGCL_INLINE_HOT explicit event(tracked_ptr<State> s) noexcept
        : _s(std::move(s)) {
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT event(sgcl::detail::FromWord, const tracked_ptr<State>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<State>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<State>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<State> _s;
    };

    SGCL_INLINE_HOT event::wait_op event::operator co_await() const noexcept {
        return wait_op(*_s);
    }

    namespace detail {
        // An event over a channel of signals the library made (the
        // reactor's, a timer's), and the channel under an event
        struct EventAccess {
            SGCL_INLINE_HOT static event make(tracked_ptr<ChannelState<void>> s) noexcept {
                return event(std::move(s));
            }

            SGCL_INLINE_HOT static const tracked_ptr<ChannelState<void>>& state(const event& e) noexcept {
                return e._s;
            }

            // A reactor's event set because its source was ready, not
            // ended with nothing (ChannelState<void>::closed_ready: the bit
            // read after the close is seen)
            SGCL_INLINE_HOT static bool ready(const event& e) noexcept {
                return e._s->closed_ready();
            }
        };
    }
}
