//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// SpawnBlocking(f): a blocking call from a task, `T r = co_await
// SpawnBlocking(f);`, run on a pool of threads apart from the scheduler's
// workers, its result (or exception) handed back through a Promise;
// BlockingTask<T> the handle, BlockingPool the pool as the program sees
// it (its statistics, WaitIdle, Stop).
#pragma once

#include "../../async/blocking.h"
#include "Promise.h"
#include "Coroutine.h"
#include "Scheduler.h"
#include "Time.h"

#include <utility>

namespace Sgcl {
    template<class T>
    class BlockingTask {
    public:
        using ValueType = T;
        using InnerType = sgcl::blocking_task<T>;

        BlockingTask() noexcept = default;

        BlockingTask(InnerType&& t) noexcept
        : _t(std::move(t)) {
        }

        bool IsDone() const noexcept {
            return _t.done();
        }

        // Waits for the result on this thread: not from a task on a worker
        T Join() {
            return _t.join();
        }

        // `co_await SpawnBlocking(f)`: the task suspended until the job ran
        auto operator co_await() noexcept {
            return _t.operator co_await();
        }

        // The promise the job fills: `t.Result().Inner().on_ready(f)` for a Select
        sgcl::promise<T>& Result() noexcept {
            return _t.result();
        }

        InnerType& Inner() noexcept {
            return _t;
        }

    private:
        InnerType _t;
    };

    // f queued for the pool: `T r = co_await SpawnBlocking(f);` from a
    // task, `SpawnBlocking(f).Join()` from a thread; f is moved into a
    // managed job, so it may capture Ptrs
    template<class F>
    auto SpawnBlocking(F f) {
        auto t = sgcl::spawn_blocking(std::move(f));
        return BlockingTask<typename decltype(t)::value_type>(std::move(t));
    }

    // The same, where it reads better: `co_await Blocking([&] { ... })`
    template<class F>
    auto Blocking(F f) {
        return SpawnBlocking(std::move(f));
    }

    // The pool as the program sees it: threads started on demand up to
    // config::BlockingThreads, gone after the idle time
    struct BlockingPool {
        struct Statistics {
            unsigned Threads = 0;   // the threads of the pool now
            unsigned Idle = 0;      // of them, parked with nothing to do
            size_t Queued = 0;      // jobs waiting for a thread
        };

        static Statistics GetStatistics() {
            auto s = sgcl::blocking_pool::get_statistics();
            return Statistics{s.threads, s.idle, s.queued};
        }

        static unsigned MaxThreads() noexcept {
            return sgcl::blocking_pool::max_threads();
        }

        static void SetIdleTime(Duration d) {
            sgcl::blocking_pool::set_idle_time(d);
        }

        static Duration IdleTime() {
            return sgcl::blocking_pool::idle_time();
        }

        // Blocks until every job queued so far has run
        static void WaitIdle() {
            sgcl::blocking_pool::wait_idle();
        }

        // The jobs queued run to the end, the threads joined; the next
        // SpawnBlocking starts the pool again
        static void Stop() {
            sgcl::blocking_pool::stop();
        }
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
