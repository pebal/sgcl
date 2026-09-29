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
            MutexState()
            : ch(1) {
                ch.try_send();
            }

            // made linked (ChannelState::link): mutex() links the channel
            // and puts the one permit in, before the state is given to
            // anyone (a MutexState that is a field of another object, a
            // shared_mutex's, is made as above)
            explicit MutexState(ChannelLinked)
            : ch(ChannelLinked{}, 1) {
            }

            MutexState(const MutexState&) = delete;
            MutexState& operator=(const MutexState&) = delete;

            void lock() {
                (void)ch.receive().wait();
            }

            bool try_lock() {
                return ch.try_receive();
            }

            void unlock() {
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
        mutex()
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
        void lock() const {
            _s->lock();
        }

        bool try_lock() const {
            return _s->try_lock();
        }

        void unlock() const {
            _s->unlock();
        }

        // A case of a select: f() with the mutex locked
        template<class F>
        auto on_lock(F f) const {
            return _s->ch.on_receive(std::move(f));
        }

        // The same mutex: the same state
        friend bool operator==(const mutex& a, const mutex& b) noexcept {
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

            ~guard() {
                if (_m) {
                    _m->unlock();
                }
            }

            // The mutex held: what a condition_variable lets go of and
            // takes back around its wait
            mutex owner() const noexcept {
                return mutex(tracked_ptr<detail::MutexState>(_m));
            }

            // The mutex let go of by the guard, still locked: the caller
            // unlocks it (std::unique_lock::release)
            mutex release() noexcept {
                mutex m{tracked_ptr<detail::MutexState>(_m)};
                _m = nullptr;
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
            bool await_ready() {
                return _op.await_ready();
            }

            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) {
                return _op.await_suspend(h);
            }

            guard await_resume() {
                _op.await_resume();
                return guard(_m);
            }

        private:
            friend class mutex;

            explicit scoped_lock_op(detail::MutexState* m) noexcept
            : _m(m)
            , _op(m->ch.receive()) {
            }

            detail::MutexState* _m;
            decltype(std::declval<detail::ChannelState<void>&>().receive()) _op;
        };

        // The lock held for a scope: `auto g = co_await m.scoped_lock();` in
        // a task, `auto g = m.scoped_lock().wait();` on a thread
        auto scoped_lock() const {
            return operation([s = _s.get()](auto how) {
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

        explicit mutex(const tracked_ptr<detail::MutexState>& s) noexcept
        : _s(s) {
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        mutex(sgcl::detail::FromWord, const tracked_ptr<detail::MutexState>& w) noexcept
        : _s(w) {
        }

        tracked_ptr<detail::MutexState>& _handle_word() noexcept {
            return _s;
        }

        const tracked_ptr<detail::MutexState>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<detail::MutexState> _s;
    };
}
