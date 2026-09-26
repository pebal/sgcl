//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../concurrent/queue.h"
#include "../core/detail/backoff.h"
#include "../core/collector.h"
#include "../core/config.h"
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
#include <thread>
#include <utility>
#include <vector>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    namespace detail {
        class Scheduler;
        class WakeBatch;
        inline Scheduler& scheduler_instance();
        void resume_frame(const tracked_ptr<FrameWord>& frame);

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

            explicit ExecutorQueue(bool strand)
            : _stub(_make_stub())
            , _head(_stub)
            , strand(strand)
            , _tail(_stub.get()) {
            }

            // A frame made ready on this executor
            void push(Frame frame, bool next) {
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
            void stop() {
                stopping.store(true, std::memory_order_seq_cst);
                _wake();
            }

            // A strand: the run of one of its frames ended; the next one
            // to the workers, if any is queued
            void run_ended() {
                if (pending.fetch_sub(1, std::memory_order_acq_rel) > 1) {
                    _dispatch(false);
                }
            }

            // An executor's thread: the next frame, spinning a while on
            // an empty queue and parking then; null when a stop() came, or
            // for a wake that found nothing (the caller asks again)
            Frame pop() {
                auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(config::worker_spin_microseconds);
                detail::Backoff<32> backoff;
                do {
                    if (auto f = take()) {
                        return f;
                    }
                    if (stopping.load(std::memory_order_acquire)) {
                        return Frame();
                    }
                    backoff();
                } while (std::chrono::steady_clock::now() < until);
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
            Frame take() {
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
            // line apart (concurrent::queue: padding, not alignas)
            unsigned char _pad[config::cache_line_size - 2 * sizeof(Frame) - sizeof(uint32_t) - 3 * sizeof(bool)] = {};
            std::atomic<FrameWord*> _tail;         // the producers': the last frame, or the stub (raw: whatever it names is held otherwise)
            std::atomic<uint32_t> _parked = {0};   // an executor: its thread asleep in pop(), or about to be

        public:
            std::atomic<uint32_t> pushes = {0};    // an executor: the pushes so far; with `taken`, what a poll has to run
            std::atomic<size_t> pending = {0};     // a strand: the frames queued, plus the one running

        private:
            static FrameLink& _link(FrameWord* frame) noexcept {
                return frame_header(frame).next;
            }

            // A frame of the queue's own: a header and nothing else
            static Frame _make_stub() {
                auto words = Maker<FrameWord[]>::make_tracked_data(FrameHeaderWords).release();
                return unique_ptr<FrameWord>(UniquePtr<FrameWord>(words));
            }

            // The frame at the tail. The exchange is seq_cst, as is the
            // park's look at the tail after its store to `_parked`: one of
            // the two sees the other (_wake)
            void _push(FrameWord* frame) noexcept {
                _link(frame) = nullptr;
                auto prev = _tail.exchange(frame, std::memory_order_seq_cst);
                _link(prev).store(frame, std::memory_order_release);
            }

            // Nothing queued, and no push under way: the stub alone, at
            // both ends
            bool _empty() const noexcept {
                auto stub = _stub.get();
                return _head.get() == stub && _tail.load(std::memory_order_seq_cst) == stub;
            }

            // The executor's thread asleep until a push or a stop: counted
            // as parked before its last look at the queue and at the stop,
            // so that a push or a stop after that look sees it (_wake).
            // A queue that is not empty but has nothing to take has a push
            // under way: the thread yields and looks again
            void _park() {
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
            void _wake() noexcept {
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
        // queue. A ring keeps the words of the entries taken until they
        // are overwritten, so a finished task's frame may stay allocated
        // until its slot is reused.
        //
        // One scheduler per process, started on the first enqueue,
        // stopped when the program ends or by scheduler::stop(): the
        // workers are joined then, so a task that never suspends never
        // lets the program end, as a thread would not; the next enqueue
        // starts it again.
        class Scheduler {
        public:
            using Frame = tracked_ptr<FrameWord>;

            // A worker's queues, a managed object: the ring, its head
            // (thieves and the owner, a compare-exchange) and tail (the
            // owner, a store), and the next slot (the owner alone)
            struct Local {
                static constexpr uint32_t Size = 256;

                std::atomic<uint32_t> head = {0};
                std::atomic<uint32_t> tail = {0};
                std::atomic<uint32_t> park = {0};   // the worker's own wake word: 1 = woken with the credit of a looking worker
                uintptr_t stack_floor = 0;          // the lowest address the worker's stack may be cleared to (_clear_dead_stack)
                Frame next;
                Frame slots[Size];
            };

            static constexpr unsigned MaxWorkers = 64;   // the bits of the set of sleepers
#ifndef SGCL_WORKER_STACK_CLEAR
#define SGCL_WORKER_STACK_CLEAR 4096
#endif
            static constexpr size_t StackClearOnIdle = SGCL_WORKER_STACK_CLEAR;   // the dead stack a worker clears before looking for work: the frames of a task's last run, with what it called (a page; the tests clear 64 KB before their checks, config::stack_clear_size)

            struct Global {
                concurrent::queue<Frame> ready;
            };

            ~Scheduler() {
                stop();
            }

            // The workers joined and the queues let go of; what was on
            // them stays suspended, and is run when the scheduler starts
            // again only if it is made ready again (a program stops the
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
            void enqueue(Frame frame, bool next) {
                if (auto executor = frame_header(frame.get()).executor.get()) {
                    executor->push(std::move(frame), next);
                    return;
                }
                enqueue_on_workers(std::move(frame), next);
            }

            // The pool's own queues: what a strand hands its frames to
            void enqueue_on_workers(Frame frame, bool next) {
                if (!_running.load(std::memory_order_acquire)) [[unlikely]] {
                    _start();
                }
                if (auto local = _local) {
                    if (next) {
                        if (local->next) {
                            _push_local(*local, local->next);   // the one waiting to run next goes to the end
                        }
                        local->next = std::move(frame);
                    } else {
                        _push_local(*local, std::move(frame));
                    }
                } else {
                    _global->ready.push(std::move(frame));
                }
                _wake_one_if_none_looking();
            }

            static bool on_worker() noexcept {
                return _local != nullptr;
            }

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
            void dump(FILE* out) {
                std::fprintf(out, "spinning %u sleepers %llx global empty %d stop %d\n", _spinning.load(), (unsigned long long)_sleepers.load(), (int)_global->ready.empty(), (int)_stop.load());
                for (unsigned i = 0; i < _locals.size(); ++i) {
                    auto& l = *_locals[i];
                    std::fprintf(out, "  w%u h %u t %u next %p park %u\n", i, l.head.load(), l.tail.load(), (void*)l.next.get(), l.park.load());
                }
            }

            unsigned workers() {
                if (!_running.load(std::memory_order_acquire)) [[unlikely]] {
                    _start();
                }
                return (unsigned)_workers.size();
            }

        private:
            friend class WakeBatch;

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
                    _start();
                }
                if (auto local = _local) {
                    auto t = local->tail.load(std::memory_order_relaxed);
                    auto k = std::min<uint32_t>(n, Local::Size - (t - local->head.load(std::memory_order_acquire)));
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

            void _start() {
                std::lock_guard lock(_lifecycle);
                if (_running.load(std::memory_order_acquire)) {
                    return;
                }
                _sleepers.store(0, std::memory_order_relaxed);
                _spinning.store(0, std::memory_order_relaxed);
                auto n = std::min(MaxWorkers, config::workers ? config::workers : std::max(1u, std::thread::hardware_concurrency()));
                if (!_global) {   // the queues are made once and kept across stops (stop): what was pushed while the workers were away runs now
                    _global = make_tracked<Global>();
                    _locals.reserve(n);
                    for (unsigned i = 0; i < n; ++i) {
                        _locals.push_back(make_tracked<Local>());
                    }
                } else {
                    for (auto& l : _locals) {
                        l->park.store(0, std::memory_order_relaxed);   // the wake of the stop, taken by nobody: a worker starting with it would count itself woken and leave its bit among the sleepers
                    }
                }
                _stop.store(false, std::memory_order_release);
                _workers.reserve(n);
                for (unsigned i = 0; i < n; ++i) {
                    _workers.emplace_back([this, i] { _run(i); });
                }
                _running.store(true, std::memory_order_release);
            }

            static void _resume(const Frame& frame) {
                resume_frame(frame);   // the frame held by the caller's local while the coroutine runs (by reference down from there: a tracked_ptr copied is a barrier and a registration check each)
            }

            void _run(unsigned index) {
                _local = _locals[index].get();
                _index = index;
                Local& local = *_local;
                local.stack_floor = detail::current_thread().stack_begin() + config::stack_guard_margin;
                uint32_t tick = 0;
                for (;;) {
                    if (local.next) {
                        Frame f = local.next;   // a move of a tracked_ptr is a copy: taken, then cleared
                        local.next = nullptr;
                        _resume(std::move(f));
                        continue;
                    }
                    // the global queue every so often, so that its tasks are
                    // not starved by a busy ring
                    if (++tick % 61 == 0) {
                        if (auto f = _global->ready.try_pop()) {
                            _resume(std::move(*f));
                            continue;
                        }
                    }
                    if (auto f = _pop_local(local)) {
                        _resume(std::move(f));
                        continue;
                    }
                    if (auto f = _find_work(local)) {
                        _resume(std::move(f));
                        continue;
                    }
                    if (_stop.load(std::memory_order_acquire)) {
                        return;
                    }
                    if (_sleep(local)) {
                        // woken with the credit of a looking worker: this
                        // one is looking now, and the credit is its own
                        if (auto f = _find_work(local, true)) {
                            _resume(std::move(f));
                        }
                    }
                }
            }

            // The owner's push, at the tail; a full ring spills to the
            // global queue
            void _push_local(Local& local, Frame frame) {
                auto t = local.tail.load(std::memory_order_relaxed);
                auto h = local.head.load(std::memory_order_acquire);
                if (t - h < Local::Size) {
                    local.slots[t % Local::Size] = std::move(frame);
                    local.tail.store(t + 1, std::memory_order_release);
                } else {
                    _global->ready.push(std::move(frame));
                }
            }

            // The owner's pop, at the head, against the thieves
            static Frame _pop_local(Local& local) {
                for (;;) {
                    auto h = local.head.load(std::memory_order_acquire);
                    auto t = local.tail.load(std::memory_order_relaxed);
                    if (h == t) {
                        return Frame();
                    }
                    Frame f = local.slots[h % Local::Size];
                    if (local.head.compare_exchange_strong(h, h + 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        return f;
                    }
                }
            }

            // Half of a victim's ring taken: the first entry returned, the
            // rest moved to this worker's ring. The entries are read, then
            // the victim's head exchanged past them; a failed exchange
            // leaves the copies in this ring's free slots, which its tail
            // never reached, to be overwritten
            Frame _steal(Local& victim, Local& mine) {
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
            // someone is still looking while work keeps coming
            Frame _find_work(Local& mine, bool credited = false) {
                // The dead part of this worker's stack cleared first: the
                // frames of the task that just ran are below here, their
                // words the collector's conservative scan still sees, and
                // an object referenced only there would live until the
                // worker's next run over that region (a test saw a value
                // outlive its broadcast by a whole idle worker, under TSan
                // half the time). A few kilobytes, once per run out of
                // work, not per hop: a wake into the next slot never comes
                // through here.
                _clear_dead_stack(mine);
                if (!credited) {
                    _spinning.fetch_add(1, std::memory_order_acq_rel);
                }
                Frame found;
                auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(config::worker_spin_microseconds);
                detail::Backoff<32> backoff;
                do {
                    if (auto f = _global->ready.try_pop()) {
                        found = std::move(*f);
                        break;
                    }
                    auto n = (unsigned)_locals.size();
                    auto start = _random() % n;
                    for (unsigned k = 0; k < n && !found; ++k) {
                        auto v = (start + k) % n;
                        if (v != _index) {
                            found = _steal(*_locals[v], mine);
                        }
                    }
                    if (found || _stop.load(std::memory_order_acquire)) {
                        break;
                    }
                    backoff();
                } while (std::chrono::steady_clock::now() < until);
                if (_spinning.fetch_sub(1, std::memory_order_acq_rel) == 1 && found) {
                    _wake_one_if_none_looking();
                }
                return found;
            }

            // The sleep: this worker's bit set in the set of sleepers
            // before its last look (against the enqueue that pushes and
            // then looks at the set: one of the two sees the other), then
            // the wait on its own word. True when woken by a wake (with
            // the credit), false when it found work itself or the
            // scheduler stops
            bool _sleep(Local& local) {
                auto bit = uint64_t(1) << _index;
                _sleepers.fetch_or(bit, std::memory_order_acq_rel);
                std::atomic_thread_fence(std::memory_order_seq_cst);
                if (!_global->ready.empty() || _has_work(local) || _stop.load(std::memory_order_acquire)) {
                    if (_sleepers.fetch_and(~bit, std::memory_order_acq_rel) & bit) {
                        return false;   // the bit was still ours: nobody woke us
                    }
                    while (local.park.load(std::memory_order_acquire) == 0) {   // a wake is on its way: take its credit
                    }
                    local.park.store(0, std::memory_order_relaxed);
                    return true;
                }
                while (local.park.load(std::memory_order_acquire) == 0) {
                    local.park.wait(0, std::memory_order_acquire);
                }
                local.park.store(0, std::memory_order_relaxed);
                return _spinning_credit_taken();
            }

            // A wake by stop() carries no credit; one by an enqueue does
            bool _spinning_credit_taken() const noexcept {
                return !_stop.load(std::memory_order_acquire);
            }

            static bool _has_work(const Local& local) noexcept {
                return local.head.load(std::memory_order_acquire) != local.tail.load(std::memory_order_acquire);
            }

            // One sleeper woken, with the credit of a looking worker, when
            // none is looking: its bit taken out of the set (exactly one
            // waker gets it), the credit counted, its word set
            void _wake_one_if_none_looking() {
                std::atomic_thread_fence(std::memory_order_seq_cst);
                if (_spinning.load(std::memory_order_relaxed) != 0) {
                    return;
                }
                auto set = _sleepers.load(std::memory_order_acquire);
                while (set) {
                    auto i = (unsigned)std::countr_zero(set);
                    auto bit = uint64_t(1) << i;
                    if (_sleepers.fetch_and(~bit, std::memory_order_acq_rel) & bit) {
                        _spinning.fetch_add(1, std::memory_order_acq_rel);
                        auto& park = _locals[i]->park;
                        park.store(1, std::memory_order_release);
                        park.notify_one();
                        return;
                    }
                    set = _sleepers.load(std::memory_order_acquire);
                }
            }

            // stop(): every worker woken, without a credit
            void _wake_all() {
                for (auto& l : _locals) {
                    l->park.store(1, std::memory_order_release);
                    l->park.notify_one();
                }
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

            std::vector<unique_ptr<Local>> _locals;
            unique_ptr<Global> _global;
            std::mutex _lifecycle;
            std::atomic<bool> _running = {false};
            std::atomic<uint64_t> _sleepers = {0};
            std::atomic<unsigned> _spinning = {0};
            std::atomic<bool> _stop = {false};
            std::vector<std::thread> _workers;
        };

        inline Scheduler& scheduler_instance() {
            static Scheduler scheduler;
            return scheduler;
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
        // that ends in a throw loses none of the tasks it claimed, unless
        // the handing over itself throws (the memory gone), which the
        // destructor swallows. A batch lives on a thread's stack, for a
        // walk that wakes and resumes nothing.
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
            void add(tracked_ptr<FrameWord> frame) {
                scheduler_instance()._gather(*this, frame);
            }

            // What was gathered, to the scheduler; what follows is
            // gathered anew
            void flush() {
                if (_n) {
                    _flush();
                }
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
            Frame* _frames() noexcept {
                return std::launder(reinterpret_cast<Frame*>(_storage));
            }

            void _add(Frame& frame) {
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
        };

        // A frame gathered, or on its executor's queue at once (enqueue).
        // The first of a batch wakes a sleeper when none is looking, as
        // its enqueue would have, before the rest of the walk and the
        // handing over: the wake through the kernel takes microseconds,
        // and the worker it brings finds the frames on the queues when it
        // looks, rather than being asked for only once the walk is done
        inline void Scheduler::_gather(WakeBatch& batch, Frame& frame) {
            if (auto executor = frame_header(frame.get()).executor.get()) {
                executor->push(std::move(frame), false);
                return;
            }
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

        inline void enqueue(tracked_ptr<FrameWord> frame, bool next) {
            scheduler_instance().enqueue(std::move(frame), next);
        }

        inline bool on_worker() noexcept {
            return Scheduler::on_worker();
        }
    }

    // The scheduler as the program sees it
    struct scheduler {
        // The number of worker threads (config::workers, or the hardware
        // concurrency); the first call makes the scheduler
        static unsigned workers() {
            return detail::scheduler_instance().workers();
        }

        // Whether the calling thread is one of the workers
        static bool on_worker() noexcept {
            return detail::Scheduler::on_worker();
        }

        // The queues as they are
        using statistics = detail::Scheduler::Statistics;

        static statistics get_statistics() {
            return detail::scheduler_instance().statistics();
        }

        // Joins the workers and lets go of the queue: for a program that
        // wants its threads gone at a point of its own (the end of the
        // program does it); the next spawn starts the scheduler again
        static void stop() {
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
        bool await_ready() const noexcept {
            return false;
        }

        template<class P>
        void await_suspend(std::coroutine_handle<P> h) {
            detail::enqueue(detail::frame_of(h), false);
        }

        void await_resume() const noexcept {
        }
    };
}
