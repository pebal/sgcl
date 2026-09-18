//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../concurrent/concurrent_queue.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "channel.h"
#include "coroutine.h"
#include "mutex.h"

#include <utility>

namespace sgcl {
    // A condition variable: Go's sync.Cond, std::condition_variable_any,
    // over the module's mutex, for tasks and threads alike. A wait lets
    // go of the mutex, waits for a notify and takes the mutex back; a
    // task waiting holds no thread (the mutex is the module's, so the
    // taking back is a co_await too). Under it a queue of waiters, each
    // a channel of one signal: notify_one takes the first and sends it
    // the signal, notify_all every one; a waiter is registered before the
    // mutex is let go of, so that a notify made under the mutex after
    // the wait began reaches it, and a signal sent before the waiter
    // reaches its receive is kept for it (the channel buffers it): no
    // wakeup is lost between the unlock and the wait. The one that is
    // lost is the one a condition variable loses by contract: a notify
    // before the wait began finds no waiter, so the waiter must check the
    // condition under the mutex before it waits (`wait(guard, predicate)`
    // does), as Go's and the standard's must.
    class condition_variable {
    public:
        condition_variable() = default;
        condition_variable(const condition_variable&) = delete;
        condition_variable& operator=(const condition_variable&) = delete;

        // Wakes the first waiter, if any
        void notify_one() {
            if (auto w = _waiters.try_pop()) {
                (*w)->try_send();
            }
        }

        // Wakes every waiter
        void notify_all() {
            while (auto w = _waiters.try_pop()) {
                (*w)->try_send();
            }
        }

        // A thread's wait: the guard's mutex let go of, the wait, the
        // mutex taken back; or any lock with unlock() and lock()
        // (std::unique_lock<sgcl::mutex>)
        void wait(mutex::guard& g) {
            auto& m = g.owner();
            tracked_ptr<channel<void>> w = _register();
            m.unlock();
            w->receive();
            m.lock();
        }

        template<class Lock>
        void wait(Lock& lock) {
            tracked_ptr<channel<void>> w = _register();
            lock.unlock();
            w->receive();
            lock.lock();
        }

        // The wait until the predicate holds, checked under the mutex
        // before every wait
        template<class Lock, class Pred>
        void wait(Lock& lock, Pred pred) {
            while (!pred()) {
                wait(lock);
            }
        }

        // A task's wait: `co_await cv.async_wait(guard)`, the mutex taken
        // back with a co_await too
        task<> async_wait(mutex::guard& g) {
            auto& m = g.owner();
            tracked_ptr<channel<void>> w = _register();
            m.unlock();
            co_await w->async_receive();
            co_await m.async_lock();
        }

        template<class Pred>
        task<> async_wait(mutex::guard& g, Pred pred) {
            while (!pred()) {
                co_await async_wait(g);
            }
        }

    private:
        tracked_ptr<channel<void>> _register() {
            tracked_ptr<channel<void>> w = make_tracked<channel<void>>(1);
            _waiters.push(w);
            return w;
        }

        concurrent_queue<tracked_ptr<channel<void>>> _waiters;
    };
}
