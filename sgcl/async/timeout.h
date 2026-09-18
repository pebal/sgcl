//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "channel.h"
#include "coroutine.h"
#include "scheduler.h"
#include "select.h"
#include "stop_token.h"
#include "timer.h"

#include <exception>
#include <stdexcept>
#include <utility>

namespace sgcl {
    // A timeout on a task, what the `timeout(d, f)` case is for a select
    // (timer.h) and Go's `select { case r := <-done: case <-time.After(d): }`
    // is: `co_await timeout(t, d)` is the task's result as an optional,
    // nullopt when d passed first (true or false for a task of nothing);
    // `co_await with_deadline(t, d)` is the result itself, or the
    // exception timed_out when d passed first; `with_deadline(t, token)`
    // the same with a token's stop as the deadline (a token whose source
    // was given a stop_after, or is stopped by hand). Each is a task,
    // waited for as one: awaited, joined, spawned or not.
    //
    // A race against a duration is one object and no frame of its own:
    // the raced task is held by it and given a continuation that is a
    // call into it (coroutine.h: Continuation), the deadline is a timer
    // that calls into it (timer.h: fire), and the first call wins with a
    // compare-exchange on the race's word and hands the timeout task's
    // frame to the scheduler; the second finds the word taken and does
    // nothing. A task that won cancels the timer (swept out of the heap
    // with the closed ones); a deadline that won leaves the task to run
    // on, its end calling into a race that is over. It was a task
    // awaited by a runner task that finished into a slot, and a race
    // task selecting between the slot's channel and the timer's, three
    // frames, a select and six hops per race (benchmarks: 1870 ns per
    // race then, 510 now). A race against a token keeps that shape: a token's
    // stop is a channel closed, which a select waits for, and there is
    // nothing to arm or cancel. A task that lost the race is not stopped
    // by the timeout on its own (a task cannot be stopped from outside:
    // it stops itself when it sees its token): it runs on to its end,
    // its result lands where nobody reads it any more, and the objects
    // and the frames are the collector's. To have the loser stop, give it a token whose source
    // the timeout may stop: `timeout(t, d, source)` requests the stop of
    // the source when d passes first, so a task made with
    // `source.token()` sees it (`stop_source src(parent); co_await
    // timeout(work(src.token()), 1s, src);`). A result that came at the
    // same instant as the deadline is a result: the slot is looked at
    // whichever case the select served.
    class timed_out : public std::runtime_error {
    public:
        timed_out()
        : std::runtime_error("timed out") {
        }
    };

    namespace detail {
        // Where the raced task finishes into: the result or the
        // exception, and the channel closed then
        template<class T>
        struct TimeoutSlot {
            channel<void> done;
            optional<T> value;
            std::exception_ptr error;

            T take() {
                if (error) {
                    std::rethrow_exception(error);
                }
                return std::move(*value);
            }

            // A task done before the race: its result or exception taken now
            void finish(task<T>& t) {
                try {
                    value.emplace(std::move(t.result()));
                } catch (...) {
                    error = std::current_exception();
                }
                done.close();
            }
        };

        template<>
        struct TimeoutSlot<void> {
            channel<void> done;
            std::exception_ptr error;

            void take() {
                if (error) {
                    std::rethrow_exception(error);
                }
            }

            void finish(task<>& t) {
                try {
                    t.result();
                } catch (...) {
                    error = std::current_exception();
                }
                done.close();
            }
        };

        template<class T>
        task<> finish_race(task<T> t, tracked_ptr<TimeoutSlot<T>> slot) {
            try {
                slot->value.emplace(co_await t);
            } catch (...) {
                slot->error = std::current_exception();
            }
            slot->done.close();
        }

        inline task<> finish_race(task<> t, tracked_ptr<TimeoutSlot<void>> slot) {
            try {
                co_await t;
            } catch (...) {
                slot->error = std::current_exception();
            }
            slot->done.close();
        }

        // The race against a token: the task started (unless it was), and
        // the select between its finish and the token's channel; true when
        // the task is done, whichever case was served first. A task done
        // before the race has its result taken at once (a zero timeout
        // on a finished task is a result, not a deadline: the runner
        // would reach the slot after the timer). The channel is held by
        // the caller's frame for the length of the race
        template<class T>
        task<bool> race(tracked_ptr<TimeoutSlot<T>> slot, task<T> t, channel<void>& deadline) {
            if (t.done()) {
                slot->finish(t);
                co_return true;
            }
            go(finish_race(std::move(t), slot));
            co_await async_select(slot->done.on_receive([] {}), deadline.on_receive([] {}));
            co_return slot->done.closed();
        }

        // The race against a duration: the task, the timeout task's frame
        // to resume, the timer, and the word the two calls race on
        template<class T>
        struct TimeoutRace : Continuation {
            enum : int { Racing, TaskWon, DeadlineWon };

            explicit TimeoutRace(task<T> t) noexcept
            : Continuation{&on_done}
            , t(std::move(t)) {
            }

            task<T> t;
            tracked_ptr<FrameWord> waiter;
            tracked_ptr<Timer> timer;
            atomic<int> state = {Racing};

            static void on_done(Continuation* c) noexcept {
                static_cast<TimeoutRace*>(c)->_finish(TaskWon, true);
            }

            static void on_time(void* r) noexcept {
                static_cast<TimeoutRace*>(r)->_finish(DeadlineWon, false);
            }

