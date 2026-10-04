//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "operation.h"
#include "../core/coroutine.h"
#include "../core/detail/frame_word.h"
#include "../core/detail/os.h"
#include "../core/tracked_ptr.h"

#include <atomic>
#include <coroutine>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <optional>
#include <thread>
#include <typeinfo>
#include <utility>
#if __has_include(<cxxabi.h>) && !defined(_MSC_VER)
#include <cxxabi.h>
#endif
#if defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace sgcl::async::detail {
    using namespace sgcl::detail;
    void enqueue(tracked_ptr<FrameWord> frame, bool next);   // scheduler.h: the coroutine made ready, to run next on this worker or later
    void enqueue_quiet(tracked_ptr<FrameWord> frame, bool next) noexcept;   // scheduler.h: the same for a noexcept call; a start of the workers that fails is let go of, the frame queued for the next start
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

        SGCL_INLINE_HOT FrameWord* load(std::memory_order m) const noexcept {
            return (FrameWord*)_ptr()->load(m);
        }

        SGCL_INLINE_HOT void store(FrameWord* frame, std::memory_order m) noexcept {
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

        SGCL_INLINE_HOT FrameHeader& operator=(const FrameHeader& h) noexcept {
            executor = h.executor;
            locals = h.locals;
            return *this;
        }
    };

    static_assert(sizeof(FrameHeader) == FrameHeaderWords * sizeof(FrameWord));   // exactly the words the core keeps in front of the frame
    static_assert(sizeof(FrameHeader) % 16 == 0);

    // The header of a frame named by its buffer's address (the promise's
    // `self`, every queue's and waiter's word; core/coroutine.h: handle_of)
    SGCL_INLINE_HOT FrameHeader& frame_header(void* frame) noexcept {
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
        // Where the calling thread belongs, for the default's line: the
        // index of a scheduler's worker (set by the worker, scheduler.h:
        // _run), the blocking pool (blocking.h: _run), or neither
        inline constexpr int PlaceThread = -1;
        inline constexpr int PlaceBlockingPool = -2;
        inline thread_local int thread_place = PlaceThread;

        // The system's number of the calling thread, the one a debugger
        // and the system's tools show
        inline unsigned long long os_thread_id() noexcept {
#if defined(_WIN32)
            return (unsigned long long)::GetCurrentThreadId();
#elif defined(__APPLE__)
            uint64_t id = 0;
            ::pthread_threadid_np(nullptr, &id);
            return id;
#elif defined(__linux__)
            return (unsigned long long)::syscall(SYS_gettid);
#else
            return (unsigned long long)std::hash<std::thread::id>()(std::this_thread::get_id());
#endif
        }

        // The name of a type as the source spells it, into out: demangled
        // where the ABI has a demangler (clang, gcc), name() as it is on
        // MSVC; the standard library's inline namespaces (libc++'s __1,
        // libstdc++'s __cxx11) dropped, so it reads std::system_error
        inline void type_name(const std::type_info& type, char* out, size_t size) noexcept {
            const char* name = type.name();
            char* demangled = nullptr;
#if __has_include(<cxxabi.h>) && !defined(_MSC_VER)
            int status = 0;
            demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
            if (demangled && status == 0) {
                name = demangled;
            }
#endif
            size_t n = 0;
            for (const char* p = name; *p && n + 1 < size;) {
                if (std::strncmp(p, "__1::", 5) == 0) {
                    p += 5;
                } else if (std::strncmp(p, "__cxx11::", 9) == 0) {
                    p += 9;
                } else {
                    out[n++] = *p++;
                }
            }
            out[n] = 0;
            std::free(demangled);
        }

        // The default of on_unhandled, Go's panic in a goroutine: one line
        // on stderr — the exception's type, what(), and where the task
        // ended (a worker's index, the blocking pool, or the thread's
        // number) — then std::terminate. Where the task was started is not
        // kept by its frame, so the line cannot say it
        [[noreturn]] inline void unhandled_default(std::exception_ptr e) noexcept {
            char place[48];
            if (thread_place >= 0) {
                std::snprintf(place, sizeof(place), "on worker %d", thread_place);
            } else if (thread_place == PlaceBlockingPool) {
                std::snprintf(place, sizeof(place), "in the blocking pool");
            } else {
                std::snprintf(place, sizeof(place), "on thread %llu", os_thread_id());
            }
            try {
                std::rethrow_exception(e);
            } catch (const std::exception& x) {
                char type[256];
                type_name(typeid(x), type, sizeof(type));
                std::fprintf(stderr, "sgcl::async: unhandled exception in a detached task %s: %s: %s\n", place, type, x.what());
            } catch (...) {
                std::fprintf(stderr, "sgcl::async: unhandled exception in a detached task %s: unknown exception\n", place);
            }
            std::fflush(stderr);
            std::terminate();
        }

        inline std::atomic<void (*)(std::exception_ptr)> unhandled_handler = {&unhandled_default};
    }

    // What becomes of an exception that a task let go of threw and nobody
    // reads: a task started by go(), detached, or whose object was dropped
    // after it started (a task let go of, DESIGN 302), and the function of
    // go_blocking (blocking.h). The handler is called with it once, on the
    // thread that ends the task (a worker), that detaches a task already
    // ended, or that destroys the object holding one (the collector's, for
    // a task in a managed object), or on the pool's thread that ran f; it
    // must not throw. The default writes one line to stderr (the type,
    // what(), where the task ended) and calls std::terminate, as an
    // unhandled panic of a goroutine ends a Go program; a handler of the
    // program's replaces it (a log, and on), nullptr puts the default
    // back. The previous handler is returned. An exception that result(),
    // wait() or co_await gave to someone is theirs, never the handler's;
    // one of a task that lost a race (when_any, with_timeout,
    // with_deadline) is dropped with its value
    inline void (*on_unhandled(void (*handler)(std::exception_ptr)) noexcept)(std::exception_ptr) {
        return detail::unhandled_handler.exchange(handler ? handler : &detail::unhandled_default, std::memory_order_acq_rel);
    }

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
            bool error_taken = false;   // given to someone by result() (wait, co_await): never the unhandled handler's

            // A task let go of ends, or one ended is let go of: its
            // exception, if nobody took it, to on_unhandled's handler
            SGCL_INLINE_HOT void report_unhandled() noexcept {
                if (error && !error_taken) {
                    unhandled_handler.load(std::memory_order_acquire)(error);
                }
            }

            // The awaiter of the final suspension: done, and everyone told
            struct final_awaiter {
                SGCL_INLINE_HOT bool await_ready() noexcept {
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
                        enqueue_quiet(p.continuation_frame, true);   // the end of a task is noexcept and has nobody to tell: an awaiter that cannot start the workers (a task ended by hand, the scheduler stopped) is queued for the next start
                        p.continuation_frame = nullptr;   // not kept: a done task would hold its awaiter's frame (and, through the dead frame's words, what they point at) for as long as the task object lives
                    }
                    // A detached task destroys itself here: its locals and
                    // parameters (a task it awaited, a root_ptr) released, the
                    // memory the collector's. Nothing of the frame is touched
                    // past the destroy.
                    if (p.released.exchange(true, std::memory_order_acq_rel)) {
                        p.report_unhandled();   // a task let go of: nobody will read what it threw
                        h.destroy();
                    }
                }

                SGCL_INLINE_HOT void await_resume() noexcept {
                }
            };

            SGCL_INLINE_HOT final_awaiter final_suspend() noexcept {
                return {};
            }

            SGCL_INLINE_HOT void unhandled_exception() noexcept {
                error = std::current_exception();
            }

            SGCL_INLINE_HOT bool done() const noexcept {
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
    // it is done. A task object that goes lets go of its task the same
    // way (DESIGN 302): a started task runs on to its end, and only one
    // that never started is destroyed with it. spawn() is nodiscard, since
    // a handle dropped at once is a result nobody reads: go() is the spawn
    // whose handle nobody keeps. The frame
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

            SGCL_INLINE_HOT task get_return_object() noexcept {
                return task(std::coroutine_handle<promise_type>::from_promise(*this));
            }

            SGCL_INLINE_HOT std::suspend_always initial_suspend() noexcept {
                return {};
            }

            SGCL_INLINE_HOT void return_value(T v) noexcept(std::is_nothrow_move_constructible_v<T>) {
                value.emplace(std::move(v));
            }
        };

        // The awaiter of `co_await task`: suspends the awaiting coroutine
        // until the task is done (not at all when it is), then its result
        class awaiter {
        public:
            SGCL_INLINE_HOT bool await_ready() const noexcept {
                return _task.done();
            }

            // Not noexcept: the start of a task nobody started may have to
            // start the workers (the awaiter run by hand, the scheduler
            // stopped), and a std::system_error then comes out of the
            // co_await, in the awaiting task; the task started is queued
            // already, and runs, let go of, at the next start
            template<class P>
            SGCL_INLINE_HOT bool await_suspend(std::coroutine_handle<P> h) {
                auto frame = detail::frame_of(h);
                _task._start(detail::frame_header(frame.get()).executor, true);   // a task nobody started yet: on the scheduler now, where this one runs, next (this one suspends)
                return _task._frame.promise().await(h, std::move(frame));
            }

            SGCL_INLINE_HOT T await_resume() {
                return std::move(_task.result());
            }

        private:
            friend class task;

            SGCL_INLINE_HOT explicit awaiter(task& t) noexcept
            : _task(t) {
            }

            task& _task;
        };

        template<class> friend struct detail::TimeoutRace;   // timeout.h: starts the task and installs its continuation

        task() noexcept = default;
        task(task&&) noexcept = default;

        // A task let go of (its object dropped or assigned over; DESIGN
        // 302): one that started runs on to its end, as detach() lets it,
        // and nothing it waits for resumes a destroyed coroutine; one that
        // never started is destroyed with its frame. Cancellation is the
        // task's own, through its stop_token
        SGCL_INLINE_HOT task& operator=(task&& o) noexcept {
            if (this != &o) {
                _let_go();
                _frame = std::move(o._frame);
            }
            return *this;
        }

        SGCL_INLINE_HOT ~task() {
            _let_go();
        }

        // On the scheduler: queued, run by a worker to its next suspension.
        // Nodiscard: a handle dropped is a result nobody reads (the task
        // runs on, let go of: DESIGN 302); keep it, or go() the task
        // instead
        [[nodiscard]] SGCL_INLINE_HOT task& spawn() {
            assert(_frame && !_frame.done() && "a task is spawned once, before it runs");
            [[maybe_unused]] bool first = _start();
            assert(first && "a task is spawned once");
            return *this;
        }

        // By hand, on this thread: to its next suspension. Nothing the
        // body throws leaves it: the promise keeps it for result()
        void resume() noexcept {
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

        SGCL_INLINE_HOT bool done() const noexcept {
            return !_frame || _frame.promise().done();   // an empty task is done, as the page says: nothing is left to run
        }

        // Waits for the task, on this thread, and gives its result, or
        // rethrows what it threw (as std::this_thread::sync_wait gives a
        // sender's). Not from a task on a worker (co_await it there)
        SGCL_INLINE_HOT T& wait() {
            _wait();
            return result();
        }

        // The result of the task, or what it threw: waited for first, on
        // this thread, when the task is not done yet
        SGCL_INLINE_HOT T& result() {
            if (!done()) {
                _wait();
            }
            auto& p = _frame.promise();
            if (p.error) {
                p.error_taken = true;
                std::rethrow_exception(p.error);
            }
            return *p.value;
        }

        SGCL_INLINE_HOT awaiter operator co_await() noexcept {
            return awaiter(*this);
        }

        // Lets go of the task: it runs on (or stays wherever it waits) and
        // destroys its frame when it is done (its locals and parameters
        // with it), the memory the collector's from then on; a task that
        // never runs leaves its frame to the collector as it is
        SGCL_INLINE_HOT void detach() noexcept {
            if (_frame.promise().released.exchange(true, std::memory_order_acq_rel)) {
                _frame.promise().report_unhandled();   // done already, what it threw unread
                _frame.destroy();   // done already: destroyed now
            } else {
                (void)_frame.release();   // running or waiting: destroys itself when done
            }
        }

        // Destroys the coroutine now, its locals and promise with it: for
        // a task that never ran or is done
        SGCL_INLINE_HOT void destroy() noexcept {
            _frame.destroy();
        }

    private:
        // What the object's end and a move-assignment over it do (above)
        SGCL_INLINE_HOT void _let_go() noexcept {
            if (_frame && _frame.promise().started.load(std::memory_order_acquire)) {
                detach();
            }
        }

        // The wait of wait() and result(): the task started if nobody
        // started it, this thread blocked until its end
        SGCL_INLINE_HOT void _wait() {
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
        // thread runs, if any): inherited by the copy of the chain's head.
        // `next`: a start by an awaiter that suspends right after it (the
        // co_await, a timeout's race), so the task runs next on this
        // worker, as a call would; from the end of the ring a looking
        // worker could take it in the moment before the suspension, and
        // the awaiter, resumed where the task ends, went with it: its
        // whole chain moved to the thief and back (DESIGN 453)
        bool _start(tracked_ptr<detail::ExecutorQueue> executor = nullptr, bool next = false) {
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
            detail::enqueue(p.self, next);
            return true;
        }

        // The promise, for an awaiter of the task's end that leaves the
        // result where it is (executor.h: run_until)
        SGCL_INLINE_HOT detail::TaskPromiseBase& _promise() const noexcept {
            return _frame.promise();
        }

        SGCL_INLINE_HOT explicit task(std::coroutine_handle<promise_type> h) noexcept
        : _frame(h) {
        }

        frame_ptr<promise_type> _frame;
    };

    template<>
    class task<void> {
    public:
        struct promise_type : detail::TaskPromiseBase {
            SGCL_INLINE_HOT task get_return_object() noexcept {
                return task(std::coroutine_handle<promise_type>::from_promise(*this));
            }

            SGCL_INLINE_HOT std::suspend_always initial_suspend() noexcept {
                return {};
            }

            SGCL_INLINE_HOT void return_void() noexcept {
            }
        };

        class awaiter {
        public:
            SGCL_INLINE_HOT bool await_ready() const noexcept {
                return _task.done();
            }

            // Not noexcept: the start of a task nobody started may have to
            // start the workers (the awaiter run by hand, the scheduler
            // stopped), and a std::system_error then comes out of the
            // co_await, in the awaiting task; the task started is queued
            // already, and runs, let go of, at the next start
            template<class P>
            SGCL_INLINE_HOT bool await_suspend(std::coroutine_handle<P> h) {
                auto frame = detail::frame_of(h);
                _task._start(detail::frame_header(frame.get()).executor, true);   // a task nobody started yet: on the scheduler now, where this one runs, next (this one suspends)
                return _task._frame.promise().await(h, std::move(frame));
            }

            SGCL_INLINE_HOT void await_resume() {
                _task.result();
            }

        private:
            friend class task;

            SGCL_INLINE_HOT explicit awaiter(task& t) noexcept
            : _task(t) {
            }

            task& _task;
        };

        template<class> friend struct detail::TimeoutRace;   // timeout.h: starts the task and installs its continuation

        task() noexcept = default;
        task(task&&) noexcept = default;

        // A task let go of (its object dropped or assigned over; DESIGN
        // 302): one that started runs on to its end, as detach() lets it,
        // and nothing it waits for resumes a destroyed coroutine; one that
        // never started is destroyed with its frame. Cancellation is the
        // task's own, through its stop_token
        SGCL_INLINE_HOT task& operator=(task&& o) noexcept {
            if (this != &o) {
                _let_go();
                _frame = std::move(o._frame);
            }
            return *this;
        }

        SGCL_INLINE_HOT ~task() {
            _let_go();
        }

        [[nodiscard]] SGCL_INLINE_HOT task& spawn() {
            assert(_frame && !_frame.done() && "a task is spawned once, before it runs");
            [[maybe_unused]] bool first = _start();
            assert(first && "a task is spawned once");
            return *this;
        }

        void resume() noexcept {
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

        SGCL_INLINE_HOT bool done() const noexcept {
            return !_frame || _frame.promise().done();   // an empty task is done, as the page says: nothing is left to run
        }

        // Waits for the task, on this thread, and rethrows what it threw.
        // Not from a task on a worker (co_await it there)
        SGCL_INLINE_HOT void wait() {
            _wait();
            result();
        }

        // Rethrows what the coroutine threw, if anything: waited for
        // first, on this thread, when the task is not done yet
        SGCL_INLINE_HOT void result() {
            if (!done()) {
                _wait();
            }
            if (auto& p = _frame.promise(); p.error) {
                p.error_taken = true;
                std::rethrow_exception(p.error);
            }
        }

        SGCL_INLINE_HOT awaiter operator co_await() noexcept {
            return awaiter(*this);
        }

        SGCL_INLINE_HOT void detach() noexcept {
            if (_frame.promise().released.exchange(true, std::memory_order_acq_rel)) {
                _frame.promise().report_unhandled();   // done already, what it threw unread
                _frame.destroy();   // done already: destroyed now
            } else {
                (void)_frame.release();   // running or waiting: destroys itself when done
            }
        }

        SGCL_INLINE_HOT void destroy() noexcept {
            _frame.destroy();
        }

    private:
        // What the object's end and a move-assignment over it do (task<T>)
        SGCL_INLINE_HOT void _let_go() noexcept {
            if (_frame && _frame.promise().started.load(std::memory_order_acquire)) {
                detach();
            }
        }

        friend class executor;   // executor.h: its spawn and run_until start the task and await its promise
        friend class strand;

        // The task put on the scheduler unless it was started already: the
        // first of spawn(), join() and co_await starts it; true when this
        // one did. Where it runs: the pool of workers, or the executor
        // given (executor.h: the awaiting task's, so that a task awaited
        // runs where its awaiter does, as a call would; an executor's
        // spawn). Its task-locals are the starting task's (the frame this
        // thread runs, if any): inherited by the copy of the chain's head.
        // `next`: a start by an awaiter that suspends right after it (the
        // co_await, a timeout's race), so the task runs next on this
        // worker, as a call would; from the end of the ring a looking
        // worker could take it in the moment before the suspension, and
        // the awaiter, resumed where the task ends, went with it: its
        // whole chain moved to the thief and back (DESIGN 453)
        bool _start(tracked_ptr<detail::ExecutorQueue> executor = nullptr, bool next = false) {
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
            detail::enqueue(p.self, next);
            return true;
        }

        // The promise, for an awaiter of the task's end that leaves the
        // result where it is (executor.h: run_until)
        SGCL_INLINE_HOT detail::TaskPromiseBase& _promise() const noexcept {
            return _frame.promise();
        }

        // The wait of wait() and result(): the task started if nobody
        // started it, this thread blocked until its end
        SGCL_INLINE_HOT void _wait() {
            assert(!detail::on_worker() && "wait() blocks the worker: co_await the task from a task");
            _start();   // a task nobody started yet: on the scheduler now
            _frame.promise().wait();
        }

        SGCL_INLINE_HOT explicit task(std::coroutine_handle<promise_type> h) noexcept
        : _frame(h) {
        }

        frame_ptr<promise_type> _frame;
    };

    // The task put on the scheduler, for `auto t = spawn(f());`; nodiscard
    // as the member (a task object dropped lets the task run on unread)
    template<class T>
    [[nodiscard]] SGCL_INLINE_HOT task<T> spawn(task<T> t) {
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
    SGCL_INLINE_HOT void go(task<T> t) {
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
    [[nodiscard]] SGCL_INLINE_HOT auto spawn(F f) {
        return spawn(detail::task_of(std::move(f)));
    }

    template<detail::TaskFactory F>
    SGCL_INLINE_HOT void go(F f) {
        go(detail::task_of(std::move(f)));
    }
}
