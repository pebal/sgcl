//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../concurrent/queue.h"
#include "../core/detail/backoff.h"
#include "../core/detail/ticks.h"
#include "../core/collector.h"
#include "../core/config.h"
#include "../core/detail/env.h"
#include "../core/duration.h"
#include "../core/unique_ptr.h"
#include "coroutine.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstdio>
#include <chrono>
#include <coroutine>
#include <mutex>
#include <new>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

// SGCL_REACTOR_IN_WORKERS: the reactor's queue read by the workers
// themselves (kqueue: macOS, FreeBSD), as Go's netpoll is: a worker with
// nothing to run waits in the kernel's queue of the descriptors instead of
// on its own word, and runs the first task an event makes ready, with no
// thread between (detail::PollSeat). 0: the reactor's thread alone reads
// the queue and wakes a worker for each task. A definition of the build wins
#ifndef SGCL_REACTOR_IN_WORKERS
#if (defined(__APPLE__) || defined(__FreeBSD__)) && !defined(SGCL_REACTOR_FORCE_EPOLL)
#define SGCL_REACTOR_IN_WORKERS 1
#else
#define SGCL_REACTOR_IN_WORKERS 0
#endif
#endif

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    namespace detail {
        class Scheduler;
        class WakeBatch;
        inline Scheduler& scheduler_instance() noexcept;
        void resume_frame(const tracked_ptr<FrameWord>& frame);

#if SGCL_REACTOR_IN_WORKERS
        // The seat at the reactor's queue (reactor.h): who reads the
        // kernel's events now. One holder at a time, so that one thread
        // sleeps in the queue (the kernel wakes every thread asleep in a
        // kqueue for an event, and one of them gets it): a worker with
        // nothing to run (it polls the queue between its looks for work,
        // and waits in it instead of on its word when it sleeps, taking
        // the events and running the first task they make ready itself),
        // or the reactor's thread, when every worker is busy (it takes
        // the events and wakes workers for them, as it always did), or
        // nobody for a moment. The rule that keeps someone reading:
        // a seat that is free has a worker looking for work (which takes
        // it at its next look, or at its sleep), or a worker woken to look,
        // or the reactor's thread watching it, which takes it when no
        // worker has read the queue for a while (Scheduler::_keep_looking,
        // reactor.h: _watch). The reactor's thread gives the seat up after
        // an event when workers look, and keeps it when none does.
        // The reactor sets the calls once (its constructor) and `enabled`
        // while its queue is open; the scheduler only reads them
        struct PollSeat {
            static constexpr uint32_t Free = 0;
            static constexpr uint32_t Reactor = 0xFFFF;    // a worker holds it as its index + 1
            static constexpr int Capacity = 32;            // the events a worker takes from the kernel at once
            static constexpr size_t BufferBytes = 1024;    // their room on the worker's stack (the reactor checks the kernel's struct fits)

            std::atomic<uint32_t> holder = {Reactor};
            [[maybe_unused]] unsigned char _pad0[config::cache_line_size - sizeof(std::atomic<uint32_t>)] = {};
            std::atomic<uint32_t> inside = {0};            // workers in a call of `wait` (the reactor's stop waits for none)
            std::atomic<uint32_t> polls = {0};             // the calls of `wait` so far: the reactor's thread watches it move (its backstop, reactor.h: _watch)
            [[maybe_unused]] unsigned char _pad1[config::cache_line_size - 2 * sizeof(std::atomic<uint32_t>)] = {};
            std::atomic<bool> enabled = {false};           // the reactor's queue open to the workers
            // The kernel's events into `events` (at most `capacity`):
            // their count, 0 for none (a poll) or an interrupted wait, -1
            // when the queue is closed to the workers or has failed
            int (*wait)(void* events, int capacity, bool block) noexcept = nullptr;
            // The events taken: the first task made ready into `first`
            // (an executor's goes to its executor), the rest to the queues
            void (*dispatch)(void* events, int n, tracked_ptr<FrameWord>& first) = nullptr;
            void (*wake_poller)() noexcept = nullptr;      // the worker asleep in the queue woken
            void (*wake_backstop)() noexcept = nullptr;    // the reactor's thread told the seat is free with every worker busy (it watches, and takes it when no worker reads the queue for a while)
        };

        inline PollSeat poll_seat;
#endif

        // How long a worker (and an executor's thread) with nothing to run
        // looks for work before it sleeps, in microseconds: set by
        // scheduler::set_worker_spin, else SGCL_WORKER_SPIN_US read at the
        // scheduler's first start, else config::worker_spin_microseconds.
        // A plain word read on the way to sleep, never on a task's path
        inline std::atomic<unsigned> worker_spin_us = {config::worker_spin_microseconds};
        inline std::atomic<bool> worker_spin_set = {false};

        // The deadline of a spin, on the processor's counter (ticks.h): a
        // read of it costs a few cycles, where the system's clock is a call
        // (mach_continuous_time on macOS, about a hundred nanoseconds, which
        // the spins read at every round, some sixty rounds in 20 us). The
        // time spun is the same; the program's timeouts stay on sgcl::clock
        SGCL_INLINE_HOT uint64_t spin_until() noexcept {
            return cpu_ticks() + ticks_of_microseconds(worker_spin_us.load(std::memory_order_relaxed));
        }

        // The queue of an executor or a strand (executor.h): a managed
        // object the executor holds through a root and every frame on it
        // through its header (coroutine.h: FrameHeader), and what
        // Scheduler::enqueue routes such a frame to. An executor's queue is
        // run by the thread that runs the executor and by no other: run()
        // pops and resumes, and parks on the queue's own word when it is
        // empty, after a spin of config::worker_spin_microseconds, as a
        // worker spins before it sleeps; a push wakes it, and so does
        // stop(), a wake that carries no work. A strand's queue is run by
        // the workers, one frame at a time, in the order of the pushes:
        // `pending` counts the frames queued and the one running, the
        // push that takes it from zero hands the head of the queue to the
        // workers, and the worker whose run of a strand's frame ended
        // (resume_frame: the coroutine suspended, finished, or moved to
        // another executor) hands the next; between the two nothing of
        // the strand runs anywhere. A frame's run ends at its next
        // suspension, so a task that waits leaves the strand to the next
        // task and, woken, comes back through the queue, behind whoever
        // is queued by then.
        //
        // The queue is Vyukov's intrusive one for many producers and one
        // consumer, linked through the frames themselves (FrameHeader::
        // next), so a push allocates nothing: a producer nulls its frame's
        // link, exchanges the tail for it and links the frame it got
        // after the one it got; the consumer walks from the head. A queue
        // that goes empty keeps a frame of its own, the stub (a buffer of
        // a header and nothing else), so that the last frame can be taken
        // with a successor behind it: the consumer that reaches the last
        // frame pushes the stub after it and takes the frame, and steps
        // over the stub when it meets it at the head. Every frame queued
        // is held by the link before it (the first one by the head), each
        // a tracked word stored through the barrier; the tail is a raw
        // word, which holds nothing and need not, as whatever it names is
        // held otherwise: the stub by the queue, a frame linked by the
        // link before it, and a frame between the exchange and its link
        // by the producer's own pointer, which it holds through the push.
        // The frame a producer got from the exchange is safe to write to
        // for the same reason: it is still queued, since the consumer takes
        // a frame only when it has a successor or is the tail, and it has
        // neither until that producer links it. The consumer takes a frame
        // and nulls its link, so a frame off the queue holds nothing of it
        // and is ready to be pushed again, here or elsewhere.
        // One consumer at a time: an executor's is the thread of its run()
        // or poll(), one at a time (`running`), and a strand's is whoever
        // holds its turn, the push that took `pending` from zero or the
        // worker whose run ended with more queued, each after the last
        // (the hand-off of the frame to the workers orders one turn before
        // the next, and `pending` orders a turn after the one that took it
        // to zero). The price of the shape is one window where it is not
        // lock-free: a producer between its exchange and its link has cut
        // the list, and the frames linked behind it wait for its store;
        // the consumer waits for it (a strand's turn, which knows there is
        // a frame), or looks again later (an executor's thread, whose park
        // does not sleep on a list that is not empty). Two instructions,
        // unless the producer is preempted between them.
        struct ExecutorQueue {
            using Frame = tracked_ptr<FrameWord>;

            SGCL_INLINE_HOT explicit ExecutorQueue(bool strand) noexcept
            : _stub(_make_stub())
            , _head(_stub)
            , strand(strand)
            , _tail(_stub.get()) {
            }

            // A frame made ready on this executor
            SGCL_INLINE_HOT void push(Frame frame, bool next) {
                _push(frame.get());
                if (strand) {
                    if (pending.fetch_add(1, std::memory_order_acq_rel) == 0) {
                        _dispatch(next);   // idle until now: the head to the workers
                    }
                } else {
                    pushes.fetch_add(1, std::memory_order_release);
                    _wake();
                }
            }

            // An executor: stop() called; the thread parked on the queue
            // woken to see it
            SGCL_INLINE_HOT void stop() noexcept {
                stopping.store(true, std::memory_order_seq_cst);
                _wake();
            }

            // A strand: the run of one of its frames ended; the next one
            // to the workers, if any is queued
            SGCL_INLINE_HOT void run_ended() {
                if (pending.fetch_sub(1, std::memory_order_acq_rel) > 1) {
                    _dispatch(false);
                }
            }

            // An executor's thread: the next frame, spinning a while on
            // an empty queue and parking then; null when a stop() came, or
            // for a wake that found nothing (the caller asks again)
            Frame pop() noexcept {
                const uint64_t until = spin_until();
                detail::Backoff<32> backoff;
                do {
                    if (auto f = take()) {
                        return f;
                    }
                    if (stopping.load(std::memory_order_acquire)) {
                        return Frame();
                    }
                    backoff();
                } while (cpu_ticks() < until);
                for (;;) {
                    if (auto f = take()) {
                        return f;
                    }
                    if (stopping.load(std::memory_order_acquire)) {
                        return Frame();
                    }
                    _park();
                }
            }

            // The consumer's: the first frame, counted as taken, or null
            // when none is linked (the queue empty, or a push under way)
            Frame take() noexcept {
                auto stub = _stub.get();
                if (_head.get() == stub) {
                    if (!_link(stub).load(std::memory_order_acquire)) {
                        return Frame();
                    }
                    _head = _link(stub);    // the stub stepped over
                    _link(stub) = nullptr;  // and holding nothing while it is off the list
                }
                auto head = _head.get();
                if (!_link(head).load(std::memory_order_acquire)) {
                    if (_tail.load(std::memory_order_acquire) != head) {
                        return Frame();   // a push under way: the next link lands in a moment
                    }
                    _push(stub);          // the last frame: the stub behind it, so that it can go
                    if (!_link(head).load(std::memory_order_acquire)) {
                        return Frame();   // a push came in between, before the stub: its link lands in a moment
                    }
                }
                Frame f = _head;
                _head = _link(head);
                _link(head) = nullptr;
                ++taken;
                return f;
            }

        private:
            Frame _stub;                           // the queue's own frame, on the list whenever the list has no other
            Frame _head;                           // the consumer's: the first frame, or the stub

        public:
            uint32_t taken = 0;                    // the pops so far (the consumer's alone)
            std::atomic<bool> stopping = {false};  // an executor: stop() called, the run in progress or the next one returns
            std::atomic<bool> running = {false};   // an executor: a run() or a poll() in progress
            const bool strand;

        private:
            // The consumer's words above, the producers' below, a cache
            // line apart (concurrent::queue: padding, not alignas). Never
            // read: its bytes are the distance against false sharing, and
            // -Wextra's "private field is not used" is told so
            [[maybe_unused]] unsigned char _pad[config::cache_line_size - 2 * sizeof(Frame) - sizeof(uint32_t) - 3 * sizeof(bool)] = {};
            std::atomic<FrameWord*> _tail;         // the producers': the last frame, or the stub (raw: whatever it names is held otherwise)
            std::atomic<uint32_t> _parked = {0};   // an executor: its thread asleep in pop(), or about to be

        public:
            std::atomic<uint32_t> pushes = {0};    // an executor: the pushes so far; with `taken`, what a poll has to run
            std::atomic<size_t> pending = {0};     // a strand: the frames queued, plus the one running

        private:
            SGCL_INLINE_HOT static FrameLink& _link(FrameWord* frame) noexcept {
                return frame_header(frame).next;
            }

            // A frame of the queue's own: a header and nothing else
            SGCL_INLINE_HOT static Frame _make_stub() noexcept {
                auto words = Maker<FrameWord[]>::make_tracked_data(FrameHeaderWords).release();
                return unique_ptr<FrameWord>(UniquePtr<FrameWord>(words));
            }

            // The frame at the tail. The exchange is seq_cst, as is the
            // park's look at the tail after its store to `_parked`: one of
            // the two sees the other (_wake)
            SGCL_INLINE_HOT void _push(FrameWord* frame) noexcept {
                _link(frame) = nullptr;
                auto prev = _tail.exchange(frame, std::memory_order_seq_cst);
                _link(prev).store(frame, std::memory_order_release);
            }

            // Nothing queued, and no push under way: the stub alone, at
            // both ends
            SGCL_INLINE_HOT bool _empty() const noexcept {
                auto stub = _stub.get();
                return _head.get() == stub && _tail.load(std::memory_order_seq_cst) == stub;
            }

            // The executor's thread asleep until a push or a stop: counted
            // as parked before its last look at the queue and at the stop,
            // so that a push or a stop after that look sees it (_wake).
            // A queue that is not empty but has nothing to take has a push
            // under way: the thread yields and looks again
            void _park() noexcept {
                _parked.store(1, std::memory_order_seq_cst);
                if (!_empty()) {
                    _parked.store(0, std::memory_order_relaxed);
                    std::this_thread::yield();
                    return;
                }
                if (!stopping.load(std::memory_order_seq_cst)) {
                    _parked.wait(1, std::memory_order_seq_cst);
                }
                _parked.store(0, std::memory_order_relaxed);
            }

            // A push or a stop: the thread woken if it is parked. The load
            // spares a push the exchange when nobody sleeps, which is the
            // rule while the thread runs or spins
            SGCL_INLINE_HOT void _wake() noexcept {
                if (_parked.load(std::memory_order_seq_cst) && _parked.exchange(0, std::memory_order_acq_rel)) {
                    _parked.notify_one();
                }
            }

            void _dispatch(bool next);   // below Scheduler: the head of the queue to the pool's queues
        };

        // The scheduler of the tasks (coroutine.h: task): worker threads,
        // one per core, each with a queue of its own of the coroutines
        // ready to run, and one global queue behind them. A coroutine is
        // made ready by enqueue: a task spawned, a task resumed by a
        // channel (channel.h: Waiter::wake), a task whose awaited task is
        // done, a task that yields; whoever makes it ready returns at
        // once, and a worker runs it, on its own stack, to the coroutine's
        // next suspension. A coroutine that waits is nowhere: a frame on
        // the managed heap and a word on some list, no thread held, so a
        // worker serves as many tasks as are ready. A ready frame is held
        // by the queue's word (the frame of a detached task has no other
        // holder) and, while it runs, by the worker's stack; the handle
        // is computed from the frame's address (coroutine.h: handle_of),
        // so a queue's entry is one word. The queues live in managed objects
        // the scheduler owns, the scheduler itself being a static.
        //
        // The shape is Go's: a worker that makes a coroutine ready puts it
        // on its own queue, and the one it wakes (a receiver served, a
        // task awaited) into its `next` slot, to run as soon as the
        // current coroutine suspends, on the same core, with the cache
        // warm (the rendezvous of two tasks never leaves the worker);
        // a thread that is not a worker puts it on the global queue. A
        // worker with nothing of its own takes from the global queue,
        // then steals half of another worker's queue; one that finds
        // nothing looks for config::worker_spin_microseconds and sleeps.
        // A worker is woken by an enqueue only when none is looking
        // (spinning) and one sleeps, and a spinning worker that finds
        // work wakes the next sleeper, so that there is one looking
        // while work keeps coming and none burning a core when it does
        // not. The one that wakes a sleeper counts it as looking before
        // it is (the credit the woken worker takes over), so that the
        // enqueues in between wake no more; the sleepers are a set of
        // bits, one per worker, so that a wake is of a worker that is
        // asleep, on its own word, and never of nobody. The local queue is a ring the owner pushes to and pops
        // from with plain stores, and thieves pop from with a
        // compare-exchange on its head, a read of the entries validated
        // by that exchange (an entry cannot be overwritten before the
        // head has passed it); a ring that is full spills to the global
        // queue. The words of the entries taken are nulled by the owner,
        // a batch at a time and all of them when it runs out of work
        // (_clear_taken), so that a finished task's frame is not held by
        // a slot the ring has not come round to.
        //
        // One scheduler per process, started on the first enqueue,
        // stopped when the program ends or by scheduler::stop(): the
        // workers are joined then, so a task that never suspends never
        // lets the program end, as a thread would not; the next enqueue
        // starts it again.
        // What a worker calls with its index on its way to sleep, when set:
        // the batches slog's buffered loggers hold for the worker, written
        // (slog/detail/output.h). One relaxed load in the worker's loop
        inline std::atomic<void (*)(unsigned)> worker_idle_hook = {nullptr};

        // Called by the start with a worker's index before its thread is
        // made, when set: a test's way to have the start fail as
        // std::thread's does (it throws std::system_error);
        // tests/async/scheduler.cpp and the tests of the wakes
        inline std::atomic<void (*)(unsigned)> scheduler_start_test_hook = {nullptr};

        // Wakes that throw nothing, for the life of the object: a wake
        // that has to start the workers and cannot (std::system_error)
        // has queued its frame first (Scheduler::_enqueue_starting), and
        // in this scope the error is let go of, so the task runs at the
        // next start. What a destructor that wakes tasks puts around the
        // wakes (the end of a task_group, of a shared_mutex's writer
        // guard): a destructor cannot throw, and every wake of a list is
        // done, where a throw would leave the rest of the list asleep
        inline thread_local unsigned quiet_wakes = 0;

        // The end of the program (DESIGN 478). The runtime's singletons
        // (the scheduler, the reactor, the timers, the blocking pool) are
        // function-local statics, destroyed at exit in the reverse order of
        // their construction, while tasks may still be parked in them or
        // running. The first of their destructors to run calls
        // runtime_exit(), which sets Exiting, waits for every start of a
        // thread in progress to finish (RuntimeStart), and joins the
        // runtime's threads: the blocking pool's (the jobs queued run to
        // their end), the workers (what is ready runs until it suspends),
        // the timers' thread and the reactor's, the reactor's without
        // waking its waits (a task parked at exit stays parked, as a
        // goroutine does when main returns). No start succeeds from then
        // on, so no thread of the runtime runs while a singleton is
        // destroyed, and the rest of the exit is the main thread's alone:
        // the paths it may take into a destroyed singleton look at
        // Exiting before any lock (runtime_exiting) and lock nothing. A
        // frame made ready is dropped (enqueue_on_workers); a task's wait
        // for I/O, a timer or a blocking job leaves it parked; a thread's
        // I/O wait ends cancelled, its timer at once, its blocking job
        // runs on it. A word with no destructor, valid through every
        // static destructor: the Exiting bit and, from RuntimeStartUnit
        // up, the starts in progress
        inline constinit std::atomic<uint32_t> runtime_state = {0};
        inline constexpr uint32_t RuntimeExiting = 1;
        inline constexpr uint32_t RuntimeStartUnit = 1u << 8;

        // Whether the program is ending (runtime_exit has begun): one load,
        // on paths that already lock or start a thread
        SGCL_INLINE_HOT bool runtime_exiting() noexcept {
            return runtime_state.load(std::memory_order_acquire) & RuntimeExiting;
        }

        // The start of a thread of the runtime (the workers, the reactor's,
        // the timers', the pool's), with the lock it is made under and the
        // hook that stops it: counted while it runs, so that runtime_exit()
        // waits for it and then finds the thread and the hook, and refused
        // (false) once the program is ending. Two atomic operations on a
        // path that makes a thread
        class RuntimeStart {
        public:
            SGCL_INLINE_HOT RuntimeStart() noexcept
            : _in(!(runtime_state.fetch_add(RuntimeStartUnit, std::memory_order_seq_cst) & RuntimeExiting)) {
                if (!_in) {
                    runtime_state.fetch_sub(RuntimeStartUnit, std::memory_order_release);
                }
            }

            SGCL_INLINE_HOT ~RuntimeStart() {
                if (_in) {
                    runtime_state.fetch_sub(RuntimeStartUnit, std::memory_order_release);
                }
            }

            SGCL_INLINE_HOT explicit operator bool() const noexcept {
                return _in;
            }

            RuntimeStart(const RuntimeStart&) = delete;
            RuntimeStart& operator=(const RuntimeStart&) = delete;

        private:
            bool _in;
        };

        inline void runtime_exit();   // below the scheduler: what the first destructor of a singleton calls

        class QuietWakes {
        public:
            SGCL_INLINE_HOT QuietWakes() noexcept {
                ++quiet_wakes;
            }

            SGCL_INLINE_HOT ~QuietWakes() {
                --quiet_wakes;
            }

            QuietWakes(const QuietWakes&) = delete;
            QuietWakes& operator=(const QuietWakes&) = delete;
        };

        class Scheduler {
        public:
            using Frame = tracked_ptr<FrameWord>;

            // A worker's queues, a managed object: the ring, its head
            // (thieves and the owner, a compare-exchange) and tail (the
            // owner, a store), and the next slot (the owner alone).
            // The words every looking worker reads (head and tail, in each
            // round over all the rings) a line apart from the ones the
            // owner writes on every hand-over (next) and from the ring:
            // on one line, each look pulled the owner's line away and its
            // next write waited for it back, and where the lines fell
            // depended on where the object did (a rendezvous between four
            // tasks 360 or 250 ns per item from one build to the next, a
            // word added to this struct enough to move it: DESIGN 454).
            // Padding rather than alignas, as in concurrent::queue: the
            // fields lie a line apart wherever the object starts
            struct Local {
                static constexpr uint32_t Size = 256;
                static constexpr uint32_t ClearBatch = 16;   // the slots taken nulled this many at a time (_clear_taken)

                std::atomic<uint32_t> head = {0};
                std::atomic<uint32_t> tail = {0};
                unsigned char _pad0[config::cache_line_size - 2 * sizeof(std::atomic<uint32_t>)] = {};
                Frame next;
                std::atomic<uint32_t> park = {0};   // the worker's own wake word: 1 = woken with the credit of a looking worker
                uint32_t cleared = 0;               // the owner's: every slot below this index nulled since it was taken
                uintptr_t stack_floor = 0;          // the lowest address the worker's stack may be cleared to (_clear_dead_stack)
                Frame inbox;                        // a task handed to this worker while it looks, by the one handling the reactor's events (_hand_to; under inbox_lock)
                std::atomic<uint32_t> inbox_lock = {0};
                std::atomic<bool> in_queue = {false};   // asleep in the reactor's queue, not on `park` (written before the bit among the sleepers, read by the waker that takes the bit)
                std::atomic<bool> looking = {false};    // in _find_work: a task may be handed to it (set and cleared by the owner, cleared under inbox_lock)
                std::atomic<bool> has_inbox = {false};  // the inbox holds a task: the owner's look at it in every round
                unsigned char _pad1[config::cache_line_size - 2 * sizeof(Frame) - 3 * sizeof(uint32_t) - sizeof(uintptr_t) - 3 * sizeof(std::atomic<bool>)] = {};
                Frame slots[Size];
            };

            static constexpr unsigned MaxWorkers = 64;   // the bits of the set of sleepers
            // The dead stack a thread of the library clears before it parks
            // or looks for work (the workers here, the blocking pool's
            // threads and the timers' thread: clear_dead_stack): 4 KB in a
            // build with NDEBUG, which measurement showed enough (note
            // 254); 64 KB without, since the frames of -O0 are several
            // times larger (a resumed coroutine, a wait's awaiter, a
            // std::function) and the words they leave lie deeper than 4 KB
            // below the park, where the conservative scan keeps what they
            // point at alive. A definition of the build wins
#ifndef SGCL_WORKER_STACK_CLEAR
#if defined(NDEBUG)
#define SGCL_WORKER_STACK_CLEAR 4096
#else
#define SGCL_WORKER_STACK_CLEAR 65536
#endif
#endif
            static constexpr uint32_t NextChain = 61;   // the hand-overs a worker runs in a row before its ring gets a turn (_run), as the global queue gets one every 61 runs
            static constexpr size_t StackClearOnIdle = SGCL_WORKER_STACK_CLEAR;   // the dead stack a worker clears before looking for work: the frames of a task's last run, with what it called (a page; the tests clear 64 KB before their checks, config::stack_clear_size); the blocking pool's threads and the timers' thread clear as much before they park (clear_dead_stack, below)

            struct Global {
                concurrent::queue<Frame> ready;
            };

            SGCL_INLINE_HOT ~Scheduler() {
                stop();
            }

            // The workers joined; the queues stay, and what is on them
            // (ready, not yet run) runs at the next start, as does a frame
            // made ready meanwhile. A task suspended in a wait stays so
            // until something makes it ready (a program stops the
            // scheduler when nothing runs)
            void stop() {
                assert(!on_worker() && "scheduler::stop() from a task would join the calling thread");
                std::lock_guard lock(_lifecycle);
                if (_workers.empty()) {
                    return;
                }
                _stop.store(true, std::memory_order_release);
                _wake_all();
                for (auto& w : _workers) {
                    w.join();
                }
                _workers.clear();
                _running.store(false, std::memory_order_release);
                // the queues stay: a frame made ready from another thread
                // while the workers are being joined (a promise set from a
                // callback, a send from a plain thread) is pushed on them,
                // not on a freed queue, and runs at the next start, as the
                // page says; the counters start afresh with the workers
            }

            // The coroutine made ready: on its executor's queue when it has
            // one (executor.h: the frame's header says so; the executor's
            // thread, or a worker in the strand's turn, runs it), else on
            // this worker's queue (next, or at the end), or on the global
            // queue from any other thread. Every path that makes a frame
            // ready comes here (a spawn, a channel's wake, a timer, a task
            // done, a yield), which is what makes a task's executor stick
            // Quiet: the enqueue of a noexcept call (a task's end, a race's
            // call), as in a QuietWakes scope: a start of the workers that
            // fails is let go of, the frame queued for the next start
            template<bool Quiet = false>
            SGCL_INLINE_HOT void enqueue(Frame frame, bool next) {
                if (auto executor = frame_header(frame.get()).executor.get()) {
                    if constexpr (Quiet) {
                        QuietWakes quiet;   // a strand's push may hand its head to the workers
                        executor->push(std::move(frame), next);
                    } else {
                        executor->push(std::move(frame), next);
                    }
                    return;
                }
                enqueue_on_workers<Quiet>(std::move(frame), next);
            }

            // The pool's own queues: what a strand hands its frames to
            template<bool Quiet = false>
            void enqueue_on_workers(Frame frame, bool next) {
                if (!_running.load(std::memory_order_acquire)) [[unlikely]] {
                    if (!_start_for(&frame, 1, Quiet)) {
                        return;   // queued for the next start
                    }
                }
                if (auto local = _local) {
                    if (next) {
                        if (!local->next) {
                            // a hand-over to this worker, run when the running
                            // coroutine suspends: nobody else can take it (the
                            // next slot is not stolen from), so nobody is woken
                            local->next = std::move(frame);
                            return;
                        }
                        _push_local(*local, local->next);   // the one waiting to run next goes to the end, where others take it: the wake below
                        local->next = std::move(frame);
                    } else {
                        _push_local(*local, std::move(frame));
                    }
                } else {
#if SGCL_REACTOR_IN_WORKERS
                    if (_dispatching && _place_dispatched(frame)) [[unlikely]] {
                        return;   // its last worker's inbox, or the dispatching worker's to run: nothing to wake
                    }
#endif
                    _global->ready.push(std::move(frame));
                }
                _wake_one_if_none_looking();
            }

            SGCL_INLINE_HOT static bool on_worker() noexcept {
                return _local != nullptr;
            }

            // The calling worker's index (0 .. MaxWorkers - 1), or MaxWorkers
            // on any other thread: the timers' shard of a thread (timer.h)
            SGCL_INLINE_HOT static unsigned worker_index() noexcept {
                return _local ? _index : MaxWorkers;
            }

            // The workers looking for work now (with the credit of the ones
            // woken to look): the reactor's thread gives the seat at its
            // queue up when some do (reactor.h, PollSeat). Sequentially
            // consistent: its look after the seat's release, against a
            // looker's look at the seat after it stops looking
            SGCL_INLINE_HOT unsigned looking() const noexcept {
                return _spinning.load(std::memory_order_seq_cst);
            }

            // The workers of the running start (0: none)
            SGCL_INLINE_HOT unsigned pool() const noexcept {
                return _pool.load(std::memory_order_relaxed);
            }

            // Whether the seat's holder (PollSeat::holder) is a worker asleep
            // in the reactor's queue: the program idle, for the reactor's
            // thread's watch (reactor.h: _watch). Sequentially consistent,
            // after the thread's own store of its watch's end
#if SGCL_REACTOR_IN_WORKERS
            bool holder_asleep_in_queue(uint32_t holder) const noexcept {
                return holder != PollSeat::Free && holder - 1 < pool() && _locals[holder - 1]->in_queue.load(std::memory_order_seq_cst);
            }
#endif

            // The wakes made while the reactor's events are handled, by a
            // worker or by the reactor's thread, for the life of the object
            // (reactor.h: _seat_dispatch, _run). A task the events make
            // ready goes back to the worker that last ran it (the mark in
            // its frame's header, _resume) when that worker is looking for
            // work, through its inbox (_hand_to): its connection's buffers
            // and state are in that core's cache, where a spinning worker
            // that stole it from the global queue spread one connection over
            // the cores (Server-Sent Events 1910 ns at one worker, 3900 to
            // 4400 from two on, measured). Else the worker handling the
            // events keeps the first it is given to run itself (`keep`), and
            // one it ran last itself goes to its next slot (Go's runnext);
            // the rest go where a thread that is no worker puts them, the
            // global queue, where each worker woken or looking takes one
            // (on the worker's own ring a thief takes half, the rest waiting
            // behind the thief's task). A worker's wakes in the scope are
            // a thread's that is no worker (its _local cleared), so that the
            // wakes the events cause in turn (a channel closed for a select,
            // a strand's turn) take the same path
            class DispatchScope {
            public:
                SGCL_INLINE_HOT explicit DispatchScope(Frame* keep) noexcept
                : _saved(std::exchange(_local, nullptr)) {
                    _dispatch_self = _saved;
                    _dispatch_keep = keep;
                    _dispatching = true;
                }

                SGCL_INLINE_HOT ~DispatchScope() {
                    _local = _saved;
                    _dispatching = false;
                    _dispatch_self = nullptr;
                    _dispatch_keep = nullptr;
                }

                DispatchScope(const DispatchScope&) = delete;
                DispatchScope& operator=(const DispatchScope&) = delete;

            private:
                Local* _saved;
            };

            // The queues as they are: for the benchmarks and a look at a load
            struct Statistics {
                unsigned workers = 0;        // the threads of the pool (0: not started)
                size_t global_queued = 0;    // tasks on the global queue
                size_t local_queued = 0;     // tasks on the workers' rings and next slots, together
                unsigned spinning = 0;       // workers looking for work
                unsigned sleeping = 0;       // workers asleep in the kernel
            };

            Statistics statistics() {
                Statistics st;
                if (runtime_exiting()) {
                    return st;   // the workers joined, the mutex perhaps destroyed (runtime_exit)
                }
                std::lock_guard lock(_lifecycle);
                if (!_running.load(std::memory_order_acquire)) {
                    return st;
                }
                st.workers = (unsigned)_workers.size();
                st.global_queued = _global->ready.size();
                for (auto& l : _locals) {
                    st.local_queued += l->tail.load(std::memory_order_acquire) - l->head.load(std::memory_order_acquire) + (l->next ? 1 : 0);
                }
                st.spinning = _spinning.load(std::memory_order_acquire);
                st.sleeping = (unsigned)std::popcount(_sleepers.load(std::memory_order_acquire));
                return st;
            }

            // The state, for a hang (debugging)
            void dump(FILE* out) noexcept {
                std::fprintf(out, "spinning %u sleepers %llx global empty %d stop %d\n", _spinning.load(), (unsigned long long)_sleepers.load(), (int)_global->ready.empty(), (int)_stop.load());
#if SGCL_REACTOR_IN_WORKERS
                std::fprintf(out, "  seat %x enabled %d inside %u\n", poll_seat.holder.load(), (int)poll_seat.enabled.load(), poll_seat.inside.load());
#endif
                for (unsigned i = 0; i < _locals.size(); ++i) {
                    auto& l = *_locals[i];
                    std::fprintf(out, "  w%u h %u t %u next %p park %u in_queue %d\n", i, l.head.load(), l.tail.load(), (void*)l.next.get(), l.park.load(), (int)l.in_queue.load());
                }
            }

            SGCL_INLINE_HOT unsigned workers() {
                if (!_running.load(std::memory_order_acquire)) [[unlikely]] {
                    if (!_start()) {
                        return 0;   // the program is ending: none started
                    }
                }
                return (unsigned)_workers.size();
            }

            // The number of workers (0: the hardware concurrency), over the
            // environment and the build's constant. Before the first start
            // it is what the start takes; after it the workers are stopped
            // (stop(): once the ready tasks have run) and started again
            // with the new number, the queues kept, the rings of workers
            // no longer there handed to the global queue (_start)
            void set_workers(unsigned n) {
                assert(!on_worker() && "scheduler::set_workers() from a task would join the calling thread");
                if (runtime_exiting()) {
                    return;
                }
                {
                    std::lock_guard lock(_lifecycle);
                    _workers_set = true;
                    _workers_asked = n;
                    if (!_running.load(std::memory_order_acquire) || _resolve_workers() == _workers.size()) {
                        return;
                    }
                }
                stop();
                _start();
            }

        private:
            friend class WakeBatch;
            friend struct SchedulerRingAccess;   // the tests of the ring (tests/async/scheduler.cpp): its operations on rings of their own

            void _gather(WakeBatch& batch, Frame& frame);   // below WakeBatch

            // A batch of wakes (WakeBatch) handed over: what as many
            // enqueues to the end of the queues would have left there,
            // with one publication and one wake. On a worker the frames go
            // to the end of the ring together, one look at the thieves'
            // head and one store of the tail for all of them, where every
            // enqueue made both on the line every spinning worker is
            // stealing from (a full ring spills to the global queue, as
            // _push_local's does); from any other thread the global queue
            // takes them as one chain, one exchange on its tail
            // (concurrent::queue: push_range)
            void _enqueue_batch(Frame* frames, unsigned n) {
                if (!_running.load(std::memory_order_acquire)) [[unlikely]] {
                    if (!_start_for(frames, n, false)) {
                        return;
                    }
                }
                if (auto local = _local) {
                    auto t = local->tail.load(std::memory_order_relaxed);
                    auto h = local->head.load(std::memory_order_acquire);
                    auto k = std::min<uint32_t>(n, Local::Size - (t - h));
                    _clear_before_push(*local, h, t + k);
                    for (uint32_t i = 0; i < k; ++i) {
                        local->slots[(t + i) % Local::Size] = std::move(frames[i]);
                    }
                    if (k) {
                        local->tail.store(t + k, std::memory_order_release);
                    }
                    for (uint32_t i = k; i < n; ++i) {
                        _global->ready.push(std::move(frames[i]));
                    }
                } else {
                    _global->ready.push_range(frames, frames + n);
                }
                _wake_one_if_none_looking();
            }

            // The number the next start takes (under _lifecycle): the
            // program's, else the environment's (read once, at the first
            // start), else the build's; 0 the hardware concurrency; at
            // most MaxWorkers
            unsigned _resolve_workers() noexcept {
                if (!_env_read) {
                    _env_read = true;
                    _workers_env = env_unsigned("SGCL_WORKERS", config::workers);
                    const unsigned spin = env_unsigned("SGCL_WORKER_SPIN_US", config::worker_spin_microseconds);
                    if (!worker_spin_set.load(std::memory_order_relaxed)) {
                        worker_spin_us.store(spin, std::memory_order_relaxed);
                    }
                }
                const unsigned asked = _workers_set ? _workers_asked : _workers_env;
                return std::min(MaxWorkers, asked ? asked : std::max(1u, std::thread::hardware_concurrency()));
            }

            // The workers started for frames made ready while they were
            // not running (before the first start, or after a stop): true
            // when they run, and the frames go on as any enqueue's. A start
            // that fails (std::thread's std::system_error, the one throw of
            // a wake) loses no task: the frames go on the global queue,
            // where they wait for the next start, and the error is the
            // waker's, unless the waker is quiet (a QuietWakes scope, a
            // destructor's; or a noexcept call's enqueue), which lets it
            // go: false then. The program ending (runtime_exit): nothing
            // started, false, and the frames dropped, their tasks left
            // where they were. Out of line, after the start, so that the
            // enqueue's own path is what it was
            SGCL_NOINLINE bool _start_for(Frame* frames, unsigned n, bool quiet) {
                try {
                    return _start();
                } catch (const std::system_error&) {
                    {
                        std::lock_guard lock(_lifecycle);
                        _make_queues();
                    }
                    if (n == 1) {
                        _global->ready.push(std::move(frames[0]));
                    } else {
                        _global->ready.push_range(frames, frames + n);
                    }
                    if (!quiet && quiet_wakes == 0) {
                        throw;
                    }
                    return false;
                }
            }

            // The queues, made once and kept across stops (stop): what was
            // pushed while the workers were away runs at the next start.
            // Under _lifecycle
            SGCL_INLINE_HOT void _make_queues() {
                if (!_global) {
                    _global = make_tracked<Global>();
                    _locals.reserve(MaxWorkers);   // never reallocated: a thread that enqueues reads _locals[i] of a sleeper's bit without the lock
                }
            }

            // The workers started. A worker's thread the system refuses
            // (std::thread's std::system_error, thrown to the caller): the
            // workers made before it are joined without having run
            // anything (they wait at the gate until the start is done),
            // and the scheduler is as it was, not running, its queues
            // kept; the next start makes every worker again. False, and
            // nothing touched, once the program is ending (runtime_exit)
            bool _start() {
                RuntimeStart starting;
                if (!starting) {
                    return false;
                }
                std::lock_guard lock(_lifecycle);
                if (_running.load(std::memory_order_acquire)) {
                    return true;
                }
                _sleepers.store(0, std::memory_order_relaxed);
                _spinning.store(0, std::memory_order_relaxed);
                auto n = _resolve_workers();
                _make_queues();
                for (auto& l : _locals) {
                    l->park.store(0, std::memory_order_relaxed);   // the wake of the stop, taken by nobody: a worker starting with it would count itself woken and leave its bit among the sleepers
                }
                while (_locals.size() < n) {
                    _locals.push_back(make_tracked<Local>());
                }
                // fewer workers than rings (set_workers): what the rings
                // past n hold, and their next slots, go to the global
                // queue, which the workers take from; nobody runs them
                // meanwhile (the workers are joined)
                for (size_t i = n; i < _locals.size(); ++i) {
                    Local& l = *_locals[i];
                    if (l.next) {
                        _global->ready.push(l.next);
                        l.next = nullptr;
                    }
                    auto h = l.head.load(std::memory_order_relaxed);
                    auto t = l.tail.load(std::memory_order_relaxed);
                    for (; h != t; ++h) {
                        _global->ready.push(l.slots[h % Local::Size]);
                    }
                    l.head.store(t, std::memory_order_relaxed);
                    _clear_taken(l, t);
                }
                _active = n;
                _pool.store(n, std::memory_order_relaxed);
                _stop.store(false, std::memory_order_release);
                _gate.store(GateClosed, std::memory_order_relaxed);
                _workers.reserve(n);
                try {
                    for (unsigned i = 0; i < n; ++i) {
                        if (auto hook = scheduler_start_test_hook.load(std::memory_order_relaxed)) [[unlikely]] {
                            hook(i);
                        }
                        _workers.emplace_back([this, i] {
                            if (_through_gate()) {
                                _run(i);
                            }
                        });
                    }
                } catch (...) {
                    _gate.store(GateAborted, std::memory_order_release);
                    _gate.notify_all();
                    for (auto& w : _workers) {
                        w.join();
                    }
                    _workers.clear();
                    throw;
                }
                _running.store(true, std::memory_order_release);
                _gate.store(GateOpen, std::memory_order_release);
                _gate.notify_all();
                return true;
            }

            SGCL_INLINE_HOT static void _resume(const Frame& frame, void* mark) {
#if SGCL_REACTOR_IN_WORKERS
                frame_header(frame.get()).unused.word = mark;   // the worker that runs it, index + 1: a small number, never an address (the frame is traced conservatively)
#else
                (void)mark;
#endif
                resume_frame(frame);   // the frame held by the caller's local while the coroutine runs (by reference down from there: a tracked_ptr copied is a barrier and a registration check each)
            }

            // A worker of a start waits until every worker is made (true)
            // or one cannot be (false: it runs nothing and ends), so that
            // no frame is run by a start that may yet fail: a worker of it
            // would enqueue, find the scheduler not running and wait for
            // the lock its starter holds while joining it. Apart from
            // _run, whose loop is the hot one
            SGCL_NOINLINE bool _through_gate() noexcept {
                int gate;
                while ((gate = _gate.load(std::memory_order_acquire)) == GateClosed) {
                    _gate.wait(GateClosed, std::memory_order_acquire);
                }
                return gate == GateOpen;
            }

            void _run(unsigned index) {
                _local = _locals[index].get();
                _index = index;
                detail::thread_place = int(index);   // on_unhandled's default names the worker (coroutine.h)
                Local& local = *_local;
                local.stack_floor = detail::current_thread().stack_begin() + config::stack_guard_margin;
                uint32_t tick = 0;
                uint32_t chain = 0;   // frames run from `next` in a row
                void* const mark = (void*)(uintptr_t)(index + 1);   // this worker in the header of every frame it runs (_resume)
#if SGCL_REACTOR_IN_WORKERS
                bool look_again = false;   // the limit of the lookers (_may_look) passed over once, after a sleep
#endif
                for (;;) {
                    if (local.next) {
                        // A chain of hand-overs (a ping-pong over a channel,
                        // a loop of co_await) would hold this worker for as
                        // long as it goes on, and the frames of its ring and
                        // the global queue would wait for its end: with one
                        // worker, forever. Every NextChain-th the frame goes
                        // to the end of the ring instead, behind the others
                        if (++chain < NextChain) {
                            Frame f = local.next;   // a move of a tracked_ptr is a copy: taken, then cleared
                            local.next = nullptr;
                            _resume(std::move(f), mark);
                            continue;
                        }
                        _push_local(local, local.next);
                        local.next = nullptr;
                    }
                    chain = 0;
                    // the global queue every so often, so that its tasks are
                    // not starved by a busy ring
                    if (++tick % 61 == 0) {
                        if (auto f = _global->ready.try_pop()) {
                            _resume(std::move(*f), mark);
                            continue;
                        }
                    }
                    if (auto f = _pop_local(local)) {
                        _resume(std::move(f), mark);
                        continue;
                    }
#if SGCL_REACTOR_IN_WORKERS
                    if (look_again || _may_look()) {
                        look_again = false;
                        if (auto f = _find_work(local)) {
                            _resume(std::move(f), mark);
                            continue;
                        }
                    } else {
                        // refused a look: what the look clears first, cleared
                        // before the sleep (the last task's words on this
                        // stack, the ring's slots taken), or the sleeper keeps
                        // that task's objects alive (_find_work)
                        _clear_dead_stack(local);
                        _clear_taken(local, local.head.load(std::memory_order_acquire));
                    }
#else
                    if (auto f = _find_work(local)) {
                        _resume(std::move(f), mark);
                        continue;
                    }
#endif
                    if (_stop.load(std::memory_order_acquire)) {
#if SGCL_REACTOR_IN_WORKERS
                        if (_unseat()) {
                            poll_seat.wake_backstop();   // nobody else to read the reactor's queue
                        }
#endif
                        return;
                    }
                    if (auto hook = worker_idle_hook.load(std::memory_order_relaxed)) {
                        hook(index);   // slog's buffered lines of this worker, written before it sleeps
                    }
                    Frame found;   // a task the reactor's queue made ready, taken in the sleep
#if SGCL_REACTOR_IN_WORKERS
                    look_again = true;   // back from the sleep (work seen, a task run): the next look is not refused
#endif
                    switch (_sleep(local, found)) {
                    case Woken::credited:
                        // woken with the credit of a looking worker: this
                        // one is looking now, and the credit is its own
                        if (found) {
                            _leave_with_work();
                            _resume(found, mark);
                        } else if (auto f = _find_work(local, true)) {
                            _resume(std::move(f), mark);
                        }
                        break;
                    case Woken::look:
                        // the seat at the reactor's queue is this worker's,
                        // and work came as it was going to sleep: a look
                        // (which keeps the seat while it finds nothing)
                        if (auto f = _find_work(local)) {
                            _resume(std::move(f), mark);
                        }
                        break;
                    case Woken::none:
                        if (found) {
                            _resume(found, mark);
                        }
                        break;
                    }
                }
            }

            // A worker that ran out of work and was going to sleep: how it
            // came back (_sleep)
            enum class Woken {
                none,       // by itself: work seen, a stop, or a task of the reactor's queue (not counted as looking)
                credited,   // by a wake, with the credit of a looking worker
                look        // by itself, as a looker (the seat at the reactor's queue kept)
            };

#if SGCL_REACTOR_IN_WORKERS
            // Go's limit of the lookers (findrunnable: 2 * nmspinning <
            // gomaxprocs - npidle), at a worker's own start of a look (one
            // woken with the credit always looks): one looker always, more
            // while they are fewer than half the workers awake (running or
            // looking). DESIGN 251 measured it and turned it down while the
            // reactor's thread woke workers: the one that had just finished
            // was refused, the lone looker's window expired, and every
            // event of a ping-pong through the reactor woke a worker
            // through the kernel. With the workers reading the queue a
            // worker refused here sleeps in the queue when the seat is free
            // and runs the event's task itself, one wake as Go's netpoll
            SGCL_INLINE_HOT bool _may_look() const noexcept {
                const unsigned looking = _spinning.load(std::memory_order_relaxed);
                if (looking == 0) {
                    return true;
                }
                const unsigned awake = _active - (unsigned)std::popcount(_sleepers.load(std::memory_order_relaxed));
                return 2 * looking < awake;
            }
#endif

            // A looker, counted with the credit or by itself, stops looking
            // with a task in hand: the count down, the seat at the
            // reactor's queue given up if it is this worker's, and someone
            // still looking if this was the last looker, or the seat given
            // up (_find_work: the same at its end)
            void _leave_with_work() noexcept {
                const auto looking = _spinning.fetch_sub(1, std::memory_order_acq_rel);
#if SGCL_REACTOR_IN_WORKERS
                const bool unseated = _unseat();
#else
                const bool unseated = false;
#endif
                if (looking == 1 || unseated) {
                    _keep_looking();
                }
            }

            // The owner's own queues: what a task of the reactor's queue,
            // handled on this worker, put in its next slot or its ring (a
            // channel's wake, a strand's turn)
            Frame _take_own(Local& local) noexcept {
                if (local.next) {
                    Frame f = local.next;
                    local.next = nullptr;
                    return f;
                }
                return _pop_local(local);
            }

#if SGCL_REACTOR_IN_WORKERS
            // The seat at the reactor's queue: this worker's already, or
            // taken now that it is free (true); never while the queue is
            // closed to the workers
            SGCL_INLINE_HOT bool _seated() noexcept {
                if (!poll_seat.enabled.load(std::memory_order_acquire)) {   // acquire: the reactor's calls, set before it
                    return false;
                }
                const uint32_t me = _index + 1;
                uint32_t h = poll_seat.holder.load(std::memory_order_relaxed);
                if (h == me) {
                    return true;
                }
                return h == PollSeat::Free && poll_seat.holder.compare_exchange_strong(h, me, std::memory_order_acq_rel, std::memory_order_relaxed);
            }

            // The seat given up, when it is this worker's (true): the
            // caller then makes sure someone takes it (_keep_looking, or
            // the reactor's thread asked)
            SGCL_INLINE_HOT bool _unseat() noexcept {
                uint32_t me = _index + 1;
                return poll_seat.holder.load(std::memory_order_relaxed) == me && poll_seat.holder.compare_exchange_strong(me, PollSeat::Free, std::memory_order_seq_cst, std::memory_order_relaxed);
            }

            // A look at the reactor's queue that does not wait, by the
            // seat's holder between its looks for work: the first task
            // made ready, or one the events put on this worker's own
            // queues, else null
            Frame _poll(Local& mine) {
                alignas(16) unsigned char events[PollSeat::BufferBytes];
                Frame first;
                int n = poll_seat.wait(events, PollSeat::Capacity, false);
                if (n > 0) {
                    poll_seat.dispatch(events, n, first);
                    if (!first) {
                        first = _take_own(mine);
                    }
                } else if (n < 0 && _unseat()) {
                    poll_seat.wake_backstop();   // the queue closed or failed: the reactor's thread sees to it
                }
                return first;
            }
#endif

#if SGCL_REACTOR_IN_WORKERS
            // A task made ready in a DispatchScope: handed to the worker
            // that last ran it while that worker looks, else kept by the
            // dispatching worker (the first), or put in its next slot when
            // it ran the task last; false: to the queues as any other
            bool _place_dispatched(Frame& frame) {
                const auto last = (uintptr_t)frame_header(frame.get()).unused.word;
                Local* self = _dispatch_self;
                Local* owner = last && last <= pool() ? _locals[last - 1].get() : nullptr;
                if (owner && owner != self && _hand_to(*owner, frame)) {
                    return true;
                }
                if (auto keep = _dispatch_keep; keep && !*keep) {
                    *keep = std::move(frame);
                    return true;
                }
                if (owner && owner == self && !self->next) {
                    self->next = std::move(frame);   // run after the one kept, on this worker (the owner's own slot: _local is cleared, not changed)
                    return true;
                }
                return false;
            }

            // The task to a worker's inbox, while it looks (true). Under
            // the inbox's lock, which the owner takes as it stops looking
            // (_inbox_leave): a task is put in only while the owner looks,
            // and the owner takes what is there when it stops, so none is
            // left in an inbox nobody looks at
            static bool _hand_to(Local& l, Frame& frame) noexcept {
                if (!l.looking.load(std::memory_order_relaxed)) {
                    return false;
                }
                _inbox_lock(l);
                const bool ok = l.looking.load(std::memory_order_relaxed) && !l.inbox;
                if (ok) {
                    l.inbox = std::move(frame);
                    l.has_inbox.store(true, std::memory_order_release);
                }
                _inbox_unlock(l);
                return ok;
            }

            // The owner: what its inbox holds, taken (null when empty)
            static Frame _inbox_take(Local& l) noexcept {
                _inbox_lock(l);
                Frame f = l.inbox;
                if (f) {
                    l.inbox = nullptr;
                    l.has_inbox.store(false, std::memory_order_relaxed);
                }
                _inbox_unlock(l);
                return f;
            }

            // The owner stops looking: no task handed from here on, and the
            // one handed meanwhile taken
            static Frame _inbox_leave(Local& l) noexcept {
                _inbox_lock(l);
                l.looking.store(false, std::memory_order_relaxed);
                Frame f = l.inbox;
                if (f) {
                    l.inbox = nullptr;
                    l.has_inbox.store(false, std::memory_order_relaxed);
                }
                _inbox_unlock(l);
                return f;
            }

            static void _inbox_lock(Local& l) noexcept {
                Backoff<16> backoff;
                while (l.inbox_lock.exchange(1, std::memory_order_acquire)) {
                    backoff();
                }
            }

            SGCL_INLINE_HOT static void _inbox_unlock(Local& l) noexcept {
                l.inbox_lock.store(0, std::memory_order_release);
            }
#endif

            // The owner's push, at the tail; a full ring spills to the
            // global queue
            void _push_local(Local& local, Frame frame) noexcept {
                auto t = local.tail.load(std::memory_order_relaxed);
                auto h = local.head.load(std::memory_order_acquire);
                if (t - h < Local::Size) {
                    _clear_before_push(local, h, t + 1);
                    local.slots[t % Local::Size] = std::move(frame);
                    local.tail.store(t + 1, std::memory_order_release);
                } else {
                    _global->ready.push(std::move(frame));
                }
            }

            // The owner's pop, at the head, against the thieves
            static Frame _pop_local(Local& local) noexcept {
                for (;;) {
                    auto h = local.head.load(std::memory_order_acquire);
                    auto t = local.tail.load(std::memory_order_relaxed);
                    if (h == t) {
                        return Frame();
                    }
                    Frame f = local.slots[h % Local::Size];
                    if (local.head.compare_exchange_strong(h, h + 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        if (h + 1 - local.cleared >= Local::ClearBatch) {
                            _clear_taken(local, h + 1);
                        }
                        return f;
                    }
                }
            }

            // The owner: the slots taken since the last clear, by the owner
            // or by thieves, nulled, so that a ring holds no frame but the
            // ones queued on it (the word of a slot taken would otherwise
            // hold the frame, and all a finished task's frame still points
            // to, until the ring came round to the slot again: a whole
            // request and its connection in the HTTP server, measured).
            // Only below `head`, as read by the owner (acquire): a thief
            // reads its slots before the exchange that moves the head past
            // them (release), so its reads happen before these stores; a
            // thief whose exchange fails may read a slot being nulled, a
            // relaxed load of an atomic word, and drops what it read. Only
            // the owner nulls: a thief's store could land after the ring
            // came round and the owner put a new frame in that slot, which
            // would be lost with the store. The null is the tracked_ptr's
            // own assignment (a null store needs no barrier)
            static void _clear_taken(Local& local, uint32_t head) noexcept {
                for (uint32_t i = local.cleared; i != head; ++i) {
                    local.slots[i % Local::Size] = nullptr;
                }
                local.cleared = head;
            }

            // The owner, about to fill the slots up to `end` (exclusive):
            // a slot there may be one taken and not yet nulled, whose null
            // would then fall on the new frame, so the taken ones below the
            // head are nulled first when the fill reaches them (end is at
            // most head + Size), and in batches of ClearBatch otherwise
            SGCL_INLINE_HOT static void _clear_before_push(Local& local, uint32_t head, uint32_t end) noexcept {
                if (end - local.cleared > Local::Size || head - local.cleared >= Local::ClearBatch) {
                    _clear_taken(local, head);
                }
            }

            // Half of a victim's ring taken: the first entry returned, the
            // rest moved to this worker's ring. The entries are read, then
            // the victim's head exchanged past them; a failed exchange
            // leaves the copies in this ring's free slots, which its tail
            // never reached, and nulls them. The victim's slots taken are
            // the victim's to null (_clear_taken)
            Frame _steal(Local& victim, Local& mine) noexcept {
                for (;;) {
                    auto h = victim.head.load(std::memory_order_acquire);
                    auto t = victim.tail.load(std::memory_order_acquire);
                    auto n = t - h;
                    n = n - n / 2;
                    if (n == 0 || n > Local::Size) {
                        return Frame();
                    }
                    auto mt = mine.tail.load(std::memory_order_relaxed);
                    auto room = Local::Size - (mt - mine.head.load(std::memory_order_acquire));
                    if (n - 1 > room) {
                        n = room + 1;
                    }
                    assert(mine.cleared == mine.head.load(std::memory_order_relaxed) && "a thief's ring is empty and cleared (_find_work): no slot it fills is nulled later");
                    Frame first = victim.slots[h % Local::Size];
                    for (uint32_t i = 1; i < n; ++i) {
                        mine.slots[(mt + i - 1) % Local::Size] = victim.slots[(h + i) % Local::Size];
                    }
                    if (victim.head.compare_exchange_strong(h, h + n, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        if (n > 1) {
                            mine.tail.store(mt + n - 1, std::memory_order_release);
                        }
                        return first;
                    }
                    // the copies let go of: past this ring's tail no thief
                    // reads them, and they would hold their frames
                    for (uint32_t i = 1; i < n; ++i) {
                        mine.slots[(mt + i - 1) % Local::Size] = nullptr;
                    }
                }
            }

            // The dead stack below the caller zeroed, StackClearOnIdle of
            // it at most and never past the floor: collector::clear_stack
            // without its query of the pages touched (a call into the
            // system per clear, 150 ns: more than the zeroing), since the
            // task that just ran touched this region, or a page fault
            // maps it once
            SGCL_NOINLINE static void _clear_dead_stack(Local& local) noexcept {
                uintptr_t here = (uintptr_t)&here;
                uintptr_t limit = here > StackClearOnIdle ? here - StackClearOnIdle : 0;
                if (limit < local.stack_floor) {
                    limit = local.stack_floor;
                }
                detail::os::hidden_call([](void*) {}, nullptr, limit);
            }

            // Nothing of its own: the global queue, then the other
            // workers' rings, from a random one round, for the spin
            // window; this worker counts as looking (spinning) meanwhile,
            // and the one that finds work wakes the next sleeper, so that
            // someone is still looking while work keeps coming.
            // The last looker to leave empty-handed looks once more, after
            // a fence, at every ring and the global queue (Go's
            // findrunnable after it drops nmspinning): a push that saw it
            // still looking woke nobody (_wake_one_if_none_looking), and
            // its last round may have passed that ring before the push, so
            // without the look the frame would wait in a busy worker's
            // ring with every other worker asleep. Either the push's fence
            // comes first, and the look sees the frame, or the looker's
            // does, and the push sees no one looking and wakes a sleeper.
            // Work seen: this worker looks again as a looker (the count up
            // again, a new round), so that a frame taken as the last
            // looker wakes the next sleeper, as any other
            Frame _find_work(Local& mine, bool credited = false) noexcept {
                // The dead part of this worker's stack cleared first: the
                // frames of the task that just ran are below here, their
                // words the collector's conservative scan still sees, and
                // an object referenced only there would live until the
                // worker's next run over that region (a test saw a value
                // outlive its broadcast by a whole idle worker, under TSan
                // half the time). A few kilobytes, once per run out of
                // work, not per hop: a wake into the next slot never comes
                // through here. The ring's slots taken are nulled here too,
                // all of them: an idle worker holds no finished frame (and
                // an empty ring, cleared to its head, is what _steal fills)
                _clear_dead_stack(mine);
                _clear_taken(mine, mine.head.load(std::memory_order_acquire));
                if (!credited) {
                    _spinning.fetch_add(1, std::memory_order_acq_rel);
                }
                Frame found;
            look:
#if SGCL_REACTOR_IN_WORKERS
                mine.looking.store(true, std::memory_order_relaxed);
#endif
                const uint64_t until = spin_until();
                detail::Backoff<32> backoff;
                do {
#if SGCL_REACTOR_IN_WORKERS
                    if (mine.has_inbox.load(std::memory_order_acquire)) {
                        if ((found = _inbox_take(mine))) {
                            break;   // a task of the reactor's queue that ran here last
                        }
                    }
#endif
                    if (auto f = _global->ready.try_pop()) {
                        found = std::move(*f);
                        break;
                    }
                    auto n = _active;   // the rings of this start's workers (the ones past them were emptied by _start)
                    auto start = _random() % n;
                    for (unsigned k = 0; k < n && !found; ++k) {
                        auto v = (start + k) % n;
                        if (v != _index) {
                            found = _steal(*_locals[v], mine);
                        }
#if SGCL_REACTOR_IN_WORKERS
                        if (mine.has_inbox.load(std::memory_order_relaxed)) {
                            break;   // handed a task: taken at the top of the round, before any other
                        }
#endif
                    }
                    if (found || _stop.load(std::memory_order_acquire)) {
                        break;
                    }
#if SGCL_REACTOR_IN_WORKERS
                    if (mine.has_inbox.load(std::memory_order_relaxed)) {
                        continue;
                    }
#endif
#if SGCL_REACTOR_IN_WORKERS
                    // the reactor's queue, by the one holding the seat (or
                    // taking it, free): Go's netpoll(0) in findrunnable
                    if (_seated()) {
                        if ((found = _poll(mine))) {
                            break;
                        }
                    }
#endif
#if SGCL_REACTOR_IN_WORKERS
                    // the pauses watch the inbox: a task handed here is the
                    // hand-over of a ping-pong's every hop, and a round ends
                    // only after every ring, the queue and the pauses
                    for (unsigned i = 0; i < backoff.pauses && !mine.has_inbox.load(std::memory_order_relaxed); ++i) {
                        os::spin_pause();
                    }
                    if (backoff.pauses < 32) {
                        backoff.pauses *= 2;
                    }
#else
                    backoff();
#endif
                } while (cpu_ticks() < until);
#if SGCL_REACTOR_IN_WORKERS
                if (Frame handed = _inbox_leave(mine)) {
                    if (found) {
                        _push_local(mine, std::move(found));   // the other one to the ring, where the next looker takes it (the cascade below wakes one when this was the last looker)
                    }
                    found = std::move(handed);
                }
#endif
                const auto looking = _spinning.fetch_sub(1, std::memory_order_acq_rel);
#if SGCL_REACTOR_IN_WORKERS
                // with work in hand the seat goes (a worker that runs a task
                // reads no queue); without, it is kept into the sleep, which
                // waits in the queue
                const bool unseated = found && _unseat();
#else
                const bool unseated = false;
#endif
                if (looking == 1) {
                    if (found) {
                        _keep_looking();
                    } else if (!_stop.load(std::memory_order_acquire)) {
                        std::atomic_thread_fence(std::memory_order_seq_cst);   // against the push's fence in _wake_one_if_none_looking
                        if (_work_elsewhere()) {
                            _spinning.fetch_add(1, std::memory_order_acq_rel);
                            goto look;
                        }
                    }
                } else if (unseated) {
                    _keep_looking();   // others were looking at the count: one of them takes the seat, or, gone since, sees it free (_keep_looking after its fence)
                }
                return found;
            }

            // Work on the global queue or in another worker's ring: the look
            // of the last looker as it leaves (_find_work) and of a worker
            // about to sleep (_sleep), each after a fence that pairs with
            // the fence of a push (_wake_one_if_none_looking). The rings
            // of the workers whose bit is among the sleepers are skipped,
            // since a sleeper's ring is empty:
            // - only its owner writes to a ring (a push, a batch, the half
            //   of a victim's ring a thief moves into its own ring), and a
            //   worker sets its bit (_sleep) only once its ring is empty
            //   (its pop failed and its look found nothing: a steal fills
            //   the thief's ring only when it returns a frame);
            // - the bit comes off before the worker runs again: the waker
            //   takes it and then stores the park word with release, which
            //   the worker loads with acquire before it runs; a worker that
            //   finds work itself takes its bit before it returns.
            // So a frame the worker pushes after it wakes is ordered after
            // the bit came off, and its push's fence decides: when that
            // fence comes first, this look after its own fence sees the bit
            // gone and the ring's tail with it; when this look's fence
            // comes first, the push sees this looker leaving (_spinning at
            // zero) or its bit among the sleepers, and wakes. A bit read
            // as still set means the worker has not run, so has pushed
            // nothing yet
            bool _work_elsewhere() const noexcept {
                if (!_global->ready.empty()) {
                    return true;
                }
                const uint64_t active = _active >= MaxWorkers ? ~uint64_t(0) : (uint64_t(1) << _active) - 1;
                uint64_t awake = active & ~_sleepers.load(std::memory_order_acquire) & ~(uint64_t(1) << _index);
                while (awake) {
                    const auto v = (unsigned)std::countr_zero(awake);
                    awake &= awake - 1;
                    if (_has_work(*_locals[v])) {
                        return true;
                    }
                }
                return false;
            }

            // The sleep: this worker's bit set in the set of sleepers
            // before its last look (against the enqueue that pushes and
            // then looks at the set: one of the two sees the other), then
            // the wait on its own word. The look covers every ring, not
            // only its own: a push into a busy worker's ring that saw
            // nobody looking (the count already down) and no bit of this
            // worker yet woke nobody when no other worker slept, and only
            // this look finds its frame (Go's stopm comes after the P is
            // given back to the idle list and the queues looked at again).
            // Woken::credited when woken by a wake (with the credit), none
            // when it found work itself or the scheduler stops.
            //
            // With the seat at the reactor's queue (SGCL_REACTOR_IN_WORKERS)
            // the wait is in the queue instead of on the word: the kernel
            // ends it for an event of a descriptor, and a wake for it (a
            // sleeper's bit taken, as for any sleeper) is a trigger of the
            // queue's user event (PollSeat::wake_poller), which the waker
            // sends instead of the word's notify when it reads `in_queue`,
            // written before the bit. Back from the queue the bit is taken
            // back (or the wake's credit taken) before the events are
            // handled, since they may put tasks on this worker's ring, which
            // a sleeper's bit says is empty (_work_elsewhere). The first task
            // the events make ready is this worker's to run (`found`): with
            // the credit it stops looking (the caller: _leave_with_work);
            // without, it is not counted as looking, and as Go's findrunnable
            // after its netpoll it gives the seat up and makes sure someone
            // else looks (_keep_looking: a spinner takes the seat, or a
            // sleeper is woken to, or the reactor's thread when every worker
            // is busy). Events that made no task ready for this worker (a
            // thread's wait, a channel, a stale wake): the sleep again.
            // A seat that is free at the look after the bit (its holder gave
            // it up before it could see this bit) is taken by a look instead
            // of a sleep on the word: a sleeper on the word is not woken
            // for a seat (_keep_looking wakes one only when nobody looks,
            // and the reactor's thread when it finds no bit)
            Woken _sleep(Local& local, Frame& found) noexcept {
                auto bit = uint64_t(1) << _index;
#if SGCL_REACTOR_IN_WORKERS
            again:
                const bool seated = _seated();
                local.in_queue.store(seated, std::memory_order_relaxed);   // published by the bit's release
#else
                (void)found;   // only the reactor's queue gives one
#endif
                _sleepers.fetch_or(bit, std::memory_order_acq_rel);
                std::atomic_thread_fence(std::memory_order_seq_cst);
                if (_has_work(local) || _work_elsewhere() || _stop.load(std::memory_order_acquire)
#if SGCL_REACTOR_IN_WORKERS
                    || (!seated && poll_seat.enabled.load(std::memory_order_relaxed) && poll_seat.holder.load(std::memory_order_seq_cst) == PollSeat::Free)
#endif
                ) {
                    if (_sleepers.fetch_and(~bit, std::memory_order_acq_rel) & bit) {
                        // the bit was still ours: nobody woke us
#if SGCL_REACTOR_IN_WORKERS
                        local.in_queue.store(false, std::memory_order_relaxed);
                        if (seated || poll_seat.holder.load(std::memory_order_relaxed) == PollSeat::Free) {
                            return Woken::look;   // a look keeps (or takes) the seat while it finds nothing
                        }
#endif
                        return Woken::none;
                    }
                    while (local.park.load(std::memory_order_acquire) == 0) {   // a wake is on its way: take its credit
                    }
                    local.park.store(0, std::memory_order_relaxed);
#if SGCL_REACTOR_IN_WORKERS
                    local.in_queue.store(false, std::memory_order_relaxed);
#endif
                    return Woken::credited;
                }
#if SGCL_REACTOR_IN_WORKERS
                if (seated) {
                    alignas(16) unsigned char events[PollSeat::BufferBytes];
                    const int n = poll_seat.wait(events, PollSeat::Capacity, true);
                    bool credited = false;
                    if (!(_sleepers.fetch_and(~bit, std::memory_order_acq_rel) & bit)) {
                        while (local.park.load(std::memory_order_acquire) == 0) {   // the waker's store comes before its trigger
                        }
                        local.park.store(0, std::memory_order_relaxed);
                        credited = _spinning_credit_taken();
                    }
                    local.in_queue.store(false, std::memory_order_relaxed);
                    if (n > 0) {
                        poll_seat.dispatch(events, n, found);
                        if (!found) {
                            found = _take_own(local);
                        }
                    } else if (n < 0 && _unseat()) {
                        poll_seat.wake_backstop();   // the queue closed or failed: the reactor's thread sees to it
                    }
                    if (credited) {
                        return Woken::credited;
                    }
                    if (found) {
                        if (_unseat()) {
                            _keep_looking();
                        }
                        return Woken::none;
                    }
                    if (_stop.load(std::memory_order_acquire)) {
                        return Woken::none;
                    }
                    goto again;
                }
#endif
                while (local.park.load(std::memory_order_acquire) == 0) {
                    local.park.wait(0, std::memory_order_acquire);
                }
                local.park.store(0, std::memory_order_relaxed);
                return _spinning_credit_taken() ? Woken::credited : Woken::none;
            }

            // A wake by stop() carries no credit; one by an enqueue does
            SGCL_INLINE_HOT bool _spinning_credit_taken() const noexcept {
                return !_stop.load(std::memory_order_acquire);
            }

            SGCL_INLINE_HOT static bool _has_work(const Local& local) noexcept {
                return local.head.load(std::memory_order_acquire) != local.tail.load(std::memory_order_acquire);
            }

            // One sleeper woken, with the credit of a looking worker, when
            // none is looking: its bit taken out of the set (exactly one
            // waker gets it), the credit counted, its word set
            void _wake_one_if_none_looking() noexcept {
                std::atomic_thread_fence(std::memory_order_seq_cst);
                if (_spinning.load(std::memory_order_relaxed) != 0) {
                    return;
                }
                (void)_wake_one();
            }

            // The same for a looker gone with work in hand as the last one
            // (the cascade), or a seat at the reactor's queue given up: when
            // nobody looks and no sleeper is there to wake (every worker
            // busy), and the seat is free, the reactor's thread is told to
            // watch it (reactor.h: _watch): it takes the seat when no worker
            // has read the queue for a while, so that the queue is read while
            // the workers run long tasks or block (a thread's wait on a
            // descriptor, a worker blocked in one), and stays out of the
            // queue while they come back to it between short tasks (Go: the
            // network polled by the scheduler when idle, by sysmon after 10
            // ms). Its fence pairs with a looker's fence after its count
            // goes down, with a sleeper's after its bit (see PollSeat), and
            // with the thread's after the end of its watch
            void _keep_looking() noexcept {
                std::atomic_thread_fence(std::memory_order_seq_cst);
                if (_spinning.load(std::memory_order_relaxed) != 0) {
                    return;
                }
                if (_wake_one()) {
                    return;
                }
#if SGCL_REACTOR_IN_WORKERS
                if (poll_seat.enabled.load(std::memory_order_acquire) && poll_seat.holder.load(std::memory_order_seq_cst) == PollSeat::Free) {
                    poll_seat.wake_backstop();
                }
#endif
            }

            // One sleeper's bit taken, the credit counted, its wake sent:
            // false when no bit was there. A sleeper asleep in the
            // reactor's queue is woken through the queue, and only when no
            // other sleeps: the others' wake costs as much, and leaves the
            // queue read
            bool _wake_one() noexcept {
                auto set = _sleepers.load(std::memory_order_acquire);
                while (set) {
                    auto pick = set;
#if SGCL_REACTOR_IN_WORKERS
                    const uint32_t h = poll_seat.holder.load(std::memory_order_relaxed);
                    if (h - 1 < MaxWorkers) {
                        if (auto others = set & ~(uint64_t(1) << (h - 1))) {
                            pick = others;
                        }
                    }
#endif
                    auto i = (unsigned)std::countr_zero(pick);
                    auto bit = uint64_t(1) << i;
                    if (_sleepers.fetch_and(~bit, std::memory_order_acq_rel) & bit) {
                        _spinning.fetch_add(1, std::memory_order_acq_rel);
                        auto& l = *_locals[i];
                        l.park.store(1, std::memory_order_release);
#if SGCL_REACTOR_IN_WORKERS
                        if (l.in_queue.load(std::memory_order_relaxed)) {
                            poll_seat.wake_poller();
                            return true;
                        }
#endif
                        l.park.notify_one();
                        return true;
                    }
                    set = _sleepers.load(std::memory_order_acquire);
                }
                return false;
            }

            // stop(): every worker woken, without a credit (the one asleep
            // in the reactor's queue through the queue)
            void _wake_all() noexcept {
                for (auto& l : _locals) {
                    l->park.store(1, std::memory_order_release);
                    l->park.notify_one();
                }
#if SGCL_REACTOR_IN_WORKERS
                if (poll_seat.enabled.load(std::memory_order_acquire)) {
                    poll_seat.wake_poller();
                }
#endif
            }

            // xorshift, a thread's own
            static uint32_t _random() noexcept {
                static thread_local uint32_t x = 2463534242u ^ (uint32_t)(uintptr_t)&x;
                x ^= x << 13;
                x ^= x >> 17;
                x ^= x << 5;
                return x;
            }

            inline static thread_local Local* _local = nullptr;
            inline static thread_local unsigned _index = 0;
            inline static thread_local bool _dispatching = false;        // in a DispatchScope
            inline static thread_local Local* _dispatch_self = nullptr;  // its worker (null on the reactor's thread)
            inline static thread_local Frame* _dispatch_keep = nullptr;  // where the first task it keeps goes (null: none kept)

            std::vector<unique_ptr<Local>> _locals;
            unique_ptr<Global> _global;
            std::mutex _lifecycle;
            std::atomic<bool> _running = {false};
            std::atomic<uint64_t> _sleepers = {0};
            std::atomic<unsigned> _spinning = {0};
            std::atomic<bool> _stop = {false};
            static constexpr int GateClosed = 0, GateOpen = 1, GateAborted = 2;
            std::atomic<int> _gate = {GateClosed};   // the workers of a start wait at it until the last is made (open) or one cannot be (aborted)
            std::vector<std::thread> _workers;
            unsigned _active = 0;          // the workers of this start: written before they are made, read by them
            std::atomic<unsigned> _pool = {0};   // the same, for other threads (the reactor's: pool())
            unsigned _workers_asked = 0;   // set_workers (under _lifecycle)
            unsigned _workers_env = 0;     // SGCL_WORKERS, or config::workers
            bool _workers_set = false;
            bool _env_read = false;
        };

        // The singleton: its destruction at exit ends the runtime first
        // (runtime_exit, below); a Scheduler of a test's own does not
        inline Scheduler& scheduler_instance() noexcept {
            struct Instance : Scheduler {
                ~Instance() {
                    runtime_exit();
                }
            };
            static Instance scheduler;
            return scheduler;
        }

        // The dead stack below the caller zeroed, SGCL_WORKER_STACK_CLEAR
        // of it at most and never below `floor` (the thread's stack begin
        // and config::stack_guard_margin): what a thread of the library
        // that runs other code does before it parks (a thread of the
        // blocking pool before it idles, the timers' thread before its
        // sleep), so that the words the frames of what it just ran left
        // there — a job and its closure, a timer and what it kept — are
        // not seen by the collector's conservative scan of the parked
        // thread, keeping their objects alive until the thread's next run
        // over that region (Scheduler::_clear_dead_stack, the workers'
        // own). The reactor's thread runs the library's code alone, whose
        // tracked words are nulled as they die: nothing seen kept there
        SGCL_NOINLINE inline void clear_dead_stack(uintptr_t floor) noexcept {
            uintptr_t here = (uintptr_t)&here;
            uintptr_t limit = here > SGCL_WORKER_STACK_CLEAR ? here - SGCL_WORKER_STACK_CLEAR : 0;
            if (limit < floor) {
                limit = floor;
            }
            detail::os::hidden_call([](void*) {}, nullptr, limit);
        }

        // The floor of clear_dead_stack for the calling thread
        SGCL_INLINE_HOT uintptr_t dead_stack_floor() noexcept {
            return detail::current_thread().stack_begin() + config::stack_guard_margin;
        }

        // The wakes of many tasks at once, gathered and handed to the
        // scheduler together (Go's injectglist and runqputbatch): a waker
        // that walks a list of waiters adds each frame it claims here
        // instead of enqueueing it, and flush() hands them all over in one
        // call (Scheduler::_enqueue_batch), where each enqueue was a look
        // at the thieves' head and a store of the tail on the line every
        // spinning worker steals from, or a node linked on the global
        // queue's contended tail from a thread that is no worker, and a
        // fence and a look at the sleepers after it. A frame whose header
        // names an executor is not gathered: it goes on the executor's
        // queue at once, as enqueue puts it. The waker does everything a
        // wake does to its waiter before the frame is added (the claim,
        // the words cleared), so a batch only moves the moment the frames
        // reach the queues: the tasks stay suspended until then, and none
        // of them can run and register again under the walk that is
        // waking it (the hang the broadcast's walk once had, broadcast.h:
        // _wake). A full batch hands its frames over and goes on
        // gathering; the destructor hands over what is left, so a waker
        // that ends in a throw loses none of the tasks it claimed. The
        // handing over may throw itself, the std::system_error of a
        // scheduler that cannot start its workers, but only once the
        // frames are on the global queue (_enqueue_starting), where they
        // wait for the next start; the destructor lets that error go. A
        // batch lives on a thread's stack, for a walk that wakes and
        // resumes nothing.
        //
        // A batch wakes one sleeper when none is looking, as one enqueue
        // does, and the workers that find its frames wake the next ones
        // as ever (Scheduler: _find_work); Go's injectglist wakes as many
        // as the batch has frames, and for tasks that run for a few
        // hundred nanoseconds that was the kernel's wakes paid for
        // nothing: sixteen subscribers woken with the workers asleep 1.6
        // us a round with one wake, 47 with sixteen, and with the workers
        // busy 7.7 us against 13.7 (measured).
        //
        // Only the broadcast's walk gathers. A walk whose every step is
        // itself the cost, a channel's close (a pop from the queue of its
        // waiters and a claim, some hundreds of nanoseconds a waiter)
        // and so a wait group's round and notify_all, lost by it: a task
        // handed over at its step starts on another worker while the walk
        // goes on, and one gathered waits for the end of the walk
        // (sixteen tasks woken by a wait group's round 12.1 to 15.3 us a
        // round from a task, 17.3 to 25.8 from a thread, notify_all 20.8
        // to 21.8; at sixty-four they gained 11, 3 and 6 per cent,
        // measured), where the broadcast's step is a compare-exchange and
        // the enqueue was three quarters of it.
        class WakeBatch {
        public:
            static constexpr unsigned Capacity = 32;

            WakeBatch() noexcept = default;
            WakeBatch(const WakeBatch&) = delete;
            WakeBatch& operator=(const WakeBatch&) = delete;

            ~WakeBatch() {
                if (_n) {
                    try {
                        _flush();
                    } catch (...) {
                    }
                }
            }

            // The frame made ready, as enqueue(frame, false) makes it (to
            // the end of the queues): gathered, or on its executor's queue
            // at once
            SGCL_INLINE_HOT void add(tracked_ptr<FrameWord> frame) {
                scheduler_instance()._gather(*this, frame);
            }

            // What was gathered, to the scheduler; what follows is
            // gathered anew
            SGCL_INLINE_HOT void flush() {
                if (_n) {
                    _flush();
                }
            }

            // The frames placed as Scheduler::DispatchScope says before
            // they are gathered (the reactor's events: each to the worker
            // that last ran it, or to the dispatching worker); the rest
            // are gathered as ever. Before the first add, in the scope
            SGCL_INLINE_HOT void affine() noexcept {
                _affine = true;
            }

        private:
            friend class Scheduler;
            using Frame = tracked_ptr<FrameWord>;

            static_assert(sizeof(Frame) == sizeof(uintptr_t));

            // The frames live in raw storage, made one by one as they
            // come: a tracked_ptr made is a check of the thread's
            // registration and a store with the barrier, and thirty-two
            // made and let go of at every walk, most of which wake nobody
            // (a broadcast's commit with every subscriber busy), cost
            // more than the batch saves on a small one
            SGCL_INLINE_HOT Frame* _frames() noexcept {
                return std::launder(reinterpret_cast<Frame*>(_storage));
            }

            SGCL_INLINE_HOT void _add(Frame& frame) {
                if (_n == Capacity) {
                    _flush();
                }
                new (&_storage[_n]) Frame(std::move(frame));
                ++_n;
            }

            void _flush() {
                struct Clear {   // the frames let go of, however the handing over ends: they are the queues' now, or lost with the memory
                    WakeBatch& batch;
                    ~Clear() {
                        auto frames = batch._frames();
                        for (unsigned i = 0; i < batch._n; ++i) {
                            frames[i].~Frame();
                        }
                        batch._n = 0;
                    }
                } clear{*this};
                scheduler_instance()._enqueue_batch(_frames(), _n);
            }

            alignas(Frame) uintptr_t _storage[Capacity];   // words, not bytes: an array of chars on the stack gets the stack protector's canary, a nanosecond at every walk (a broadcast's send, 27.1 to 28.4 ns with no subscriber waiting)
            unsigned _n = 0;
            bool _affine = false;     // affine(): placed first (Scheduler::_place_dispatched)
        };

        // A frame gathered, or on its executor's queue at once (enqueue).
        // The first of a batch wakes a sleeper when none is looking, as
        // its enqueue would have, before the rest of the walk and the
        // handing over: the wake through the kernel takes microseconds,
        // and the worker it brings finds the frames on the queues when it
        // looks, rather than being asked for only once the walk is done
        SGCL_INLINE_HOT void Scheduler::_gather(WakeBatch& batch, Frame& frame) {
            if (auto executor = frame_header(frame.get()).executor.get()) {
                executor->push(std::move(frame), false);
                return;
            }
#if SGCL_REACTOR_IN_WORKERS
            if (batch._affine && _place_dispatched(frame)) [[unlikely]] {
                return;   // its last worker's inbox, or the dispatching worker's to run: nothing to wake
            }
#endif
            if (!batch._n) {
                _wake_one_if_none_looking();
            }
            batch._add(frame);
        }

        // The strand's turn: its count says a frame is queued, so one is
        // or is about to be linked; a take that finds none met a push
        // between its exchange and its link, and the turn waits for it
        // (spinning, then yielding the core: a producer preempted there
        // needs it back to finish)
        inline void ExecutorQueue::_dispatch(bool next) {
            Frame f = take();
            if (!f) [[unlikely]] {
                detail::Backoff<32> backoff;
                for (unsigned tries = 0; !f; ++tries) {
                    if (tries < 64) {
                        backoff();
                    } else {
                        std::this_thread::yield();
                    }
                    f = take();
                }
            }
            scheduler_instance().enqueue_on_workers(std::move(f), next);
        }

        // A frame resumed on this thread (a worker, an executor's thread):
        // the thread's current frame while it runs (coroutine.h:
        // current_frame, for the task-locals of the functions it calls
        // and the tasks it starts), and, for a frame of a strand, the
        // strand told when the run ends. The strand is read before the
        // run and held through it (a tracked_ptr on this stack): the
        // header may say another executor after the run, or nothing, and
        // the coroutine may have destroyed itself
        inline void resume_frame(const tracked_ptr<FrameWord>& frame) {
            auto running = std::exchange(current_frame, frame.get());
            if (auto q = frame_header(frame.get()).executor.get(); q && q->strand) {
                tracked_ptr<ExecutorQueue> strand(q);
                handle_of(frame.get()).resume();
                current_frame = running;
                strand->run_ended();
            } else {
                handle_of(frame.get()).resume();
                current_frame = running;
            }
        }

        // What else scheduler::stop() stops, once started: the timer thread
        // (timer.h), the reactor thread (reactor.h), the blocking pool (blocking.h)
        inline std::atomic<void (*)()> scheduler_stop_hook = {nullptr};
        inline std::atomic<void (*)()> scheduler_stop_hook2 = {nullptr};
        inline std::atomic<void (*)()> scheduler_stop_hook3 = {nullptr};
        // The reactor's stop at the end of the program, which wakes none of
        // its waits; set when the reactor is made (reactor.h)
        inline std::atomic<void (*)()> reactor_exit_hook = {nullptr};

        // The first destructor of a singleton of the runtime (the others
        // find Exiting set and return): no start from here on, the starts
        // in progress waited for, and the runtime's threads joined, the
        // pool's first (its jobs may wait for tasks), then the workers
        // (their tasks for timers and I/O), the timers' thread and the
        // reactor's. Every thread these could start is started by now, its
        // hook stored before its start was counted out, and nothing starts
        // another: the set joined here is all there is. On the main thread
        // (exit), before any singleton is destroyed: each hook calls into a
        // live object
        inline void runtime_exit() {
            if (runtime_state.fetch_or(RuntimeExiting, std::memory_order_seq_cst) & RuntimeExiting) {
                return;
            }
            while (runtime_state.load(std::memory_order_acquire) >= RuntimeStartUnit) {
                std::this_thread::yield();   // a start in progress: a thread made, a lock held for a moment
            }
            if (auto hook = scheduler_stop_hook3.load(std::memory_order_acquire)) {
                hook();
            }
            scheduler_instance().stop();
            if (auto hook = scheduler_stop_hook.load(std::memory_order_acquire)) {
                hook();
            }
            if (auto hook = reactor_exit_hook.load(std::memory_order_acquire)) {
                hook();
            }
        }

        SGCL_INLINE_HOT void enqueue(tracked_ptr<FrameWord> frame, bool next) {
            scheduler_instance().enqueue(std::move(frame), next);
        }

        SGCL_INLINE_HOT void enqueue_quiet(tracked_ptr<FrameWord> frame, bool next) noexcept {
            scheduler_instance().enqueue<true>(std::move(frame), next);
        }

        SGCL_INLINE_HOT bool on_worker() noexcept {
            return Scheduler::on_worker();
        }
    }

    // The scheduler as the program sees it
    struct scheduler {
        // The number of worker threads: set_workers', else SGCL_WORKERS
        // from the environment (read once, at the first start), else
        // config::workers; 0 in any of them the hardware concurrency; at
        // most 64. The first call makes the scheduler
        SGCL_INLINE_HOT static unsigned workers() {
            return detail::scheduler_instance().workers();
        }

        // The number of workers from now on (0: the hardware concurrency),
        // over SGCL_WORKERS and config::workers. Before the first task it
        // is what the scheduler starts with; after it the workers are
        // stopped as stop() stops them, once the ready tasks have run,
        // and started again with the new number, the queues kept. Not
        // from a task (it joins the workers); a program sets it at a
        // quiet moment
        SGCL_INLINE_HOT static void set_workers(unsigned n) {
            detail::scheduler_instance().set_workers(n);
        }

        // How long a worker with nothing to run looks for work before it
        // sleeps in the kernel (a task made ready in that window costs no
        // wake), over SGCL_WORKER_SPIN_US and
        // config::worker_spin_microseconds; applies at once, workers
        // running or not
        SGCL_INLINE_HOT static void set_worker_spin(duration d) noexcept {
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::nanoseconds(d)).count();
            detail::worker_spin_set.store(true, std::memory_order_relaxed);
            detail::worker_spin_us.store(us < 0 ? 0u : unsigned(std::min<long long>(us, 0xFFFFFFFFll)), std::memory_order_relaxed);
        }

        SGCL_INLINE_HOT static duration worker_spin() noexcept {
            return std::chrono::microseconds(detail::worker_spin_us.load(std::memory_order_relaxed));
        }

        // Whether the calling thread is one of the workers
        SGCL_INLINE_HOT static bool on_worker() noexcept {
            return detail::Scheduler::on_worker();
        }

        // The queues as they are
        using statistics = detail::Scheduler::Statistics;

        SGCL_INLINE_HOT static statistics get_statistics() {
            return detail::scheduler_instance().statistics();
        }

        // Joins the workers and lets go of the queue: for a program that
        // wants its threads gone at a point of its own (the end of the
        // program does it); the next spawn starts the scheduler again
        SGCL_INLINE_HOT static void stop() {
            if (detail::runtime_exiting()) {
                return;   // the end of the program stopped everything already (runtime_exit)
            }
            if (auto hook = detail::scheduler_stop_hook.load(std::memory_order_acquire)) {
                hook();
            }
            if (auto hook = detail::scheduler_stop_hook2.load(std::memory_order_acquire)) {
                hook();
            }
            if (auto hook = detail::scheduler_stop_hook3.load(std::memory_order_acquire)) {
                hook();
            }
            detail::scheduler_instance().stop();
        }
    };

    // `co_await sgcl::async::yield()`: the task goes to the back of the queue and
    // the worker takes the next ready one
    struct [[nodiscard]] yield {
        SGCL_INLINE_HOT bool await_ready() const noexcept {
            return false;
        }

        template<class P>
        SGCL_INLINE_HOT void await_suspend(std::coroutine_handle<P> h) {
            detail::enqueue(detail::frame_of(h), false);
        }

        SGCL_INLINE_HOT void await_resume() const noexcept {
        }
    };
}
