//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "operation.h"
#include "../core/coroutine.h"
#include "../core/detail/frame_word.h"
#include "../core/tracked_ptr.h"

#include <atomic>
#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

namespace sgcl::async::detail {
    using namespace sgcl::detail;
    void enqueue(tracked_ptr<FrameWord> frame, bool next);   // scheduler.h: the coroutine made ready, to run next on this worker or later
    bool on_worker() noexcept;                                                // scheduler.h: whether this thread is a worker

    struct ExecutorQueue;   // scheduler.h: the queue of an executor or a strand (executor.h)
    template<class> struct TimeoutRace;   // timeout.h: a task raced against a deadline
    struct TaskLocals;      // task_local.h: the values of a task's task_locals, a chain of nodes

    // The link of a frame on an executor's queue (scheduler.h:
    // ExecutorQueue): a tracked word, stored through the barrier as every
    // other, with the two operations the queue needs and tracked_ptr does
    // not give — a load with an order of the caller's, for the consumer,
    // which reads a link a producer stored and must see the frame as the
    // producer left it, and a store with one. The load takes no hazard
    // pointer, as the atomics' load does: the frame it reads is held by
    // the queue while it is linked, and only the queue's consumer reads
    // it, so it cannot be collected between the load and the copy. It
    // lives in a frame's header only, which is zeroed memory and never
    // constructed: never made, copied or destroyed as an object.
    struct FrameLink
    : tracked_ptr<FrameWord> {
        using tracked_ptr<FrameWord>::operator=;

        FrameWord* load(std::memory_order m) const noexcept {
            return (FrameWord*)_ptr()->load(m);
        }

        void store(FrameWord* frame, std::memory_order m) noexcept {
            _ptr()->store(frame, m);
        }
    };

    // The header of a managed frame as the async module lays it out, in
    // the words the core keeps in front of every managed frame's buffer
    // (core/coroutine.h: FrameHeaderWords, managed_frame's operator new;
    // the coroutine's own frame starts past them): where the coroutine
    // runs, what its task-locals are, and the link of the queue of an
    // executor it may be on. The executor
    // (executor.h) is the queue that enqueue (scheduler.h)
    // routes the frame to when it is made ready, null for the pool of
    // workers; a coroutine that moves to an executor sets it, and every
    // wake of the frame from then on (a channel, a timer, a task it
    // awaited, a yield) lands on that executor. The locals are the head
    // of the chain of the values set by task_local::set, inherited by
    // the tasks this one starts (copied by the start: a child that sets a
    // value puts a node of its own in front, so the parent's chain never
    // changes under it). Both are tracked words inside the frame's
    // buffer, traced as every word of it is: the queue and the chain live
    // while the frame does. Written only by the coroutine that owns the
    // frame, while it runs, and read by whoever makes it ready after
    // that, through the same order the wait itself needs. The link is
    // the executor's queue's own (scheduler.h: ExecutorQueue, an
    // intrusive list, so that a push allocates nothing): the frame queued
    // after this one while this one is queued there, null at every other
    // moment, since a frame is made ready once per suspension and so sits
    // on one queue at a time. The fourth word is unused: the header is
    // kept at a multiple of sixteen bytes, so that the coroutine's frame
    // past it stays as aligned as the buffer (array_base.h: sixteen), which
    // is what operator new promises a frame. A header copied (a task that
    // takes its resumer's) takes the executor and the locals, never the
    // link, which is the queue's.
    struct FrameHeader {
        tracked_ptr<ExecutorQueue> executor;
        tracked_ptr<TaskLocals> locals;
        FrameLink next;
        FrameWord unused;

        FrameHeader& operator=(const FrameHeader& h) noexcept {
            executor = h.executor;
            locals = h.locals;
            return *this;
        }
    };

    static_assert(sizeof(FrameHeader) == FrameHeaderWords * sizeof(FrameWord));   // exactly the words the core keeps in front of the frame
    static_assert(sizeof(FrameHeader) % 16 == 0);

