//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Any number of readers at once, or one writer, over a word, the
// channels for the waits; blocking and awaitable (sgcl::shared_mutex)
#pragma once

#include "../../async/shared_mutex.h"


namespace Sgcl {
    // Any number of readers at once, or one writer (Go's sync.RWMutex,
    // Java's ReentrantReadWriteLock): a word for the readers' count and
    // the writer, channels for the waits; a writer waiting blocks the
    // readers that come after it, and the readers it held back get in
    // before the next writer
    class SharedMutex {
    public:
        using InnerType = sgcl::shared_mutex;
        using Guard = sgcl::shared_mutex::guard;                // the writer's lock held for a scope
        using SharedGuard = sgcl::shared_mutex::shared_guard;   // a reader's

        SharedMutex() = default;
        SharedMutex(const SharedMutex&) = delete;
        SharedMutex& operator=(const SharedMutex&) = delete;

        // A reader
        void LockShared() {
            _m.lock_shared();
        }

        bool TryLockShared() noexcept {
            return _m.try_lock_shared();
        }

        void UnlockShared() {
            _m.unlock_shared();
        }

        // The writer
        void Lock() {
            _m.lock();
        }

        bool TryLock() {
            return _m.try_lock();
        }

        void Unlock() {
            _m.unlock();
        }

        // `co_await m.AsyncLockShared()`: a reader, locked when the task
        // resumes; `auto g = co_await m.AsyncScopedLockShared();` the
        // same, unlocked with the guard
        auto AsyncLockShared() noexcept {
            return _m.async_lock_shared();
        }

        auto AsyncScopedLockShared() noexcept {
            return _m.async_scoped_lock_shared();
        }

        // The same for the writer
        auto AsyncLock() noexcept {
            return _m.async_lock();
        }

        auto AsyncScopedLock() noexcept {
            return _m.async_scoped_lock();
        }

        // For std::shared_lock<SharedMutex> and std::lock_guard<SharedMutex>
        void lock_shared() {
            _m.lock_shared();
        }

        bool try_lock_shared() noexcept {
            return _m.try_lock_shared();
        }

        void unlock_shared() {
            _m.unlock_shared();
        }

        void lock() {
            _m.lock();
        }

        bool try_lock() {
            return _m.try_lock();
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
