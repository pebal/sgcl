//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// One holder at a time, the three forms of a wait a Channel has:
// blocking (for a thread), awaitable (for a task, no thread held) and
// as a case of a Select; a std::mutex on a worker would park the worker
// and every task on it, this parks nothing (sgcl::mutex)
#pragma once

#include "../../async/mutex.h"

#include <utility>

namespace Sgcl {
    // One holder at a time; Lock() is a receive, Unlock() a send
    class Mutex {
    public:
        using InnerType = sgcl::mutex;
        using Guard = sgcl::mutex::guard;   // the lock held for a scope

        Mutex() = default;
        Mutex(const Mutex&) = delete;
        Mutex& operator=(const Mutex&) = delete;

        void Lock() {
            _m.lock();
        }

        bool TryLock() {
            return _m.try_lock();
        }

        void Unlock() {
            _m.unlock();
        }

        // `co_await m.AsyncLock()`: locked when the task resumes
        auto AsyncLock() noexcept {
            return _m.async_lock();
        }

        // `auto guard = co_await m.AsyncScopedLock();`: unlocked with the guard
        auto AsyncScopedLock() noexcept {
            return _m.async_scoped_lock();
        }

        // A case of a Select: f() with the mutex locked
        template<class F>
        auto OnLock(F f) {
            return _m.on_lock(std::move(f));
        }

        // For std::lock_guard<Mutex>
        void lock() {
            _m.lock();
        }

        void unlock() {
            _m.unlock();
        }

        InnerType& Inner() noexcept {
            return _m;
        }

    private:
        InnerType _m;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
