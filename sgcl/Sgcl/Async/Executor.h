//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Executor: a queue of tasks that one thread runs, the thread of the
// program's choosing (the main one, for a UI toolkit): Run() is that
// thread's loop, Run(task) the loop until the task is done, Poll() one
// pass for a foreign loop. Strand: an executor with no thread of its own,
// whose tasks run on the workers one at a time, in order. `co_await On(ex)`
// moves a task there, `co_await OnWorkers()` back to the pool; a task on
// an executor is woken on it, whatever woke it.
#pragma once

#include "../../async/executor.h"
#include "Coroutine.h"
#include "Scheduler.h"

#include <cstddef>
#include <utility>

namespace Sgcl {
    class Executor {
    public:
        using InnerType = sgcl::executor;

        Executor() = default;
        Executor(const Executor&) = delete;
        Executor& operator=(const Executor&) = delete;

        // The calling thread's loop, until Stop()
        void Run() {
            _ex.run();
        }

        // The loop until the task is done, and its result (or what it
        // threw): `return main.Run(Program());`
        template<class T>
        T Run(Task<T> t) {
            return _ex.run(std::move(t.Inner()));
        }

        void Run(Task<> t) {
            _ex.run(std::move(t.Inner()));
        }

        // The loop until the task is done (or Stop()); the task started
        // here when nobody started it
        template<class T>
        void RunUntil(Task<T>& t) {
            _ex.run_until(t.Inner());
        }

        // One pass over what is queued, and how many tasks ran
        size_t Poll() {
            return _ex.poll();
        }

        // Run() returns; the tasks stay queued for the next Run() or Poll()
        void Stop() {
            _ex.stop();
        }

        bool IsRunning() const noexcept {
            return _ex.running();
        }

        // A task started on this executor; Go for one nobody waits for
        template<class T>
        [[nodiscard]] Task<T> Spawn(Task<T> t) {
            return Task<T>(_ex.spawn(std::move(t.Inner())));
        }

        template<class T>
        void Go(Task<T> t) {
            _ex.go(std::move(t.Inner()));
        }

        InnerType& Inner() noexcept {
            return _ex;
        }

        const InnerType& Inner() const noexcept {
            return _ex;
        }

    private:
        InnerType _ex;
    };

    class Strand {
    public:
        using InnerType = sgcl::strand;

        Strand() = default;
        Strand(const Strand&) = delete;
        Strand& operator=(const Strand&) = delete;

        // A task started on this strand: run by a worker, in its turn
        template<class T>
        [[nodiscard]] Task<T> Spawn(Task<T> t) {
            return Task<T>(_s.spawn(std::move(t.Inner())));
        }

        template<class T>
        void Go(Task<T> t) {
            _s.go(std::move(t.Inner()));
        }

        // Whether a task of the strand runs or is queued at this moment
        bool IsBusy() const noexcept {
            return _s.busy();
        }

        InnerType& Inner() noexcept {
            return _s;
        }

        const InnerType& Inner() const noexcept {
            return _s;
        }

    private:
        InnerType _s;
    };

    // `co_await On(ex)`: the task goes on on the executor or the strand
    class On : public sgcl::on {
    public:
        explicit On(Executor& ex) noexcept
        : sgcl::on(ex.Inner()) {
        }

        explicit On(Strand& s) noexcept
        : sgcl::on(s.Inner()) {
        }
    };

    // `co_await OnWorkers()`: the task goes on on the pool of workers
    using OnWorkers = sgcl::on_workers;

    // The task started on the executor or the strand: `auto t = Spawn(f(), ex);`, `Go(f(), ex);`
    template<class T, class E>
    [[nodiscard]] Task<T> Spawn(Task<T> t, E& ex) {
        return ex.Spawn(std::move(t));
    }

    template<class T, class E>
    void Go(Task<T> t, E& ex) {
        ex.Go(std::move(t));
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
