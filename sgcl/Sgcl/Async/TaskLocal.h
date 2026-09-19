//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// TaskLocal<T>: a value visible to a task and to the tasks it starts, read
// from anywhere inside the task without passing it along: a request id,
// a deadline, a logger, the current user. Declared at namespace scope;
// `co_await x.Set(v)` sets it for the task and its children from then on,
// x.Get() reads it (None when unset or outside a task), x.With(v, task)
// is a task that runs the task with the value.
#pragma once

#include "../../async/task_local.h"
#include "../Core/Types.h"
#include "Coroutine.h"
#include "Scheduler.h"

#include <utility>

namespace Sgcl {
    template<class T>
    class TaskLocal {
    public:
        using ValueType = T;
        using InnerType = sgcl::task_local<T>;

        TaskLocal() noexcept = default;
        TaskLocal(const TaskLocal&) = delete;
        TaskLocal& operator=(const TaskLocal&) = delete;

        // `co_await x.Set(v)`: the value for this task and the tasks it
        // starts from here on; never suspends
        auto Set(T value) {
            return _l.set(std::move(value));
        }

        // The value of the task this thread runs; None when unset, and
        // outside a task
        Optional<T> Get() const {
            return _l.get();
        }

        // The value, or the one given when there is none
        T GetOr(T fallback) const {
            return _l.get_or(std::move(fallback));
        }

        bool IsSet() const noexcept {
            return _l.is_set();
        }

        // A task that runs t with the value set
        template<class U>
        Task<U> With(T value, Task<U> t) {
            return _l.with(std::move(value), std::move(t.Inner()));   // the inner task as a Task: no frame of its own (Timeout.h)
        }

        InnerType& Inner() noexcept {
            return _l;
        }

        const InnerType& Inner() const noexcept {
            return _l;
        }

    private:
        InnerType _l;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
