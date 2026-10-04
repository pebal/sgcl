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

#include <coroutine>
#include <system_error>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // The synchronization of tasks, and of threads with them: this mutex,
    // a semaphore, an event, a wait group and a once (their own headers),
    // each with the three forms of a wait a channel has: blocking
    // (`lock()`, for a thread), awaitable (`co_await m.scoped_lock()`, for
    // a task, which holds no thread while it waits), and as a case of a
    // select (`m.on_lock(f)`), because each is a channel of signals under
    // the name of what it does: a mutex is a channel holding one signal
    // (Go's idiom), a semaphore one holding n, an event and a wait group
    // a channel closed when the moment comes. A std::mutex on a worker
    // would park the worker and every task on it; these park nothing.
    // After them a shared_mutex (readers and a writer over a word, the
    // channels for its waits) and a condition_variable over this mutex,
    // each blocking and awaitable.
    //
    class mutex;

    namespace detail {
        // The state of a mutex: a channel holding one signal. lock() is a
        // receive, unlock() a send
        struct MutexState {
            // noexcept: a channel just made has nobody to wake, and a ring
            // of one is never too large
            SGCL_INLINE_HOT MutexState() noexcept
            : ch(1) {
                ch.try_send();
            }

            // made linked (ChannelState::link): mutex() links the channel
            // and puts the one permit in, before the state is given to
            // anyone (a MutexState that is a field of another object, a
            // shared_mutex's, is made as above)
            SGCL_INLINE_HOT explicit MutexState(ChannelLinked) noexcept
            : ch(ChannelLinked{}, 1) {
            }

            MutexState(const MutexState&) = delete;
            MutexState& operator=(const MutexState&) = delete;

            // noexcept: a receive is potentially throwing only for the
            // wake of a sender waiting on the channel (a wake may start
            // the workers), and this channel never has one: unlock is a
            // try_send, which never waits
            SGCL_INLINE_HOT void lock() noexcept {
                (void)ch.receive().wait();
            }

            SGCL_INLINE_HOT bool try_lock() noexcept {
                return ch.try_receive();
            }

            SGCL_INLINE_HOT void unlock() {
                ch.try_send();
            }

            ChannelState<void> ch;
        };
    }

    // A mutex: one holder at a time; lock() is a receive, unlock() a send.
    // A handle: one word, a tracked_ptr to the state, which copies share —
    // passed by value into a task, a member of a managed object, a local;
    // in a global or a std container, a rooted<async::mutex>. Made unlocked
    // by its constructor; there is no empty mutex.
    class mutex {
    public:
        SGCL_INLINE_HOT mutex() noexcept
        : _s(make_tracked<detail::MutexState>(detail::ChannelLinked{})) {
            _s->ch.link();   // before the state is given to anyone
            _s->ch.try_send();   // unlocked
        }

        mutex(const mutex&) noexcept = default;
        mutex(mutex&&) noexcept = default;
        mutex& operator=(const mutex&) noexcept = default;
        mutex& operator=(mutex&&) noexcept = default;

        // The standard's Lockable, blocking (std::lock_guard, std::unique_lock);
        // a task takes the mutex with `co_await m.scoped_lock()`
        SGCL_INLINE_HOT void lock() const noexcept {
            _s->lock();
        }

        SGCL_INLINE_HOT bool try_lock() const noexcept {
            return _s->try_lock();
        }

        SGCL_INLINE_HOT void unlock() const {
            _s->unlock();
        }

        // A case of a select: f() with the mutex locked
        template<class F>
        SGCL_INLINE_HOT auto on_lock(F f) const noexcept(std::is_nothrow_move_constructible_v<F>) {
            return _s->ch.on_receive(std::move(f));
        }

        // The same mutex: the same state
        SGCL_INLINE_HOT friend bool operator==(const mutex& a, const mutex& b) noexcept {
            return a._s == b._s;
        }

        // The lock held for a scope: `auto guard = co_await m.scoped_lock();`
        // (std::lock_guard<sgcl::async::mutex> does the same for a thread).
        // An object of its scope, as the mutex* it held before the handles:
        // the state's address, one word, on a stack or in a task's frame,
        // both scanned conservatively, so that the word keeps the state; never
        // in a managed object or a container, as any raw pointer (a store
        // there has no barrier, and may land in an object the cycle has
        // scanned already). Empty once moved from or released. A tracked
        // word here was measured: +1.1 ns (+9%) per lock (DESIGN note)
        class guard {
        public:
            explicit guard(const mutex& m) noexcept
            : _m(m._s.get()) {
            }

            guard(guard&& o) noexcept
            : _m(std::exchange(o._m, nullptr)) {
            }

            guard(const guard&) = delete;
            guard& operator=(const guard&) = delete;

            // noexcept, as a destructor is. The unlock's one throw is the
            // std::system_error of a start of the workers its wake of the
            // next holder needed and could not make (the workers stopped);
            // the woken task is queued before the start, and runs at the
            // next one, so the error is let go of here
            ~guard() {
                if (_m) {
                    try {
                        _m->unlock();
                    } catch (const std::system_error&) {
                    }
                }
            }

            // The mutex held: what a condition_variable lets go of and
            // takes back around its wait; nothing for an empty guard (moved
            // from or released), since there is no mutex without a state
            SGCL_INLINE_HOT optional<mutex> owner() const noexcept {
                if (!_m) {
                    return nullopt;
                }
                return mutex(tracked_ptr<detail::MutexState>(_m));
            }

            // The mutex let go of by the guard, still locked: the caller
            // unlocks it (std::unique_lock::release); nothing for an empty
            // guard
            SGCL_INLINE_HOT optional<mutex> release() noexcept {
                if (!_m) {
                    return nullopt;
                }
                mutex m{tracked_ptr<detail::MutexState>(std::exchange(_m, nullptr))};
                return m;
            }

        private:
            friend class mutex;

            // From the state an operation holds (the mutex's own word, alive
            // for the operation's expression)
            explicit guard(detail::MutexState* s) noexcept
            : _m(s) {
            }

            detail::MutexState* _m;
        };

        // The awaiter of scoped_lock(): the state by its address, as a
        // channel's awaiters hold theirs, in the awaiting frame, which the
        // collector scans
        class scoped_lock_op {
        public:
            SGCL_INLINE_HOT bool await_ready() {
                return _op.await_ready();
            }

            template<class P>
            SGCL_INLINE_HOT bool await_suspend(std::coroutine_handle<P> h) {
                return _op.await_suspend(h);
            }

            SGCL_INLINE_HOT guard await_resume() {
                _op.await_resume();
                return guard(_m);
            }

        private:
            friend class mutex;

            SGCL_INLINE_HOT explicit scoped_lock_op(detail::MutexState* m) noexcept
            : _m(m)
            , _op(m->ch.receive()) {
            }

            detail::MutexState* _m;
            decltype(std::declval<detail::ChannelState<void>&>().receive()) _op;
        };

        // The lock held for a scope: `auto g = co_await m.scoped_lock();` in
        // a task, `auto g = m.scoped_lock().wait();` on a thread
        SGCL_INLINE_HOT auto scoped_lock() const noexcept {
            return detail::make_operation([s = _s.get()](auto how) {
                if constexpr (std::is_same_v<decltype(how), detail::awaited_t>) {
                    return scoped_lock_op(s);
                } else {
                    s->lock();
                    return guard(s);
                }
            });
        }

    private:
        friend class condition_variable;
        friend class shared_mutex;

        SGCL_INLINE_HOT explicit mutex(const tracked_ptr<detail::MutexState>& s) noexcept
        : _s(s) {
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT mutex(sgcl::detail::FromWord, const tracked_ptr<detail::MutexState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::MutexState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::MutexState>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<detail::MutexState> _s;
    };
}
