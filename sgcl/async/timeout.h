//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "channel.h"
#include "coroutine.h"
#include "scheduler.h"
#include "select.h"
#include "stop_token.h"
#include "timer.h"

#include <exception>
#include <type_traits>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // A timeout on a task, what the `timeout(d, f)` case is for a select
    // (timer.h) and Go's `select { case r := <-done: case <-time.After(d): }`
    // is: `co_await with_timeout(t, d)` is the task's result, or the error
    // timed_out when d passed first; `with_deadline(t, when)` the same by a
    // point of the module's clock (Go's context.WithTimeout and
    // WithDeadline); `with_deadline(t, token)` the result, or the error
    // stopped when the token's stop came first (a source given a
    // stop_after or a stop_at, or stopped by hand: the library cannot
    // tell which). A failure of the library is an expected (DESIGN 220);
    // what the task itself threw comes through as it is. Each is a task,
    // waited for as one: awaited, waited for, spawned or not.
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
    // the timeout may stop: `with_timeout(t, d, source)` requests the
    // stop of the source when d passes first, so a task made with
    // `source.token()` sees it (`stop_source src(parent); co_await
    // with_timeout(work(src.token()), 1s, src);`). A result that came at the
    // same instant as the deadline is a result: the slot is looked at
    // whichever case the select served.
    // The errors of the races: the time passed first, or the token's stop
    // came first
    class timed_out {
    public:
        string message() const {
            return "timed out";
        }

        friend bool operator==(const timed_out&, const timed_out&) noexcept = default;
    };

    class stopped {
    public:
        string message() const {
            return "stopped";
        }

        friend bool operator==(const stopped&, const stopped&) noexcept = default;
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
            co_await select(slot->done.on_receive([] {}), deadline.on_receive([] {}));
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
                time_point when;   // the deadline: a point of the module's clock

                bool await_ready() const noexcept {
                    return race->t.done();
                }

                template<class P>
                bool await_suspend(std::coroutine_handle<P> h) {
                    tracked_ptr<TimeoutRace> r = race;   // this awaiter lives in the frame: a copy of its own
                    auto frame = frame_of(h);
                    r->waiter = frame;
                    r->t._start(frame_header(frame.get()).executor);
                    r->timer = add_timer(when, r, &on_time);
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

        // The result of a race won by the task, as the expected it gives:
        // the value (nothing for a task of nothing), or what the task threw
        template<class E, class T>
        expected<T, E> race_won(task<T>& t) {
            if constexpr (std::is_void_v<T>) {
                t.result();
                return expected<void, E>();
            } else {
                return std::move(t.result());
            }
        }

        template<class E, class T>
        expected<T, E> race_won(TimeoutSlot<T>& slot) {
            if constexpr (std::is_void_v<T>) {
                slot.take();
                return expected<void, E>();
            } else {
                return slot.take();
            }
        }
    }

    // `co_await with_deadline(t, when)`: the result of t, or timed_out
    // when the point of the module's clock came first (Go's
    // context.WithDeadline); with_timeout gives the time instead of the
    // point (context.WithTimeout)
    template<class T>
    task<expected<T, timed_out>> with_deadline(task<T> t, time_point when) {
        tracked_ptr<detail::TimeoutRace<T>> race = make_tracked<detail::TimeoutRace<T>>(std::move(t));
        if (!co_await typename detail::TimeoutRace<T>::awaiter{race, when}) {
            co_return unexpected(timed_out());
        }
        co_return detail::race_won<timed_out>(race->t);
    }

    // The same, with the stop of the source requested when the point came
    // first: for a task made with the source's token, which stops itself
    template<class T>
    task<expected<T, timed_out>> with_deadline(task<T> t, time_point when, stop_source loser) {
        tracked_ptr<detail::TimeoutRace<T>> race = make_tracked<detail::TimeoutRace<T>>(std::move(t));
        if (!co_await typename detail::TimeoutRace<T>::awaiter{race, when}) {
            loser.request_stop();
            co_return unexpected(timed_out());
        }
        co_return detail::race_won<timed_out>(race->t);
    }

    // `co_await with_timeout(t, d)`: the result of t, or timed_out when d
    // passed first
    template<class T>
    task<expected<T, timed_out>> with_timeout(task<T> t, duration d) {
        return with_deadline(std::move(t), clock::now() + d);
    }

    template<class T>
    task<expected<T, timed_out>> with_timeout(task<T> t, duration d, stop_source loser) {
        return with_deadline(std::move(t), clock::now() + d, std::move(loser));
    }

    // `co_await with_deadline(t, token)`: the result of t, or stopped when
    // the token was stopped first (a deadline given to its source with
    // stop_after or stop_at, or a stop by hand); a task made with the same
    // token stops itself
    template<class T>
    task<expected<T, stopped>> with_deadline(task<T> t, stop_token token) {
        if (!token.stop_possible()) {   // an empty token: no deadline
            if constexpr (std::is_void_v<T>) {
                co_await t;
                co_return expected<void, stopped>();
            } else {
                co_return co_await t;
            }
        }
        tracked_ptr<detail::TimeoutSlot<T>> slot = make_tracked<detail::TimeoutSlot<T>>();
        if (!co_await detail::race(slot, std::move(t), token.channel())) {
            co_return unexpected(stopped());
        }
        co_return detail::race_won<stopped>(*slot);
    }
}
