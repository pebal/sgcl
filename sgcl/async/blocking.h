//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../concurrent/queue.h"
#include "../core/config.h"
#include "../core/detail/env.h"
#include "../core/make_tracked.h"
#include "../core/root_ptr.h"
#include "../core/tracked_ptr.h"
#include "../core/unique_ptr.h"
#include "promise.h"
#include "scheduler.h"
#include "timer.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <list>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // A blocking call from a task: `T r = co_await spawn_blocking(f);`
    // runs f on a pool of threads apart from the scheduler's workers and
    // hands what it returns (or throws) back through a promise
    // (promise.h), the task holding no thread meanwhile; `go_blocking(f);`
    // runs it with no handle, what it throws to on_unhandled. A worker runs
    // every task that is ready; a call that blocks it (a file read
    // without the reactor, getaddrinfo, a C library, a database driver)
    // takes it from all of them for as long as the call lasts, so such a
    // call goes to the blocking pool, whose threads are meant to sit in
    // the kernel: tokio's spawn_blocking, Go's answer being a goroutine
    // whose thread the runtime replaces while it is in the call.
    //
    // The pool, one per process: a queue of jobs and threads that take
    // from it, started with the first job that finds no idle thread and
    // grown one thread per such job up to config::blocking_threads (the
    // larger of 64 and four times the hardware concurrency, by default:
    // the threads are expected to block, so there are more of them than
    // cores); a thread that finds the queue empty parks for the idle
    // time (config::blocking_idle_milliseconds, 10 s; blocking_pool::
    // set_idle_time) and exits when nothing came, so that a program that
    // stopped blocking has no threads for it. A job is a managed object
    // holding the closure and the promise: the closure lives there, so
    // that what it captured (a tracked_ptr to the buffer being read into)
    // is traced through the job's pointer map, and the promise is where
    // the task waits. The queue is a concurrent::queue in a managed object
    // the pool owns (as the scheduler's global queue is), lock-free, and a
    // push holds the job while it waits; the pool's mutex covers its
    // counts only (the idle threads, the wake credits, the jobs pending,
    // the threads themselves), taken once per job by the submitter and
    // once per batch of jobs by a thread, which the thread wake it is
    // there to arbitrate costs far more than (the round trip of a job
    // that does nothing is 6 to 7 us: two hand-offs through the
    // kernel). A thread
    // of the pool is a thread of the program to the collector, like any
    // other. scheduler::stop() stops the pool too, after the timers and
    // the reactor; blocking_pool::stop() and wait_idle() are the pool's
    // own.
    namespace detail {
        class BlockingPool;
        inline BlockingPool& blocking_pool_instance();

        // A job: run by a thread of the pool, once
        struct BlockingJobBase {
            virtual ~BlockingJobBase() = default;
            virtual void run() noexcept = 0;

            // Set under the pool's lock when its submit could start no
            // thread and threw: never run, passed by when a thread pops
            // it (a thread that pops it is started under the lock later)
            std::atomic<bool> dropped = {false};
        };

        // A job with its result: the promise the task awaits
        template<class T>
        struct BlockingJob : BlockingJobBase {
            PromiseState<T> result;   // the state itself, in the job: one object
        };

        // A job with its closure: what it returns or throws goes to the promise
        template<class F, class T>
        struct BlockingJobOf : BlockingJob<T> {
            SGCL_INLINE_HOT explicit BlockingJobOf(F f) noexcept(std::is_nothrow_move_constructible_v<F>)
            : f(std::move(f)) {
            }

            void run() noexcept override {
                try {
                    if constexpr (std::is_void_v<T>) {
                        f();
                        this->result.set_value();
                    } else {
                        this->result.set_value(f());
                    }
                } catch (...) {
                    this->result.set_exception(std::current_exception());
                }
            }

            F f;
        };

        // A job of go_blocking, with no promise: nobody reads what f
        // gives, so what it returns is dropped and what it throws goes to
        // on_unhandled's handler (coroutine.h), on the thread of the pool
        // that ran it, as a detached task's goes there from the worker
        // that ends it (the default's line names the pool: thread_place)
        template<class F>
        struct BlockingJobDetached : BlockingJobBase {
            SGCL_INLINE_HOT explicit BlockingJobDetached(F f) noexcept(std::is_nothrow_move_constructible_v<F>)
            : f(std::move(f)) {
            }

            void run() noexcept override {
                std::exception_ptr error;
                try {
                    (void)f();
                } catch (...) {
                    error = std::current_exception();
                }
                if (error) {
                    unhandled_handler.load(std::memory_order_acquire)(error);
                }
            }

            F f;
        };

        // Called by the pool before it makes a thread, when set: a test's
        // way to have the start fail as std::thread's does (it throws
        // std::system_error); tests/async/blocking.cpp
        inline std::atomic<void (*)()> blocking_start_test_hook = {nullptr};

        class BlockingPool {
        public:
            SGCL_INLINE_HOT BlockingPool() {
                scheduler_instance();   // made after the scheduler, so destroyed before it (timer.h: Timers)
            }

            struct Queue {
                concurrent::queue<tracked_ptr<BlockingJobBase>> jobs;
            };

            struct Statistics {
                unsigned threads = 0;   // the threads of the pool now (0: none, or not started)
                unsigned idle = 0;      // of them, parked with nothing to do
                size_t queued = 0;      // jobs waiting for a thread
            };

            SGCL_INLINE_HOT ~BlockingPool() {
                stop();
            }

            // The job queued, and a thread woken for it or started; the
            // push is outside the lock (the queue is lock-free), the
            // decision under it, so that a thread about to park sees the
            // push (its look at the queue is under the lock too) or is
            // seen here as idle and woken
            void submit(const tracked_ptr<BlockingJobBase>& job) {
                std::unique_lock lock(_m);
                if (!_queue) {
                    _queue = make_tracked<Queue>();
                    scheduler_stop_hook3.store([] { blocking_pool_instance().stop(); }, std::memory_order_release);   // scheduler::stop() stops the pool too
                }
                _queue->jobs.push(job);
                ++_pending;
                if (_idle > 0) {
                    --_idle;   // the credit: this thread counts as woken from here, and the one that wakes takes it
                    ++_notify;
                    lock.unlock();
                    _cv.notify_one();
                } else if (_threads.size() < _cap()) {
                    _start(lock, job);
                }
                // every thread busy and the pool at its cap: the job waits
                // in the queue for the next thread to finish its job
            }

            // Every thread woken to drain the queue and exit, then joined;
            // a job queued meanwhile is run before the last thread goes
            // (the next submit starts the pool again)
            void stop() {
                std::unique_lock lock(_m);
                if (_threads.empty() && _finished.empty()) {
                    return;
                }
                _stop = true;
                _cv.notify_all();
                _exit_cv.wait(lock, [&] { return _threads.empty(); });
                auto finished = std::move(_finished);
                _finished.clear();
                _stop = false;
                _queue.reset();
                lock.unlock();
                for (auto& t : finished) {
                    t.join();
                }
            }

            // Blocks until every job queued so far has run
            SGCL_INLINE_HOT void wait_idle() {
                std::unique_lock lock(_m);
                _idle_cv.wait(lock, [&] { return _pending == 0; });
            }

            Statistics statistics() {
                Statistics st;
                std::lock_guard lock(_m);
                st.threads = (unsigned)_threads.size();
                st.idle = _idle;
                st.queued = _queue ? _queue->jobs.size() : 0;
                return st;
            }

            SGCL_INLINE_HOT void set_idle_time(duration d) {
                std::lock_guard lock(_m);
                _idle_time = d;
            }

            SGCL_INLINE_HOT duration idle_time() {
                std::lock_guard lock(_m);
                return _idle_time;
            }

            // The most threads the pool grows to: set_threads', else
            // SGCL_BLOCKING_THREADS (read once, at the first need), else
            // config::blocking_threads; 0 in any of them the larger of 64
            // and four times the hardware concurrency. A smaller number
            // stops the growth at once; the threads over it exit as they
            // run out of work (the idle time)
            SGCL_INLINE_HOT unsigned cap() {
                std::lock_guard lock(_m);
                return _cap();
            }

            SGCL_INLINE_HOT void set_cap(unsigned n) {
                std::lock_guard lock(_m);
                _cap_set = true;
                _cap_asked = n;
            }

        private:
            using Threads = std::list<std::thread>;

            // Under _m
            SGCL_INLINE_HOT unsigned _cap() noexcept {
                if (!_cap_set && !_cap_env_read) {
                    _cap_env_read = true;
                    _cap_asked = env_unsigned("SGCL_BLOCKING_THREADS", config::blocking_threads);
                }
                return _cap_asked ? _cap_asked : std::max(64u, 4 * std::thread::hardware_concurrency());
            }

            // A thread started for the job, under the lock: the handles of
            // the threads that exited on their own are joined after (they
            // are done). A thread that cannot be started (std::thread's
            // std::system_error) leaves no entry, which stop() would wait
            // for and the cap would count. With another thread there the
            // job waits for it in the queue, as at the cap: every thread
            // looks at the queue before it parks or leaves, and none is
            // idle (a submit with one idle wakes it and starts nothing).
            // With none the job could only wait for the next submit: it is
            // dropped instead, owed to nobody (_pending), and the throw is
            // its submitter's, so a spawn_blocking that throws never runs
            // its function
            void _start(std::unique_lock<std::mutex>& lock, const tracked_ptr<BlockingJobBase>& job) {
                auto finished = std::move(_finished);
                _finished.clear();
                auto it = _threads.emplace(_threads.end());
                try {
                    if (auto hook = blocking_start_test_hook.load(std::memory_order_relaxed)) [[unlikely]] {
                        hook();
                    }
                    *it = std::thread([this, it] { _run(it); });
                } catch (...) {
                    _threads.erase(it);
                    bool alone = _threads.empty();
                    if (alone) {
                        job->dropped.store(true, std::memory_order_relaxed);
                        if (--_pending == 0) {
                            _idle_cv.notify_all();
                        }
                    }
                    lock.unlock();
                    for (auto& t : finished) {
                        t.join();
                    }
                    if (alone) {
                        throw;
                    }
                    return;
                }
                lock.unlock();
                for (auto& t : finished) {
                    t.join();
                }
            }

            // A thread of the pool: jobs while there are any, then parked
            // for the idle time; woken with a credit (a job came) it looks
            // again, woken without (a stop) it drains and exits, timed out
            // it exits. A job pushed between the last pop and the park is
            // seen by the look under the lock, since the push comes before
            // the submitter's own lock.
            void _run(Threads::iterator it) {
                thread_place = PlaceBlockingPool;   // on_unhandled's default names the pool (coroutine.h)
                const uintptr_t floor = dead_stack_floor();
                for (;;) {
                    unsigned ran = _run_jobs();
                    std::unique_lock lock(_m);
                    if ((_pending -= ran) == 0) {
                        _idle_cv.notify_all();   // wait_idle: every job queued so far ran
                    }
                    if (!_queue->jobs.empty()) {
                        continue;
                    }
                    if (_stop) {
                        _leave(it);
                        return;
                    }
                    clear_dead_stack(floor);   // the words the jobs' frames left: the job, and through its closure what it holds (scheduler.h)
                    ++_idle;
                    auto deadline = std::chrono::steady_clock::now() + _idle_time;
                    bool credited = false;
                    for (;;) {
                        bool timed_out = _cv.wait_until(lock, deadline) == std::cv_status::timeout;
                        if (_notify > 0) {   // a job came: the submitter counted this thread out of idle
                            --_notify;
                            credited = true;
                            break;
                        }
                        if (_stop || timed_out) {
                            break;
                        }
                    }
                    if (!credited) {
                        --_idle;
                        if (!_stop) {
                            _leave(it);   // idle for the whole time: gone
                            return;
                        }
                    }
                }
            }

            // The jobs queued, run while there are any; how many. A frame
            // of its own below _run's, so that everything of a job — its
            // pointer and the temporaries of the call, which an unoptimized
            // build keeps in the frame and not in a register — is in the
            // dead stack clear_dead_stack zeroes, not in _run's live frame
            SGCL_NOINLINE unsigned _run_jobs() noexcept {
                unsigned ran = 0;
                while (auto job = _queue->jobs.try_pop()) {
                    if ((*job)->dropped.load(std::memory_order_relaxed)) {
                        continue;   // its submit threw (_start): not run, and not owed
                    }
                    (*job)->run();
                    ++ran;
                }
                return ran;
            }

            // The thread's exit, under the lock its decision was taken
            // under: its handle to the finished list, to be joined by a
            // start or a stop (a thread cannot join itself). One lock for
            // the decision and the leave, so that a submit between the two
            // never counts a thread that is neither idle nor going to look
            // at the queue again (it would neither wake nor start one, and
            // the job would wait for the next submit)
            SGCL_INLINE_HOT void _leave(Threads::iterator it) noexcept {
                _finished.splice(_finished.end(), _threads, it);
                if (_threads.empty()) {
                    _exit_cv.notify_all();
                }
            }

            std::mutex _m;
            std::condition_variable _cv;        // the parked threads
            std::condition_variable _exit_cv;   // stop(): the last thread gone
            std::condition_variable _idle_cv;   // wait_idle(): nothing queued, nothing running
            unique_ptr<Queue> _queue;
            Threads _threads;                   // the threads alive, guarded by _m
            Threads _finished;                  // exited, not yet joined
            unsigned _idle = 0;                 // threads parked and not yet credited with a job
            unsigned _notify = 0;               // credits handed out and not yet taken
            size_t _pending = 0;                // jobs queued or running
            bool _stop = false;
            duration _idle_time = std::chrono::milliseconds(config::blocking_idle_milliseconds);
            unsigned _cap_asked = 0;         // set_cap, or SGCL_BLOCKING_THREADS, or config::blocking_threads (under _m)
            bool _cap_set = false;
            bool _cap_env_read = false;
        };

        inline BlockingPool& blocking_pool_instance() {
            static BlockingPool pool;
            return pool;
        }
    }

    // The handle of a job on the pool: `co_await` gives what f returned,
    // or rethrows what it threw, and so does wait() for a thread; the job
    // runs whether or not the handle is kept (a handle dropped is a job
    // whose result nobody reads, and the job is the collector's once it
    // ran). A root_ptr to the job, as a task's handle is to its frame, so
    // that the handle lives anywhere, a std::vector of handles included,
    // at the cost of a cell per handle (root_ptr.h); move-only.
    template<class T>
    class blocking_task {
    public:
        using value_type = T;

        blocking_task() noexcept = default;
        blocking_task(blocking_task&&) noexcept = default;
        blocking_task& operator=(blocking_task&&) noexcept = default;
        blocking_task(const blocking_task&) = delete;
        blocking_task& operator=(const blocking_task&) = delete;

        SGCL_INLINE_HOT bool done() const noexcept {
            return _job && _job->result.done();
        }

        // Waits for the job, on this thread: what f returned, or what it
        // threw, rethrown. Not from a task on a worker
        SGCL_INLINE_HOT T wait() {
            if constexpr (std::is_void_v<T>) {
                _job->result.wait();
            } else {
                return std::move(_job->result.wait());
            }
        }

        // `co_await spawn_blocking(f)`: the task suspended until the job ran
        class awaiter {
        public:
            SGCL_INLINE_HOT bool await_ready() {
                return _aw.await_ready();
            }

            template<class P>
            SGCL_INLINE_HOT bool await_suspend(std::coroutine_handle<P> h) {
                return _aw.await_suspend(h);
            }

            SGCL_INLINE_HOT T await_resume() {
                if constexpr (std::is_void_v<T>) {
                    _aw.await_resume();
                } else {
                    return std::move(_aw.await_resume());
                }
            }

        private:
            friend class blocking_task;

            SGCL_INLINE_HOT explicit awaiter(const tracked_ptr<detail::BlockingJob<T>>& job) noexcept
            : _job(job)
            , _aw(_job->result.operator co_await()) {
            }

            tracked_ptr<detail::BlockingJob<T>> _job;   // held here too: the awaiter outlives the handle it came from
            typename detail::PromiseState<T>::awaiter _aw;
        };

        SGCL_INLINE_HOT awaiter operator co_await() noexcept {
            return awaiter(_job.ptr());
        }

        // What f returned, or what it threw, as a task's result() has it:
        // waited for first, on this thread, when the job has not run yet;
        // a reference into the job (void for a job of nothing)
        SGCL_INLINE_HOT decltype(auto) result() {
            return _job->result.result();
        }

        // A case of a select: f() once the job ran
        template<class F>
        SGCL_INLINE_HOT auto on_done(F f) noexcept(std::is_nothrow_move_constructible_v<F>) {
            return _job->result.on_done(std::move(f));
        }

    private:
        template<class F>
        friend auto spawn_blocking(F f);

        // From the job, by spawn_blocking
        SGCL_INLINE_HOT explicit blocking_task(const tracked_ptr<detail::BlockingJob<T>>& job) noexcept
        : _job(job) {
        }

        root_ptr<detail::BlockingJob<T>> _job;
    };

    // f queued for the pool: `T r = co_await sgcl::async::spawn_blocking(f);`
    // from a task, `spawn_blocking(f).wait()` from a thread. f is moved
    // into the job, a managed object, so it may capture tracked pointers;
    // what it captures by reference must outlive the job, which a task's
    // locals do while the task awaits it
    template<class F>
    SGCL_INLINE_HOT auto spawn_blocking(F f) {
        using T = std::decay_t<std::invoke_result_t<F&>>;
        tracked_ptr<detail::BlockingJobOf<F, T>> job = make_tracked<detail::BlockingJobOf<F, T>>(std::move(f));
        job->result.link();   // the promise's channel, before the job is given to the pool (ChannelState::link)
        detail::blocking_pool_instance().submit(job);
        return blocking_task<T>(job);
    }

    // f queued for the pool and let go of, go()'s counterpart for a
    // blocking call: `sgcl::async::go_blocking(f);` runs f on a thread of
    // the pool, keeps no handle, drops what f returns, and gives what f
    // throws to on_unhandled's handler, which a dropped handle of
    // spawn_blocking would leave in a promise nobody reads. The same job
    // and the same pool as spawn_blocking's, so the same rules: f may
    // capture tracked pointers, and what it captures by reference must
    // outlive the job; the call throws as spawn_blocking's does
    template<class F>
    SGCL_INLINE_HOT void go_blocking(F f) {
        tracked_ptr<detail::BlockingJobDetached<F>> job = make_tracked<detail::BlockingJobDetached<F>>(std::move(f));
        detail::blocking_pool_instance().submit(job);
    }

    // The pool as the program sees it
    struct blocking_pool {
        using statistics = detail::BlockingPool::Statistics;

        // The threads, the idle ones among them, the jobs waiting
        SGCL_INLINE_HOT static statistics get_statistics() {
            return detail::blocking_pool_instance().statistics();
        }

        // The most threads the pool grows to: set_threads', else
        // SGCL_BLOCKING_THREADS from the environment (read once), else
        // config::blocking_threads; 0 in any of them the larger of 64 and
        // four times the hardware concurrency
        SGCL_INLINE_HOT static unsigned max_threads() {
            return detail::blocking_pool_instance().cap();
        }

        // The most threads from now on (0: the default above). A smaller
        // number stops the growth at once; threads over it exit as they
        // run out of work, after the idle time
        SGCL_INLINE_HOT static void set_threads(unsigned n) {
            detail::blocking_pool_instance().set_cap(n);
        }

        // How long an idle thread waits for a job before it exits
        // (config::blocking_idle_milliseconds by default)
        SGCL_INLINE_HOT static void set_idle_time(duration d) {
            detail::blocking_pool_instance().set_idle_time(d);
        }

        SGCL_INLINE_HOT static duration idle_time() {
            return detail::blocking_pool_instance().idle_time();
        }

        // Blocks until every job queued so far has run
        SGCL_INLINE_HOT static void wait_idle() {
            assert(!detail::on_worker() && "wait_idle() blocks the worker");
            detail::blocking_pool_instance().wait_idle();
        }

        // The jobs queued run to the end and the threads joined: for a
        // program that wants its threads gone at a point of its own (the
        // end of the program, and scheduler::stop(), do it); the next
        // spawn_blocking starts the pool again
        SGCL_INLINE_HOT static void stop() {
            assert(!detail::on_worker() && "stop() blocks the worker");
            detail::blocking_pool_instance().stop();
        }
    };
}
