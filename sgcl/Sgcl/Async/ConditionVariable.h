//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A wait under the Mutex for a notify, blocking and awaitable
// (sgcl::condition_variable)
#pragma once

#include "../../async/condition_variable.h"
#include "Mutex.h"
#include "Task.h"

#include <utility>

namespace Sgcl {
    // Go's sync.Cond over the Mutex: a wait lets go of the mutex, waits
    // for a notify and takes it back; a task waiting holds no thread. A
    // notify before the wait began is lost, so the condition is checked
    // under the mutex before every wait (the predicate forms do)
    class ConditionVariable {
    public:
        using InnerType = sgcl::condition_variable;

        ConditionVariable() = default;
        ConditionVariable(const ConditionVariable&) = delete;
        ConditionVariable& operator=(const ConditionVariable&) = delete;

        void NotifyOne() {
            _cv.notify_one();
        }

        void NotifyAll() {
            _cv.notify_all();
        }

        // A thread's wait, with a Mutex::Guard or a std::unique_lock<Mutex>
        void Wait(Mutex::Guard& guard) {
            _cv.wait(guard);
        }

        template<class Lock>
        void Wait(Lock& lock) {
            _cv.wait(lock);
        }

        template<class Lock, class Pred>
        void Wait(Lock& lock, Pred pred) {
            _cv.wait(lock, std::move(pred));
        }

        // A task's wait: `co_await cv.AsyncWait(guard)`
        Task<> AsyncWait(Mutex::Guard& guard) {
            return _cv.async_wait(guard);
        }

        template<class Pred>
        Task<> AsyncWait(Mutex::Guard& guard, Pred pred) {
            while (!pred()) {
                co_await _cv.async_wait(guard);
            }
        }

        InnerType& Inner() noexcept {
            return _cv;
        }

    private:
        InnerType _cv;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