    // The header of a frame named by its buffer's address (the promise's
    // `self`, every queue's and waiter's word; core/coroutine.h: handle_of)
    inline FrameHeader& frame_header(void* frame) noexcept {
        return *(FrameHeader*)frame;
    }

    // The frame the calling thread is running (a worker, an executor's
    // thread, a thread that resumes a task by hand), null outside one:
    // set around every resume (scheduler.h: resume_frame), so that a
    // function the task calls finds the task's locals (task_local.h) and
    // a task started from it inherits them
    inline thread_local FrameWord* current_frame = nullptr;
}

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    namespace detail {
        // What every task's promise has besides its value: the state of
        // the task (running until its final suspension; done after) for
        // the ones that wait for it, a thread on the word (join), a
        // coroutine by its handle (co_await task), one of each at most.
        // At the final suspension the coroutine marks itself done, wakes
        // the thread and hands the coroutine to the scheduler; the frame
        // stays, suspended, for the result. A raw handle is a word the
        // collector does not follow: the continuation's frame is held by
        // its own tracked_ptr next to the handle.
        // What a task's end may resume instead of a coroutine: an object
        // called with itself, on the finishing task's worker, from the
        // final suspension (quick, nothrow, the task's frame not touched
        // after: it may be destroyed right behind the call). A race
        // (timeout.h) awaits a task so, and decides in the call whether
        // the task or the deadline came first.
        struct Continuation {
            void (*done)(Continuation*);
        };

        struct TaskPromiseBase : managed_frame {
            // One word: Running, Done, the handle of the coroutine awaiting
            // the task, or a Continuation with the Call bit (a handle is
            // aligned to more than that). The awaiter installs its handle
            // with a compare-exchange against Running (its frame stored
            // before); the final suspension exchanges Done in and, finding
            // a handle, hands it to the scheduler, finding a continuation,
            // calls it. Whichever comes second sees the first: an awaiter
            // that finds Done resumes at once, a finish that finds a
            // handle resumes it; never both.
            static constexpr uintptr_t Running = 0;
            static constexpr uintptr_t Done = 1;
            static constexpr uintptr_t Call = 2;

            std::atomic<uintptr_t> state = {Running};
            std::atomic<uint32_t> waiters = {0};   // threads in wait(): the end notifies only when there is one (a notify with nobody waiting is a fetch-add and a fence on a table the library shares, per task end)
            std::atomic<bool> started = {false};   // spawned, resumed, or started by the first wait for it
            std::atomic<bool> released = {false};  // the task object let go of (detach), or the coroutine finished: the second of the two destroys the frame
            tracked_ptr<FrameWord> continuation_frame;
            tracked_ptr<void> continuation_keep;   // the continuation's object, held while the task runs
            std::exception_ptr error;

            // The awaiter of the final suspension: done, and everyone told
            struct final_awaiter {
                bool await_ready() noexcept {
                    return false;
                }

                template<class P>
                void await_suspend(std::coroutine_handle<P> h) noexcept {
                    TaskPromiseBase& p = h.promise();
                    auto was = p.state.exchange(Done, std::memory_order_seq_cst);
                    if (p.waiters.load(std::memory_order_seq_cst)) {   // the store then the load, seq_cst on both sides (wait(): the increment then the load): the waiter sees Done or the end sees the waiter
                        p.state.notify_all();
                    }
                    if (was & Call) {
                        auto c = reinterpret_cast<Continuation*>(was & ~(Call | Done));
                        c->done(c);
                        p.continuation_keep = nullptr;
                    } else if (was != Running) {
                        enqueue(p.continuation_frame, true);
                        p.continuation_frame = nullptr;   // not kept: a done task would hold its awaiter's frame (and, through the dead frame's words, what they point at) for as long as the task object lives
                    }
                    // A detached task destroys itself here: its locals and
                    // parameters (a task it awaited, a root_ptr) released, the
                    // memory the collector's. Nothing of the frame is touched
                    // past the destroy.
                    if (p.released.exchange(true, std::memory_order_acq_rel)) {
                        h.destroy();
                    }
                }

                void await_resume() noexcept {
                }
            };

            final_awaiter final_suspend() noexcept {
                return {};
            }

            void unhandled_exception() noexcept {
                error = std::current_exception();
            }

            bool done() const noexcept {
                return state.load(std::memory_order_acquire) == Done;
            }

            // A coroutine that awaits this task: its handle installed,
            // unless the task is done already (false: it goes on at once)
            bool await(std::coroutine_handle<> c, tracked_ptr<FrameWord> frame) noexcept {
                continuation_frame = std::move(frame);
                uintptr_t e = Running;
                if (state.compare_exchange_strong(e, (uintptr_t)c.address(), std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
                assert(e == Done && "a task is awaited by one coroutine at a time");   // a second awaiter would have overwritten the first's frame above, and the first would be resumed with none
                continuation_frame = nullptr;
                return false;
            }

            // A continuation installed the same way: called at the task's
            // end, unless the task is done already (false)
            bool await(Continuation* c, tracked_ptr<void> keep) noexcept {
                continuation_keep = std::move(keep);
                uintptr_t e = Running;
                if (state.compare_exchange_strong(e, reinterpret_cast<uintptr_t>(c) | Call, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
                assert(e == Done && "a task is awaited by one coroutine at a time");
                continuation_keep = nullptr;
                return false;
            }

            void wait() noexcept {
                if (state.load(std::memory_order_acquire) == Done) {
                    return;
                }
                waiters.fetch_add(1, std::memory_order_seq_cst);
                for (auto v = state.load(std::memory_order_seq_cst); v != Done; v = state.load(std::memory_order_seq_cst)) {
                    state.wait(v, std::memory_order_seq_cst);
                }
                waiters.fetch_sub(1, std::memory_order_relaxed);
            }
        };
    }

    // A coroutine that runs on the scheduler (scheduler.h) or by hand:
    // spawn() puts it on the queue of the ready and returns at once, a
    // worker runs it to its next suspension; resume() runs it on the
    // calling thread instead, a step at a time. What it returns is kept
    // for result(): a thread waits for it with join(), a coroutine with
    // `co_await task`, which suspends it until the task is done, with no
    // thread held meanwhile; either rethrows what the task threw. A task
    // nobody waits for is let go of, go() (spawn and detach): its frame
    // lives while it is queued, waiting or running (the queue, the
    // channel's waiter or the worker hold it) and is the collector's once
    // it is done. The task object destroys the coroutine when it goes,
    // so a task spawned must be held or detached: spawn() is nodiscard,
    // and go() is the spawn whose handle nobody keeps. The frame
    // is managed (managed_frame): the task's locals and parameters are
    // roots while it lives. Move-only, one word of frame and one of
    // handle. task<void> for a coroutine that returns nothing.
    template<class T = void>
    class task {
    public:
        // The promise the compiler drives: a managed frame, suspended at
        // the start (spawn or resume runs it) and at the end, the value or
        // the exception kept for result()
        struct promise_type : detail::TaskPromiseBase {
            optional<T> value;

            task get_return_object() {
                return task(std::coroutine_handle<promise_type>::from_promise(*this));
            }

            std::suspend_always initial_suspend() noexcept {
                return {};
            }

            void return_value(T v) {
                value.emplace(std::move(v));
            }
        };

        // The awaiter of `co_await task`: suspends the awaiting coroutine
        // until the task is done (not at all when it is), then its result
        class awaiter {
        public:
            bool await_ready() const noexcept {
                return _task.done();
            }

            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) noexcept {
                auto frame = detail::frame_of(h);
                _task._start(detail::frame_header(frame.get()).executor);   // a task nobody started yet: on the scheduler now, where this one runs
                return _task._frame.promise().await(h, std::move(frame));
            }

            T await_resume() {
                return std::move(_task.result());
            }

        private:
            friend class task;

            explicit awaiter(task& t) noexcept
            : _task(t) {
            }

            task& _task;
        };

        template<class> friend struct detail::TimeoutRace;   // timeout.h: starts the task and installs its continuation

        task() noexcept = default;

        // On the scheduler: queued, run by a worker to its next suspension.
        // Nodiscard: the task object destroys the coroutine when dropped;
        // keep it, or go() the task instead
        [[nodiscard]] task& spawn() {
            assert(_frame && !_frame.done() && "a task is spawned once, before it runs");
            [[maybe_unused]] bool first = _start();
            assert(first && "a task is spawned once");
            return *this;
        }

        // By hand, on this thread: to its next suspension
        void resume() {
            auto& p = _frame.promise();
            if (!p.started.load(std::memory_order_relaxed)) {   // its first run, by hand: the frame takes the resumer's header (its executor, its task-locals), as a task started on the scheduler does (_start), so that what it starts inherits them and its own waits bring it back where the resumer runs (task_group::go: the runner resumed by hand, the child started from it)
                if (auto parent = detail::current_frame) {
                    detail::frame_header(p.self.get()) = detail::frame_header(parent);
                }
            }
            p.started.store(true, std::memory_order_release);
            auto running = std::exchange(detail::current_frame, p.self.get());   // this thread runs the frame: its locals are the current ones
            _frame.resume();
            detail::current_frame = running;
        }

        bool done() const noexcept {
            return !_frame || _frame.promise().done();   // an empty task is done, as the page says: nothing is left to run
        }

        // Waits for the task, on this thread, and gives its result, or
        // rethrows what it threw (as std::this_thread::sync_wait gives a
        // sender's). Not from a task on a worker (co_await it there)
        T& wait() {
            _wait();
            return result();
        }

        // The result of the task, or what it threw: waited for first, on
        // this thread, when the task is not done yet
        T& result() {
            if (!done()) {
                _wait();
            }
            auto& p = _frame.promise();
            if (p.error) {
                std::rethrow_exception(p.error);
            }
            return *p.value;
        }

        awaiter operator co_await() noexcept {
            return awaiter(*this);
        }

        // Lets go of the task: it runs on (or stays wherever it waits) and
        // destroys its frame when it is done (its locals and parameters
        // with it), the memory the collector's from then on; a task that
        // never runs leaves its frame to the collector as it is
        void detach() noexcept {
            if (_frame.promise().released.exchange(true, std::memory_order_acq_rel)) {
                _frame.destroy();   // done already: destroyed now
            } else {
                (void)_frame.release();   // running or waiting: destroys itself when done
            }
        }

        // Destroys the coroutine now, its locals and promise with it: for
        // a task that never ran or is done
        void destroy() noexcept {
            _frame.destroy();
        }

    private:
        // The wait of wait() and result(): the task started if nobody
        // started it, this thread blocked until its end
        void _wait() {
            assert(!detail::on_worker() && "wait() blocks the worker: co_await the task from a task");
            _start();   // a task nobody started yet: on the scheduler now
            _frame.promise().wait();
        }

        friend class executor;   // executor.h: its spawn and run_until start the task and await its promise
        friend class strand;

        // The task put on the scheduler unless it was started already: the
        // first of spawn(), join() and co_await starts it; true when this
        // one did. Where it runs: the pool of workers, or the executor
        // given (executor.h: the awaiting task's, so that a task awaited
        // runs where its awaiter does, as a call would; an executor's
        // spawn). Its task-locals are the starting task's (the frame this
        // thread runs, if any): inherited by the copy of the chain's head
        bool _start(tracked_ptr<detail::ExecutorQueue> executor = nullptr) {
            auto& p = _frame.promise();
            if (p.started.exchange(true, std::memory_order_acq_rel)) {
                return false;
            }
            auto& header = detail::frame_header(p.self.get());
            if (executor || header.executor) {   // a null over a null spared its barrier
                header.executor = executor;
            }
            if (auto parent = detail::current_frame) {
                header.locals = detail::frame_header(parent).locals;
            }
            detail::enqueue(p.self, false);
            return true;
        }

        // The promise, for an awaiter of the task's end that leaves the
        // result where it is (executor.h: run_until)
        detail::TaskPromiseBase& _promise() const noexcept {
            return _frame.promise();
        }

        explicit task(std::coroutine_handle<promise_type> h)
        : _frame(h) {
        }

        frame_ptr<promise_type> _frame;
    };

    template<>
    class task<void> {
    public:
        struct promise_type : detail::TaskPromiseBase {
            task get_return_object() {
                return task(std::coroutine_handle<promise_type>::from_promise(*this));
            }

            std::suspend_always initial_suspend() noexcept {
                return {};
            }

            void return_void() noexcept {
            }
        };

        class awaiter {
        public:
            bool await_ready() const noexcept {
                return _task.done();
            }

            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) noexcept {
                auto frame = detail::frame_of(h);
                _task._start(detail::frame_header(frame.get()).executor);   // a task nobody started yet: on the scheduler now, where this one runs
                return _task._frame.promise().await(h, std::move(frame));
            }

            void await_resume() {
                _task.result();
            }

        private:
            friend class task;

            explicit awaiter(task& t) noexcept
            : _task(t) {
            }

            task& _task;
        };

        template<class> friend struct detail::TimeoutRace;   // timeout.h: starts the task and installs its continuation

        task() noexcept = default;

        [[nodiscard]] task& spawn() {
            assert(_frame && !_frame.done() && "a task is spawned once, before it runs");
            [[maybe_unused]] bool first = _start();
            assert(first && "a task is spawned once");
            return *this;
        }

        void resume() {
            auto& p = _frame.promise();
            if (!p.started.load(std::memory_order_relaxed)) {   // its first run, by hand: the frame takes the resumer's header (task<T>::resume)
                if (auto parent = detail::current_frame) {
                    detail::frame_header(p.self.get()) = detail::frame_header(parent);
                }
            }
            p.started.store(true, std::memory_order_release);
            auto running = std::exchange(detail::current_frame, p.self.get());
            _frame.resume();
            detail::current_frame = running;
        }

        bool done() const noexcept {
            return !_frame || _frame.promise().done();   // an empty task is done, as the page says: nothing is left to run
        }

        // Waits for the task, on this thread, and rethrows what it threw.
        // Not from a task on a worker (co_await it there)
        void wait() {
            _wait();
            result();
        }

        // Rethrows what the coroutine threw, if anything: waited for
        // first, on this thread, when the task is not done yet
        void result() {
            if (!done()) {
                _wait();
            }
            if (auto& p = _frame.promise(); p.error) {
                std::rethrow_exception(p.error);
            }
        }

        awaiter operator co_await() noexcept {
            return awaiter(*this);
        }

        void detach() noexcept {
            if (_frame.promise().released.exchange(true, std::memory_order_acq_rel)) {
                _frame.destroy();   // done already: destroyed now
            } else {
                (void)_frame.release();   // running or waiting: destroys itself when done
            }
        }

        void destroy() noexcept {
            _frame.destroy();
        }

    private:
        friend class executor;   // executor.h: its spawn and run_until start the task and await its promise
        friend class strand;

        // The task put on the scheduler unless it was started already: the
        // first of spawn(), join() and co_await starts it; true when this
        // one did. Where it runs: the pool of workers, or the executor
        // given (executor.h: the awaiting task's, so that a task awaited
        // runs where its awaiter does, as a call would; an executor's
        // spawn). Its task-locals are the starting task's (the frame this
        // thread runs, if any): inherited by the copy of the chain's head
        bool _start(tracked_ptr<detail::ExecutorQueue> executor = nullptr) {
            auto& p = _frame.promise();
            if (p.started.exchange(true, std::memory_order_acq_rel)) {
                return false;
            }
            auto& header = detail::frame_header(p.self.get());
            if (executor || header.executor) {   // a null over a null spared its barrier
                header.executor = executor;
            }
            if (auto parent = detail::current_frame) {
                header.locals = detail::frame_header(parent).locals;
            }
            detail::enqueue(p.self, false);
            return true;
        }

        // The promise, for an awaiter of the task's end that leaves the
        // result where it is (executor.h: run_until)
        detail::TaskPromiseBase& _promise() const noexcept {
            return _frame.promise();
        }

        // The wait of wait() and result(): the task started if nobody
        // started it, this thread blocked until its end
        void _wait() {
            assert(!detail::on_worker() && "wait() blocks the worker: co_await the task from a task");
            _start();   // a task nobody started yet: on the scheduler now
            _frame.promise().wait();
        }

        explicit task(std::coroutine_handle<promise_type> h)
        : _frame(h) {
        }

        frame_ptr<promise_type> _frame;
    };

    // The task put on the scheduler, for `auto t = spawn(f());`; nodiscard
    // as the member (the task object dropped destroys the coroutine)
    template<class T>
    [[nodiscard]] task<T> spawn(task<T> t) {
        (void)t.spawn();
        return t;
    }

    // An operation run as a task of its own, concurrently: `spawn(ch.receive())`
    // is a task whose result is what `co_await ch.receive()` gives
    template<class F>
    [[nodiscard]] auto spawn(operation<F> op) {
        using R = decltype(std::declval<operation<F>&>().await_resume());
        return spawn([](operation<F> o) -> task<std::remove_cvref_t<R>> {
            if constexpr (std::is_void_v<R>) {
                co_await o;
            } else {
                co_return co_await o;
            }
        }(std::move(op)));
    }

    // The task put on the scheduler and let go of: it runs, nobody waits
    // for it, its frame is the collector's once it is done. Go's `go f()`
    template<class T>
    void go(task<T> t) {
        t.spawn().detach();
    }

    namespace detail {
        // A callable that makes a task (or another coroutine type over a
        // managed frame): what spawn and go take besides a task
        template<class F>
        concept TaskFactory = std::invocable<F&> && requires {
            typename std::remove_cvref_t<std::invoke_result_t<F&>>::promise_type;
        };

        // The task of such a callable, the callable copied into a frame
        // that lives as long as the task. A lambda's captures are fields
        // of the closure and a coroutine's frame holds the closure by
        // `this`, not by copy (only parameters are copied into a frame),
        // so `go([x]() -> task<> { ... }())` runs on a closure that died
        // at the semicolon; here the closure is the parameter f, copied
        // into this frame, and the inner coroutine's `this` points into it
        template<class F>
        std::remove_cvref_t<std::invoke_result_t<F&>> task_of(F f) {
            co_return co_await f();
        }
    }

    // The same, given the coroutine function rather than its task:
    // `spawn([x]() -> task<int> { ... })`, `go([&ch]() -> task<> { ... })`,
    // no call. The closure is copied into a frame of the task's own, so
    // its captures live as long as the task — where the task of a
    // temporary closure, `spawn([x]() -> task<int> { ... }())`, refers
    // to a closure that dies at the end of the statement (CppCoreGuidelines
    // CP.51: a lambda with captures must not be a coroutine). A lambda
    // without captures, or a named coroutine with parameters, may be
    // called and its task passed; one with captures is passed itself.
    template<detail::TaskFactory F>
    [[nodiscard]] auto spawn(F f) {
        return spawn(detail::task_of(std::move(f)));
    }

    template<detail::TaskFactory F>
    void go(F f) {
        go(detail::task_of(std::move(f)));
    }
}
