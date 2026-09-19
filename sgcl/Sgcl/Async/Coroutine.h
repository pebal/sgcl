//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The coroutines with managed frames: ManagedFrame, the base of a promise
// whose frames come from the managed heap, and FramePtr, the owner of
// such a coroutine, for a coroutine type of your own; Task<T>, a coroutine
// that runs on the scheduler (Scheduler.h: Spawn, Go) or by hand, joined
// or co_awaited for its result; Generator<T>, a coroutine that yields
// values for a range-for. The promise is the one of the type inside, so a
// Task is a coroutine's return type.
#pragma once

#include "../../async/coroutine.h"

#include <coroutine>
#include <utility>

namespace Sgcl {
    // The base of a promise type whose coroutines get their frames from
    // the managed heap (sgcl::managed_frame): a promise of your own
    // derives from it and returns, from get_return_object, an object
    // holding a FramePtr made from the handle
    using ManagedFrame = sgcl::managed_frame;

    // The owner of a coroutine whose promise derives from ManagedFrame:
    // a RootPtr to the frame and the coroutine handle, move-only,
    // destroying the coroutine when destroyed (sgcl::frame_ptr)
    template<class P>
    class FramePtr {
    public:
        using PromiseType = P;
        using HandleType = std::coroutine_handle<P>;
        using InnerType = sgcl::frame_ptr<P>;

        FramePtr() noexcept = default;

        // Takes the frame of the coroutine over: from get_return_object,
        // with std::coroutine_handle<P>::from_promise(*this)
        explicit FramePtr(HandleType h)
        : _f(h) {
        }

        FramePtr(FramePtr&&) noexcept = default;
        FramePtr& operator=(FramePtr&&) noexcept = default;
        FramePtr(const FramePtr&) = delete;
        FramePtr& operator=(const FramePtr&) = delete;

        explicit FramePtr(InnerType&& f) noexcept
        : _f(std::move(f)) {
        }

        // Whether a coroutine is held
        explicit operator bool() const noexcept {
            return static_cast<bool>(_f);
        }

        // The coroutine handle, null when empty: valid while this holds
        // the frame; never destroy the coroutine through it
        HandleType Handle() const noexcept {
            return _f.handle();
        }

        // The promise in the frame; not empty
        P& Promise() const {
            return _f.promise();
        }

        // Runs the coroutine to its next suspension or its end
        void Resume() {
            _f.resume();
        }

        // Empty, or suspended at the final suspend point
        bool IsDone() const noexcept {
            return _f.done();
        }

        // Destroys the coroutine, if any, and leaves this empty
        void Destroy() noexcept {
            _f.destroy();
        }

        // Lets go of the frame without destroying the coroutine (a task
        // detached: it destroys itself when done)
        void Release() noexcept {
            _f.release();
        }

        InnerType& Inner() noexcept {
            return _f;
        }

        const InnerType& Inner() const noexcept {
            return _f;
        }

    private:
        InnerType _f;
    };

    template<class T = void>
    class Task {
    public:
        using ResultType = T;
        using InnerType = sgcl::task<T>;
        using promise_type = typename InnerType::promise_type;   // the coroutine's: the frame is managed

        Task() noexcept = default;
        Task(Task&&) noexcept = default;
        Task& operator=(Task&&) noexcept = default;
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;

        // What the promise returns, made into the Task
        Task(InnerType&& t) noexcept
        : _t(std::move(t)) {
        }

        // On the scheduler: queued, run by a worker to its next suspension;
        // nodiscard, since the Task dropped destroys the coroutine (Go for
        // a task nobody waits for)
        [[nodiscard]] Task& Spawn() {
            (void)_t.spawn();
            return *this;
        }

        // By hand, on this thread: to its next suspension
        void Resume() {
            _t.resume();
        }

        bool IsDone() const noexcept {
            return _t.done();
        }

        // Waits for the task on this thread: its result, or what it threw
        decltype(auto) Join() {
            return _t.join();
        }

        // The result of a task that is done, or what it threw
        decltype(auto) Result() {
            return _t.result();
        }

        // `co_await task` in a task: suspended until it is done, its result
        auto operator co_await() noexcept {
            return _t.operator co_await();
        }

        // Lets go of the task: it runs on, its frame the collector's once
        // it is done
        void Detach() noexcept {
            _t.detach();
        }

        // Destroys the coroutine now: for a task that never ran or is done
        void Destroy() noexcept {
            _t.destroy();
        }

        InnerType& Inner() noexcept {
            return _t;
        }

        const InnerType& Inner() const noexcept {
            return _t;
        }

    private:
        InnerType _t;
    };

    template<class T>
    class Generator {
    public:
        using ValueType = T;
        using InnerType = sgcl::generator<T>;
        using promise_type = typename InnerType::promise_type;

        Generator() noexcept = default;
        Generator(Generator&&) noexcept = default;
        Generator& operator=(Generator&&) noexcept = default;
        Generator(const Generator&) = delete;
        Generator& operator=(const Generator&) = delete;

        Generator(InnerType&& g) noexcept
        : _g(std::move(g)) {
        }

        // To the next value: false when the coroutine is done
        bool Next() {
            return _g.next();
        }

        const T& Value() const noexcept {
            return _g.value();
        }

        void Destroy() noexcept {
            _g.destroy();
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

    template<class T>
    auto begin(Generator<T>& g) {
        return g.Inner().begin();
    }

    template<class T>
    auto end(Generator<T>& g) noexcept {
        return g.Inner().end();
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
