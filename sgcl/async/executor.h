//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/make_tracked.h"
#include "../core/root_ptr.h"
#include "coroutine.h"
#include "scheduler.h"

#include <cassert>
#include <coroutine>
#include <cstddef>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    class strand;

    // A task on a thread of the program's choosing. The scheduler runs a
    // task on whichever worker is free; a platform's UI toolkit (Cocoa,
    // Win32, GTK, the browser) and many a C library demand one thread,
    // usually the main one, for every call into them. An executor is a
    // queue of frames that only the thread running the executor resumes:
    // `ex.run()` is that thread's loop, run(task) the loop until the task
    // is done (`int main() { sgcl::async::executor main; return main.run(program()); }`:
    // the program one task on the main thread), poll() one pass over
    // what is queued, for a foreign loop (a CFRunLoop, a GMainLoop, a
    // game's frame) that calls it when it has a moment. A task moves to
    // an executor with `co_await sgcl::async::on(ex)` (Kotlin's
    // withContext(Dispatchers.Main)) and back to the pool with `co_await
    // sgcl::async::on_workers()`; ex.spawn(t) starts a task on it. A task on an
    // executor stays on it: its frame remembers the executor (coroutine.h:
    // FrameHeader), and every wake of the frame, whatever woke it (a
    // channel, a timer, a task it awaited, a yield, a select), goes
    // through the scheduler's enqueue, which routes it to the executor's
    // queue (scheduler.h: ExecutorQueue). A task the task awaits runs
    // where the task does, as a call would; a task it spawns with
    // sgcl::async::spawn or go runs on the pool, a task it spawns with ex.spawn
    // on ex.
    //
    // The thread running the executor parks when the queue is empty, on
    // the queue's own word, and a push wakes it (after the spin every
    // worker does before it sleeps); stop() makes run() return after the
    // frame it is resuming, from any thread. What stop() stops is the
    // loop, not the tasks: they stay on the queue, suspended, and the next
    // run() or poll() resumes them; a stop() with no run in progress makes
    // the next run() return at once. An executor destroyed leaves its
    // tasks the same way, suspended for good: the queue is a managed
    // object every frame on it holds through its header, so nothing
    // dangles, a wake of such a task lands on the queue nobody runs, and
    // the frames are the collector's when nothing else holds them, their
    // destructors never run, as with a task the scheduler was stopped
    // under. A program that wants its tasks finished lets them finish, or
    // runs the executor until they are.
    //
    // The queue is held through a root: the executor lives anywhere (a
    // local of main, a global, a member).
    class executor {
    public:
        executor()
        : _q(make_tracked<detail::ExecutorQueue>(false)) {
        }

        executor(const executor&) = delete;
        executor& operator=(const executor&) = delete;

        // The calling thread's loop: what is queued, run; nothing queued,
        // parked; until stop()
        void run() {
            _loop([] { return false; });
        }

        // The loop until the task is done (or stop()): the task started
        // on this executor when nobody started it yet, waited for through
        // a task of this executor that awaits its end (one awaiter per
        // task: nobody else co_awaits it meanwhile; the result stays for
        // t.result()). The loop runs until the watcher is done, which is
        // right after t: the watcher is resumed here, whatever thread t
        // ended on, so the end of t is a wake of this loop
        template<class T>
        void run_until(task<T>& t) {
            if (t.done()) {
                return;
            }
            if (_watched != &t || _watcher.done()) {   // a watcher left by a stop is taken up again for the same task: a second awaiter of t would take the first's place and leave it with no frame to resume (coroutine.h: await)
                _watcher = spawn(_watch(t));
                _watched = &t;
            }
            _loop([&] { return _watcher.done(); });
            if (_watcher.done()) {
                _watcher = task<>();
                _watched = nullptr;
            }
        }

        // The loop until the task is done, and its result (or what it
        // threw): the main function's idiom, `return main.run(program());`
        template<class T>
        T run(task<T> t) {
            run_until(t);
            assert(t.done() && "the executor was stopped before the task ended");
            return std::move(t.result());
        }

        void run(task<> t) {
            run_until(t);
            assert(t.done() && "the executor was stopped before the task ended");
            t.result();
        }

        // One pass, on the calling thread: the frames queued at the call
        // run (each to its next suspension; one queued by them meanwhile
        // waits for the next pass), and their number returned; nothing
        // queued, nothing done. For a loop of the program's own
        size_t poll() {
            [[maybe_unused]] bool was = _q->running.exchange(true, std::memory_order_acq_rel);
            assert(!was && "an executor is run by one thread at a time");
            size_t n = 0;
            auto left = _q->pushes.load(std::memory_order_acquire) - _q->taken;
            if (int32_t(left) < 0) {   // a push linked and popped by a run() before its count landed: nothing owed
                left = 0;
            }
            for (; left > 0; --left) {
                auto f = _q->take();
                if (!f) {
                    break;   // the rest is a push under way: the next pass
                }
                detail::resume_frame(std::move(f));
                ++n;
            }
            _q->running.store(false, std::memory_order_release);
            return n;
        }

        // run() returns, after the frame it is resuming; from any thread
        // (a task on the executor, a signal handler's thread, the program's
        // last line). The tasks stay queued for the next run() or poll()
        void stop() {
            _q->stop();
        }

        // Whether a run() or a poll() is in progress
        bool running() const noexcept {
            return _q->running.load(std::memory_order_acquire);
        }

        // A task started on this executor: queued here, run by the thread
        // that runs the executor; nodiscard as sgcl::async::spawn (a task object
        // dropped destroys the coroutine; go() for a task nobody waits for)
        template<class T>
        [[nodiscard]] task<T> spawn(task<T> t) {
            [[maybe_unused]] bool first = t._start(_q.ptr());
            assert(first && "a task is spawned once");
            return t;
        }

        template<class T>
        void go(task<T> t) {
            spawn(std::move(t)).detach();
        }

        // The same for a coroutine function with captures (sgcl::async::spawn)
        template<detail::TaskFactory F>
        [[nodiscard]] auto spawn(F f) {
            return spawn(detail::task_of(std::move(f)));
        }

        template<detail::TaskFactory F>
        void go(F f) {
            spawn(detail::task_of(std::move(f))).detach();
        }

    private:
        friend class on;

        template<class Done>
        void _loop(Done done) {
            [[maybe_unused]] bool was = _q->running.exchange(true, std::memory_order_acq_rel);
            assert(!was && "an executor is run by one thread at a time");
            for (;;) {
                if (_q->stopping.load(std::memory_order_acquire) && _q->stopping.exchange(false, std::memory_order_acq_rel)) {
                    break;
                }
                if (done()) {
                    break;
                }
                if (auto f = _q->pop()) {
                    detail::resume_frame(std::move(f));
                }
            }
            _q->running.store(false, std::memory_order_release);
        }

        // The wait of the watcher: the task's end, its result and its
        // exception left where they are (`co_await t` would move the
        // result out)
        template<class T>
        struct done_awaiter {
            task<T>& t;

            bool await_ready() const noexcept {
                return t.done();
            }

            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) noexcept {
                auto frame = detail::frame_of(h);
                t._start(detail::frame_header(frame.get()).executor);   // not started yet: here
                return t._promise().await(h, std::move(frame));
            }

            void await_resume() const noexcept {
            }
        };

        // The task of run_until: a task on this executor that awaits t's
        // end. It touches t only until it suspends: a watcher left behind
        // by a stop wakes later with a reference that may be gone
        template<class T>
        static task<> _watch(task<T>& t) {
            co_await done_awaiter<T>{t};
        }

        root_ptr<detail::ExecutorQueue> _q;
        task<> _watcher;             // the watcher of run_until, kept across a stop for the same task
        const void* _watched = nullptr;
    };

    // An executor with no thread of its own: its tasks run on the workers,
    // never two at once, in the order they were queued (Boost.Asio's
    // strand): serial access to something without a lock, the handler of
    // a connection, the owner of a document. A task on a strand that waits
    // leaves the strand to the next task, and comes back through the
    // strand's queue when it is woken, so the strand is held between two
    // suspensions of a task and never across one: a task that reads a
    // structure, awaits, and writes it does not find it as it left it
    // (a mutex does that, mutex.h). `co_await sgcl::async::on(s)` moves a task to
    // the strand, s.spawn(t) starts one there; the frame remembers the
    // strand as it would an executor. The head of the queue is handed to
    // the workers when the strand goes from idle to busy, the next one
    // when the running task suspends or finishes (scheduler.h:
    // ExecutorQueue, resume_frame): a strand costs its queue and one
    // count, and nothing while nothing is queued.
    class strand {
    public:
        strand()
        : _q(make_tracked<detail::ExecutorQueue>(true)) {
        }

        strand(const strand&) = delete;
        strand& operator=(const strand&) = delete;

        // A task started on this strand: queued here, run by a worker in
        // its turn
        template<class T>
        [[nodiscard]] task<T> spawn(task<T> t) {
            [[maybe_unused]] bool first = t._start(_q.ptr());
            assert(first && "a task is spawned once");
            return t;
        }

        template<class T>
        void go(task<T> t) {
            spawn(std::move(t)).detach();
        }

        // The same for a coroutine function with captures (sgcl::async::spawn)
        template<detail::TaskFactory F>
        [[nodiscard]] auto spawn(F f) {
            return spawn(detail::task_of(std::move(f)));
        }

        template<detail::TaskFactory F>
        void go(F f) {
            spawn(detail::task_of(std::move(f))).detach();
        }

        // Whether a task of the strand runs or is queued at this moment
        bool busy() const noexcept {
            return _q->pending.load(std::memory_order_acquire) != 0;
        }

    private:
        friend class on;

        root_ptr<detail::ExecutorQueue> _q;
    };

    // `co_await sgcl::async::on(ex)`: the task goes on on the executor (or the
    // strand), from the next line; at once when it is there already. What
    // it awaits from then on wakes it there. From a thread that is no
    // worker and no executor (a task resumed by hand) as from a worker
    class [[nodiscard]] on {
    public:
        explicit on(executor& ex) noexcept
        : _q(ex._q.ptr()) {
        }

        explicit on(strand& s) noexcept
        : _q(s._q.ptr()) {
        }

        bool await_ready() const noexcept {
            return false;
        }

        // The frame's executor set, the frame queued there: from the push
        // on the executor's thread may run it, so nothing of the frame is
        // touched past it
        template<class P>
        bool await_suspend(std::coroutine_handle<P> h) {
            auto frame = detail::frame_of(h);
            auto& header = detail::frame_header(frame.get());
            if (header.executor.get() == _q.get()) {
                return false;   // there already
            }
            header.executor = _q;
            _q->push(std::move(frame), false);
            return true;
        }

        void await_resume() const noexcept {
        }

    private:
        tracked_ptr<detail::ExecutorQueue> _q;
    };

    // `co_await sgcl::async::on_workers()`: the task goes on on the pool of
    // workers, from the next line; at once when it is on a worker with no
    // executor. A task that left the main thread for a computation
    struct [[nodiscard]] on_workers {
        bool await_ready() const noexcept {
            return false;
        }

        template<class P>
        bool await_suspend(std::coroutine_handle<P> h) {
            auto frame = detail::frame_of(h);
            auto& header = detail::frame_header(frame.get());
            if (!header.executor && detail::on_worker()) {
                return false;
            }
            header.executor = nullptr;
            detail::enqueue(std::move(frame), false);
            return true;
        }

        void await_resume() const noexcept {
        }
    };

    // The task started on the executor or the strand, for
    // `auto t = spawn(f(), ex);` and `go(f(), ex);`
    template<class T, class Executor>
    [[nodiscard]] task<T> spawn(task<T> t, Executor& ex) {
        return ex.spawn(std::move(t));
    }

    template<class T, class Executor>
    void go(task<T> t, Executor& ex) {
        ex.go(std::move(t));
    }

    template<detail::TaskFactory F, class Executor>
    [[nodiscard]] auto spawn(F f, Executor& ex) {
        return ex.spawn(std::move(f));
    }

    template<detail::TaskFactory F, class Executor>
    void go(F f, Executor& ex) {
        ex.go(std::move(f));
    }
}
