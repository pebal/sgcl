//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Task<T>: a coroutine with a managed frame that runs on the scheduler,
// Spawn(t) puts it on the queue and Go(t) forgets it, Join() and
// `co_await t` wait for its result. Generator<T>: a coroutine that yields
// values, for a range-for. The promise is the one of the type inside, so
// a Task is a coroutine's return type; Scheduler and Yield the
// scheduler's.
#pragma once

#include "../../async/async_generator.h"
#include "../../async/coroutine.h"
#include "../../async/scheduler.h"
#include "../../async/when.h"
#include "../Containers/List.h"

namespace Sgcl {
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

    // The task put on the scheduler, for `auto t = Spawn(f());`
    template<class T>
    [[nodiscard]] Task<T> Spawn(Task<T> t) {
        (void)t.Spawn();
        return t;
    }

    // The task put on the scheduler and let go of: Go's `go f()`
    template<class T>
    void Go(Task<T> t) {
        t.Spawn().Detach();
    }

    // `co_await Yield()`: the task goes to the back of the queue
    using Yield = sgcl::yield;

    // The composition of tasks: `co_await WhenAll(a, b)` waits for every
    // task and gives their results as a tuple (a List for a List of
    // tasks of one type; nothing for tasks of nothing), what any threw
    // rethrown; `co_await WhenAny(a, b)` gives the index of the first to
    // finish and lets go of the rest. The tasks are taken over: spawned
    // ones, waited for here, not started here.
    template<class... T>
    Task<std::tuple<T...>> WhenAll(Task<T>... ts) {
        return sgcl::when_all(std::move(ts.Inner())...);   // the inner task as a Task: no frame of its own (Timeout.h)
    }

    template<class... Void>
    requires (std::is_void_v<Void> && ...)
    Task<> WhenAll(Task<Void>... ts) {
        return sgcl::when_all(std::move(ts.Inner())...);
    }

    template<class T>
    Task<List<T>> WhenAll(List<Task<T>> ts) {
        sgcl::vector<sgcl::task<T>> inner;
        inner.reserve(ts.Count());
        for (auto& t : ts) {
            inner.push_back(std::move(t.Inner()));
        }
        co_return List<T>(co_await sgcl::when_all(std::move(inner)));
    }

    inline Task<> WhenAll(List<Task<>> ts) {
        sgcl::vector<sgcl::task<>> inner;
        inner.reserve(ts.Count());
        for (auto& t : ts) {
            inner.push_back(std::move(t.Inner()));
        }
        return sgcl::when_all(std::move(inner));
    }

    template<class... T>
    Task<size_t> WhenAny(Task<T>... ts) {
        return sgcl::when_any(std::move(ts.Inner())...);
    }

    template<class T>
    Task<size_t> WhenAny(List<Task<T>> ts) {
        sgcl::vector<sgcl::task<T>> inner;
        inner.reserve(ts.Count());
        for (auto& t : ts) {
            inner.push_back(std::move(t.Inner()));
        }
        return sgcl::when_any(std::move(inner));
    }

    // The scheduler as the program sees it: the workers (one per core
    // unless SGCL_WORKERS says otherwise), started by the first Spawn
    struct Scheduler {
        static unsigned Workers() {
            return sgcl::scheduler::workers();
        }

        static bool OnWorker() noexcept {
            return sgcl::scheduler::on_worker();
        }

        // Joins the workers; the next Spawn starts them again
        static void Stop() {
            sgcl::scheduler::stop();
        }

        // The queues as they are: for the benchmarks and a look at a load
        struct Statistics {
            unsigned Workers = 0;        // the threads of the pool (0: not started)
            size_t GlobalQueued = 0;     // tasks on the global queue
            size_t LocalQueued = 0;      // tasks on the workers' rings and next slots, together
            unsigned Spinning = 0;       // workers looking for work
            unsigned Sleeping = 0;       // workers asleep in the kernel
        };

        static Statistics GetStatistics() {
            auto s = sgcl::scheduler::get_statistics();
            return Statistics{s.workers, s.global_queued, s.local_queued, s.spinning, s.sleeping};
        }
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

    // A generator that may wait: co_yields values and co_awaits between
    // them, consumed from a task with `while (auto v = co_await g.Next())`
    template<class T>
    class AsyncGenerator {
    public:
        using ValueType = T;
        using InnerType = sgcl::async_generator<T>;
        using promise_type = typename InnerType::promise_type;

        AsyncGenerator() noexcept = default;
        AsyncGenerator(AsyncGenerator&&) noexcept = default;
        AsyncGenerator& operator=(AsyncGenerator&&) noexcept = default;
        AsyncGenerator(const AsyncGenerator&) = delete;
        AsyncGenerator& operator=(const AsyncGenerator&) = delete;

        AsyncGenerator(InnerType&& g) noexcept
        : _g(std::move(g)) {
        }

        // `co_await g.Next()`: the next value, or None at the end; rethrows
        auto Next() noexcept {
            return _g.next();
        }

        bool IsDone() const noexcept {
            return _g.done();
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

