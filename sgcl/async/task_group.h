//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "operation.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "coroutine.h"
#include "scheduler.h"
#include "stop_token.h"
#include "wait_group.h"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <exception>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // Structured concurrency, the way Go's errgroup and Kotlin's
    // coroutineScope have it: a scope that owns the tasks it spawns. A
    // group is made under a token (`task_group g(token)`: a child
    // stop_source, stopped with the caller's), `g.go(t)` starts a
    // child on the scheduler and counts it, and the wait for the group
    // (`g.wait()` on a thread, `co_await g` in a task,
    // `g.on_done(f)` as a case of a select) is the wait for every child.
    // The first child that throws requests the stop of the group, which
    // the others see through `g.token()` and leave; the wait rethrows
    // that first exception once every child has finished, so nothing of
    // the scope runs on past it and no exception is lost. The children
    // report nothing but through their own side effects (a channel, an
    // object they were given), as errgroup's do: a result that must come
    // back is when_all's business (when.h).
    //
    // The state of a group (its stop source, the count of the children
    // and the first exception) is a managed object that every child holds
    // through its frame, so the group object itself may go while
    // children run: its destructor requests the stop of the scope and
    // lets them finish on their own, their frames the collector's once
    // they are done (the way a coroutineScope cancelled by its parent
    // ends). That is the fallback, not the way to use it: a scope is
    // waited for, as errgroup's Wait is called, and a child that is
    // stopped this way still refers to whatever the caller gave it
    // (a channel on the caller's stack, say), which must outlive it.
    //
    // What a spawn costs: the child's frame and one more, the runner's,
    // a small task that awaits the child, records its exception and
    // counts it off; the runner is run on the calling thread to its
    // first suspension (which puts the child on the scheduler) and let
    // go of, so that the child's finish resumes the runner as its
    // continuation: two frames and two turns of the scheduler per child,
    // as `co_await` of a spawned task costs.
    namespace detail {
        struct GroupState {
            explicit GroupState(const stop_token& parent)
            : source(parent) {
            }

            stop_source source;
            wait_group running;
            std::atomic<bool> failed = {false};
            std::exception_ptr error;   // the first exception, written by the child that claimed `failed`, read after the wait

            // The child's exception, the first one kept, and the stop of
            // the scope requested
            void fail(std::exception_ptr e) {
                bool claimed = false;
                if (failed.compare_exchange_strong(claimed, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    error = std::move(e);
                }
                source.request_stop();
            }

            void rethrow() {
                if (failed.load(std::memory_order_acquire) && error) {
                    std::rethrow_exception(error);
                }
            }
        };

        // A child in its group: awaited, its exception recorded, counted
        // off. Nothing of the state is touched past the count: the wait
        // may return and the group go the moment it reaches zero
        template<class T>
        task<> run_in_group(task<T> t, tracked_ptr<GroupState> state) {
            try {
                co_await t;
            } catch (...) {
                state->fail(std::current_exception());
            }
            state->running.done();
        }
    }

    class task_group {
    public:
        // A scope under the token: its own source is a child of the
        // token's, stopped with it (and at once when it is stopped
        // already); an empty token, the default, is a scope on its own
        explicit task_group(const stop_token& parent = stop_token())
        : _s(make_tracked<detail::GroupState>(parent)) {
        }

        task_group(const task_group&) = delete;
        task_group& operator=(const task_group&) = delete;

        // The scope ends: children still running are told to stop and
        // let go of (see above); a group waited for has none
        ~task_group() {
            if (_s->running.count() > 0) {
                _s->source.request_stop();
            }
        }

        // A child: started on the scheduler and counted, with no handle
        // given back, as sgcl::async::go starts a task (spawn is the name
        // of the forms that hand one back); a task nobody spawned, or one
        // spawned already. Its result, if it has one, is dropped; its
        // exception is the group's
        template<class T>
        void go(task<T> t) {
            task<> runner = detail::run_in_group(std::move(t), _s);   // made before the count: a frame that cannot be made (bad_alloc) leaves the count as it was
            _s->running.add();
            runner.resume();   // to its first suspension: the child put on the scheduler, the runner its continuation; the runner takes this task's header first (task::resume), so the child inherits the executor and the task-locals
            runner.detach();
        }

        // The same for a coroutine function with captures (sgcl::async::go)
        template<detail::TaskFactory F>
        void go(F f) {
            go(detail::task_of(std::move(f)));
        }

        // The token the children are given: stopped by the first
        // exception, by request_stop(), by the parent, or by the
        // group's end
        stop_token token() const noexcept {
            return _s->source.token();
        }

        // The stop of the whole scope, by hand
        void request_stop() {
            _s->source.request_stop();
        }

        bool stop_requested() const noexcept {
            return _s->source.stop_requested();
        }

        // The children not yet finished
        size_t count() const noexcept {
            return (size_t)_s->running.count();
        }

        // Waits for every child, then rethrows the first exception a child
        // threw, if any: `g.wait()` on a thread (not from a task on a
        // worker, which it would block), `co_await g` in a task, as a task
        // is waited for
        void wait() {
            assert(!detail::on_worker() && "wait() blocks the worker: co_await the group from a task");
            _s->running.wait();
            _s->rethrow();
        }

        auto operator co_await() {
            return detail::either([this] { return _co_wait(); }, [this] { wait(); });
        }

        // A case of a select: f() when every child has finished; the
        // exception, if any, is rethrown by the wait that follows
        // (`g.wait()` returns at once then)
        template<class F>
        auto on_done(F f) {
            return _s->running.on_done(std::move(f));
        }

    private:
        tracked_ptr<detail::GroupState> _s;

        // the two halves of the operations above: a thread's and a task's
        task<> _co_wait() {
            tracked_ptr<detail::GroupState> s = _s;   // the frame's own hold: the state outlives the group object
            co_await s->running;
            s->rethrow();
        }
    };
}
