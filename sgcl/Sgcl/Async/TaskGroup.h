//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// TaskGroup: structured concurrency, the way Go's errgroup and Kotlin's
// coroutineScope have it. A scope under a token that owns the tasks it
// spawns: Spawn(t) starts a child and counts it, Wait() (a thread),
// `co_await AsyncWait()` (a task) and OnDone(f) (a Select case) wait
// for every child; the first child that throws stops the others through
// Token(), and the wait rethrows its exception once every child has
// finished.
#pragma once

#include "../../async/task_group.h"
#include "StopToken.h"
#include "Task.h"

#include <cstddef>
#include <utility>

namespace Sgcl {
    class TaskGroup {
    public:
        using InnerType = sgcl::task_group;

        // A scope under the token: its own source is a child of the
        // token's, stopped with it; an empty token is a scope on its own
        explicit TaskGroup(const StopToken& parent = StopToken())
        : _g(parent.Inner()) {
        }

        TaskGroup(const TaskGroup&) = delete;
        TaskGroup& operator=(const TaskGroup&) = delete;

        // A child: started on the scheduler and counted; its result, if
        // any, dropped, its exception the group's
        template<class T>
        void Spawn(Task<T> t) {
            _g.spawn(std::move(t.Inner()));
        }

        // The token the children are given: stopped by the first
        // exception, by RequestStop(), by the parent, or by the group's end
        StopToken Token() const noexcept {
            return StopToken(_g.token());
        }

        // The stop of the whole scope, by hand
        void RequestStop() {
            _g.request_stop();
        }

        bool IsStopRequested() const noexcept {
            return _g.stop_requested();
        }

        // The children not yet finished
        size_t Count() const noexcept {
            return _g.count();
        }

        // Waits for every child on this thread, then rethrows the first
        // exception a child threw, if any
        void Wait() {
            _g.wait();
        }

        // `co_await g.AsyncWait()`: the task resumed when every child has
        // finished, the first exception rethrown then
        Task<> AsyncWait() {
            return _g.async_wait();   // the inner task as a Task: no frame of its own (Timeout.h)
        }

        // A case of a Select: f() when every child has finished
        template<class F>
        auto OnDone(F f) {
            return _g.on_done(std::move(f));
        }

        InnerType& Inner() noexcept {
            return _g;
        }

        const InnerType& Inner() const noexcept {
            return _g;
        }

    private:
        InnerType _g;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
