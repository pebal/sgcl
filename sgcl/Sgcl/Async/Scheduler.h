//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The scheduler as the program sees it: Spawn(t) puts a Task on the queue
// of the ready, Go(t) spawns it and lets go of it, `co_await Yield()`
// sends a task to the back of the queue; Scheduler for the workers.
#pragma once

#include "../../async/scheduler.h"
#include "Coroutine.h"

#include <utility>

namespace Sgcl {
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

    // The same for a coroutine function with captures: `Spawn([x]() ->
    // Task<int> { ... })`, no call; the closure is copied into a frame
    // that lives as long as the task, where the task of a temporary
    // closure would refer to one that died at the semicolon
    template<sgcl::detail::TaskFactory F>
    [[nodiscard]] auto Spawn(F f) {
        return Spawn(sgcl::detail::task_of(std::move(f)));
    }

    template<sgcl::detail::TaskFactory F>
    void Go(F f) {
        Go(sgcl::detail::task_of(std::move(f)));
    }

    // `co_await Yield()`: the task goes to the back of the queue
    using Yield = sgcl::yield;

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
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