            // The first of the two: the timer cancelled when the task won,
            // the frame handed to the scheduler (next on this worker after
            // the task's end, in the queue from the timer's thread)
            void _finish(int who, bool next) noexcept {
                int e = Racing;
                if (!state.compare_exchange_strong(e, who, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return;
                }
                if (who == TaskWon && timer) {
                    timer->cancelled.store(true, std::memory_order_release);
                    timer_cancelled();
                }
                enqueue(waiter, next);
                waiter = nullptr;
            }

            // The awaiter of the race, in the timeout task: the timer armed
            // (with the race kept by it), then the continuation installed;
            // a task done already takes the race back at once, unless the
            // timer got there first, in which case the frame is already on
            // its way to the scheduler and the wait goes on. Nothing of the
            // frame is touched once the continuation is in: the task's end
            // may resume it before this returns.
            struct awaiter {
                tracked_ptr<TimeoutRace> race;
                duration d;

                bool await_ready() const noexcept {
                    return race->t.done();
                }

                template<class P>
                bool await_suspend(std::coroutine_handle<P> h) {
                    tracked_ptr<TimeoutRace> r = race;   // this awaiter lives in the frame: a copy of its own
                    auto frame = frame_of(h);
                    r->waiter = frame;
                    r->t._start(frame_header(frame.get()).executor);
                    r->timer = add_timer(d, r, &on_time);
                    if (r->t._frame.promise().await(r.get(), r)) {
                        return true;
                    }
                    int e = Racing;   // done already: the race is the task's, unless the deadline of zero fired meanwhile
                    if (r->state.compare_exchange_strong(e, TaskWon, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        r->timer->cancelled.store(true, std::memory_order_release);
                        timer_cancelled();
                        r->waiter = nullptr;
                        return false;
                    }
                    return true;
                }

                bool await_resume() const noexcept {   // whether the task is done: a result that came with the deadline is a result
                    return race->t.done();
                }
            };
        };

        // What timeout gives: the result as an optional, or a bool for a task of nothing
        template<class T>
        struct TimeoutResult {
            using type = optional<T>;

            static type some(TimeoutSlot<T>& slot) {
                return type(std::in_place, slot.take());
            }

            static type some(task<T>& t) {
                return type(std::in_place, std::move(t.result()));
            }

            static T take(task<T>& t) {   // the result itself, for with_deadline
                return std::move(t.result());
            }

            static type none() noexcept {
                return nullopt;
            }
        };

        template<>
        struct TimeoutResult<void> {
            using type = bool;

            static type some(TimeoutSlot<void>& slot) {
                slot.take();
                return true;
            }

            static type some(task<>& t) {
                t.result();
                return true;
            }

            static void take(task<>& t) {
                t.result();
            }

            static type none() noexcept {
                return false;
            }
        };
    }

    // `co_await timeout(t, d)`: the result of t, or nullopt when d passed
    // first (true or false for a task<>); the loser runs on, unseen
    template<class T>
    task<typename detail::TimeoutResult<T>::type> timeout(task<T> t, duration d) {
        tracked_ptr<detail::TimeoutRace<T>> race = make_tracked<detail::TimeoutRace<T>>(std::move(t));
        if (co_await typename detail::TimeoutRace<T>::awaiter{race, d}) {
            co_return detail::TimeoutResult<T>::some(race->t);
        }
        co_return detail::TimeoutResult<T>::none();
    }

    // The same, with the stop of the source requested when d passed
    // first: for a task made with the source's token, which stops itself
    template<class T>
    task<typename detail::TimeoutResult<T>::type> timeout(task<T> t, duration d, stop_source loser) {
        tracked_ptr<detail::TimeoutRace<T>> race = make_tracked<detail::TimeoutRace<T>>(std::move(t));
        if (co_await typename detail::TimeoutRace<T>::awaiter{race, d}) {
            co_return detail::TimeoutResult<T>::some(race->t);
        }
        loser.request_stop();
        co_return detail::TimeoutResult<T>::none();
    }

    // `co_await with_deadline(t, d)`: the result of t, or timed_out
    // thrown when d passed first
    template<class T>
    task<T> with_deadline(task<T> t, duration d) {
        tracked_ptr<detail::TimeoutRace<T>> race = make_tracked<detail::TimeoutRace<T>>(std::move(t));
        if (!co_await typename detail::TimeoutRace<T>::awaiter{race, d}) {
            throw timed_out();
        }
        co_return detail::TimeoutResult<T>::take(race->t);
    }

    template<class T>
    task<T> with_deadline(task<T> t, duration d, stop_source loser) {
        tracked_ptr<detail::TimeoutRace<T>> race = make_tracked<detail::TimeoutRace<T>>(std::move(t));
        if (!co_await typename detail::TimeoutRace<T>::awaiter{race, d}) {
            loser.request_stop();
            throw timed_out();
        }
        co_return detail::TimeoutResult<T>::take(race->t);
    }

    // `co_await with_deadline(t, token)`: the result of t, or timed_out
    // thrown when the token was stopped first (a deadline given to its
    // source with stop_after, or a stop by hand); a task made with the
    // same token stops itself
    template<class T>
    task<T> with_deadline(task<T> t, stop_token token) {
        if (!token.stop_possible()) {   // an empty token: no deadline
            co_return co_await t;
        }
        tracked_ptr<detail::TimeoutSlot<T>> slot = make_tracked<detail::TimeoutSlot<T>>();
        if (!co_await detail::race(slot, std::move(t), token.channel())) {
            throw timed_out();
        }
        co_return slot->take();
    }
}
