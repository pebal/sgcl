//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

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
    // A mutex: one holder at a time; lock() is a receive, unlock() a send
    class mutex {
    public:
        mutex()
        : _ch(1) {
            _ch.try_send();
        }

        mutex(const mutex&) = delete;
        mutex& operator=(const mutex&) = delete;

        // The standard's Lockable, blocking (std::lock_guard, std::unique_lock);
        // a task takes the mutex with `co_await m.scoped_lock()`
        void lock() {
            (void)_ch.receive().wait();
        }

        bool try_lock() {
            return _ch.try_receive();
        }

        void unlock() {
            _ch.try_send();
        }

        // A case of a select: f() with the mutex locked
        template<class F>
        auto on_lock(F f) {
            return _ch.on_receive(std::move(f));
        }

        // The lock held for a scope: `auto guard = co_await m.scoped_lock();`
        // (std::lock_guard<sgcl::async::mutex> does the same for a thread)
        class guard {
        public:
            explicit guard(mutex& m) noexcept
            : _m(&m) {
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
            mutex& owner() const noexcept {
                return *_m;
            }

            // The mutex let go of by the guard, still locked: the caller
            // unlocks it (std::unique_lock::release)
            mutex* release() noexcept {
                return std::exchange(_m, nullptr);
            }

        private:
            mutex* _m;
        };

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
                return guard(*_m);
            }

        private:
            friend class mutex;

            explicit scoped_lock_op(mutex& m) noexcept
            : _m(&m)
            , _op(m._ch.receive()) {
            }

            mutex* _m;
            decltype(std::declval<channel<void>&>().receive()) _op;
        };

        // The lock held for a scope: `auto g = co_await m.scoped_lock();` in
        // a task, `auto g = m.scoped_lock().wait();` on a thread
        auto scoped_lock() {
            return operation([this](auto how) {
                if constexpr (std::is_same_v<decltype(how), detail::awaited_t>) {
                    return scoped_lock_op(*this);
                } else {
                    lock();
                    return guard(*this);
                }
            });
        }

    private:
        friend class condition_variable;
        friend class shared_mutex;

        channel<void> _ch;
    };
}
