//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/backoff.h"
#include "../core/map.h"
#include "../core/aliases.h"
#include "../core/atomic.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "coroutine.h"
#include "operation.h"
#include "promise.h"

#include <atomic>
#include <cassert>
#include <concepts>
#include <exception>
#include <type_traits>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // Go's golang.org/x/sync/singleflight: one call of a function per key
    // at a time. The first caller of a key runs f; every caller of the key
    // that comes before f ends waits for that call, holding no thread in a
    // task, and gets a copy of its result, or what it threw rethrown; the
    // first caller after the end runs f again. The cache stampede's cure:
    // a thousand requests for one cold entry make one fetch.
    //
    // The table is sixteen shards, each a hash map of a string to the call
    // in flight under a lock held for a lookup and an insertion or an
    // erasure, never across f (Go holds one mutex the same way; the
    // concurrent module's lock-free map cost 145 ns an insertion and an
    // erasure here, its node and its marker allocated, against 23 for the
    // core's map). A call is a small managed object: the type of its
    // result, and the promise its waiters wait on, made by the first
    // waiter under the lock, so that a call nobody joins allocates no
    // promise. The end of a call takes its own entry out, if the key still
    // holds it (a forget() and the next call of the key are left in place),
    // and the promise it found, under the lock, and sets it after. The
    // class is no template: run() takes the result's type from f, and a
    // caller whose f gives another type than the call in flight for its
    // key runs f on its own, shared with nobody (a debug build asserts:
    // it is almost surely a mistake of the program's).
    namespace detail {
        template<class T>
        struct FlightTag {
            static constexpr char id = 0;
        };

        // A call in flight. Its waiters' promise is made by the first of
        // them, outside the shard's lock, and put in with a
        // compare-exchange; the end marks the call ended and then reads the
        // promise, a waiter puts it in (or reads it) and then looks at the
        // mark, both sequentially consistent, so that either the end finds
        // the promise and sets it, or the waiter sees the end and looks the
        // key up again, as a caller after the end would
        struct FlightCall {
            const void* tag = nullptr;      // the result's type: &FlightTag<T>::id
            atomic<tracked_ptr<void>> done; // the PromiseState<T> of the waiters
            std::atomic<bool> ended{false};
        };

        // A shard: its lock, its map, a line of its own
        struct FlightShard {
            std::atomic<bool> busy{false};
            map<string, tracked_ptr<FlightCall>> calls;
            char pad[128];

            SGCL_INLINE_HOT void lock() noexcept {
                if (!busy.exchange(true, std::memory_order_acquire)) [[likely]] {
                    return;
                }
                Backoff<> backoff;
                do {
                    while (busy.load(std::memory_order_relaxed)) {
                        backoff();
                    }
                } while (busy.exchange(true, std::memory_order_acquire));
            }

            SGCL_INLINE_HOT void unlock() noexcept {
                busy.store(false, std::memory_order_release);
            }
        };

        struct FlightTable {
            static constexpr size_t Shards = 16;
            FlightShard shards[Shards];

            SGCL_INLINE_HOT FlightShard& shard(const string& key) noexcept {
                size_t h = std::hash<string>{}(key);
                return shards[(h ^ (h >> 17)) & (Shards - 1)];
            }
        };

        template<class X>
        struct FlightTaskValue {};

        template<class T>
        struct FlightTaskValue<task<T>> {
            using type = T;
        };

        // What a caller gets: f's result, or the result of f's task
        template<class F>
        struct FlightResultOf {
            using type = std::invoke_result_t<F&>;
        };

        template<TaskFactory F>
        struct FlightResultOf<F> {
            using type = typename FlightTaskValue<std::remove_cvref_t<std::invoke_result_t<F&>>>::type;
        };

        template<class F>
        using FlightResult = typename FlightResultOf<F>::type;

        // A caller's place: the call it runs (mine), or the promise it
        // waits on, or neither (f run alone: another type in flight)
        template<class T>
        struct FlightJoin {
            tracked_ptr<FlightCall> mine;
            tracked_ptr<PromiseState<T>> wait;
        };

        template<class T>
        FlightJoin<T> flight_join(FlightTable& table, const string& key) {
            FlightShard& sh = table.shard(key);
            FlightJoin<T> j;
            tracked_ptr<FlightCall> made = make_tracked<FlightCall>();   // before the lock: little allocated under it (the map's node)
            made->tag = &FlightTag<T>::id;
            for (;;) {
                sh.lock();
                auto [at, inserted] = sh.calls.try_emplace(key, made);
                if (inserted) {
                    sh.unlock();
                    j.mine = std::move(made);
                    return j;
                }
                tracked_ptr<FlightCall> c = at->second;
                sh.unlock();
                if (c->tag != &FlightTag<T>::id) {
                    assert(false && "singleflight: a call of the key with another result type is in flight");
                    return j;
                }
                tracked_ptr<void> done = c->done.load();
                if (!done) {
                    tracked_ptr<PromiseState<T>> p = make_tracked<PromiseState<T>>();
                    p->link();
                    tracked_ptr<void> none;
                    done = c->done.compare_exchange_strong(none, p) ? tracked_ptr<void>(p) : none;   // another waiter's, if it was first
                }
                if (c->ended.load()) {   // the end may not have seen the promise: the key looked up again
                    continue;
                }
                j.wait = done.template as<PromiseState<T>>();
                return j;
            }
        }

        // The end of a call: its entry out of the shard if the key still
        // holds it, then the call marked ended and the promise of its
        // waiters read, to be set
        template<class T>
        tracked_ptr<PromiseState<T>> flight_end(FlightTable& table, const string& key, const tracked_ptr<FlightCall>& call) noexcept {
            FlightShard& sh = table.shard(key);
            sh.lock();
            auto at = sh.calls.find(key);
            if (at != sh.calls.end() && at->second == call) {
                sh.calls.erase(at);
            }
            sh.unlock();
            call->ended.store(true);
            return call->done.load().template as<PromiseState<T>>();
        }

        template<class T, class F>
        T flight_block(FlightTable& table, const string& key, F& f);

        // What run() returns: the operation of a call, carried out by
        // `co_await` (an awaitable itself: the join in await_ready, no
        // frame of its own; a waiter awaits the promise, the runner calls a
        // function there and then, or awaits the task of a coroutine
        // function and ends the call in await_resume, where the task's
        // result or exception arrives) or by `.wait()` on a thread. Its own
        // type rather than an operation<F> of a closure, as select's is:
        // the table and the key held once (the closure and the operation
        // around it held them three times, 62 ns a call in a task against
        // 52 for Go's Do; a call on a thread 51)
        template<class T, class F>
        class [[nodiscard]] FlightAwaiter {
            using Value = std::conditional_t<std::is_void_v<T>, char, T>;
            using PromiseAwaiter = decltype(std::declval<PromiseState<T>&>().operator co_await());

        public:
            SGCL_INLINE_HOT FlightAwaiter(const tracked_ptr<FlightTable>& table, const string& key, F f) noexcept(std::is_nothrow_move_constructible_v<F>)
            : _table(table)
            , _key(key)
            , _f(std::move(f)) {
            }

            FlightAwaiter(const FlightAwaiter&) = delete;
            FlightAwaiter& operator=(const FlightAwaiter&) = delete;

            // The thread's way: blocks until the call ends, gives its result
            SGCL_INLINE_HOT T wait() && {
                return flight_block<T>(*_table, _key, _f);
            }

            bool await_ready() {
                _j = flight_join<T>(*_table, _key);
                if (_j.wait) {
                    _waiting.emplace(_j.wait->operator co_await());
                    return _waiting->await_ready();
                }
                if constexpr (TaskFactory<F>) {
                    _task.emplace(_f());
                    return false;   // the task's awaiter starts it in await_suspend, done or not
                } else {
                    try {
                        if constexpr (std::is_void_v<T>) {
                            _f();
                        } else {
                            _value.emplace(_f());
                        }
                    } catch (...) {
                        _error = std::current_exception();
                    }
                    _end();
                    return true;
                }
            }

            template<class P>
            SGCL_INLINE_HOT bool await_suspend(std::coroutine_handle<P> h) {
                if (_waiting) {
                    return _waiting->await_suspend(h);
                }
                if constexpr (TaskFactory<F>) {
                    return _task->operator co_await().await_suspend(h);
                } else {
                    return false;
                }
            }

            T await_resume() {
                if (_waiting) {
                    if constexpr (std::is_void_v<T>) {
                        _waiting->await_resume();
                        return;
                    } else {
                        return T(_waiting->await_resume());
                    }
                }
                if constexpr (TaskFactory<F>) {
                    try {
                        if constexpr (std::is_void_v<T>) {
                            _task->operator co_await().await_resume();
                        } else {
                            _value.emplace(_task->operator co_await().await_resume());
                        }
                    } catch (...) {
                        _error = std::current_exception();
                    }
                    _end();
                }
                if (_error) {
                    std::rethrow_exception(_error);
                }
                if constexpr (!std::is_void_v<T>) {
                    return std::move(*_value);
                }
            }

        private:
            // The call ended: its waiters given the value or the exception
            void _end() {
                if (!_j.mine) {
                    return;
                }
                if (auto done = flight_end<T>(*_table, _key, _j.mine)) {
                    if (_error) {
                        done->set_exception(_error);
                    } else if constexpr (std::is_void_v<T>) {
                        done->set_value();
                    } else {
                        done->set_value(*_value);
                    }
                }
            }

            tracked_ptr<FlightTable> _table;
            string _key;
            F _f;
            FlightJoin<T> _j;
            optional<PromiseAwaiter> _waiting;
            optional<std::conditional_t<TaskFactory<F>, task<T>, char>> _task;
            optional<Value> _value;
            std::exception_ptr _error;
        };

        template<class T, class F>
        T flight_block(FlightTable& table, const string& key, F& f) {
            FlightJoin<T> j = flight_join<T>(table, key);
            if (j.wait) {
                if constexpr (std::is_void_v<T>) {
                    j.wait->wait();
                    return;
                } else {
                    return T(j.wait->wait());
                }
            }
            try {
                if constexpr (std::is_void_v<T>) {
                    if constexpr (TaskFactory<F>) {
                        f().wait();
                    } else {
                        f();
                    }
                    if (j.mine) {
                        if (auto done = flight_end<T>(table, key, j.mine)) {
                            done->set_value();
                        }
                    }
                } else {
                    optional<T> v;
                    if constexpr (TaskFactory<F>) {
                        v.emplace(std::move(f().wait()));
                    } else {
                        v.emplace(f());
                    }
                    if (j.mine) {
                        if (auto done = flight_end<T>(table, key, j.mine)) {
                            done->set_value(*v);
                        }
                    }
                    return std::move(*v);
                }
            } catch (...) {
                if (j.mine) {
                    if (auto done = flight_end<T>(table, key, j.mine)) {
                        done->set_exception(std::current_exception());
                    }
                }
                throw;
            }
        }
    }

    // One call per key at a time: a handle, one tracked word, its copies
    // sharing the table of calls in flight
    class singleflight {
    public:
        singleflight()
        : _s(make_tracked<detail::FlightTable>()) {
        }

        // f run once for the key and its result given to every caller of
        // the key until it ends: `co_await flights.run(key, f)` in a task,
        // `flights.run(key, f).wait()` on a thread. f is a function, called
        // by the first caller where it runs, or a coroutine function whose
        // task that caller awaits; what f throws is rethrown to every
        // caller of the call. The result is a copy of f's for each
        template<class F>
        requires std::invocable<F&>
        SGCL_INLINE_HOT auto run(const string& key, F f) const {
            using T = detail::FlightResult<F>;
            static_assert(std::is_void_v<T> || std::is_copy_constructible_v<T>, "sgcl::async::singleflight: the result is copied to every caller");
            return detail::FlightAwaiter<T, F>(_s, key, std::move(f));
        }

        // The key let go of: the next run of it starts a new call; the
        // callers of the call in flight get its result still
        SGCL_INLINE_HOT void forget(const string& key) const noexcept {
            detail::FlightShard& sh = _s->shard(key);
            sh.lock();
            sh.calls.erase(key);
            sh.unlock();
        }

        SGCL_INLINE_HOT friend bool operator==(const singleflight& a, const singleflight& b) noexcept {
            return a._s == b._s;
        }

    private:
        tracked_ptr<detail::FlightTable> _s;
    };
}
