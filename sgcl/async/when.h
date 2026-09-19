//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../containers/vector.h"
#include "channel.h"
#include "coroutine.h"
#include "scheduler.h"

#include <cstddef>
#include <exception>
#include <optional>
#include <tuple>
#include <utility>

namespace sgcl {
    // The composition of tasks: `co_await when_all(a, b, c)` waits for
    // every task and gives their results as a tuple (a vector for a range
    // of tasks of one type; nothing for tasks of nothing), what any of
    // them threw rethrown; `co_await when_any(a, b, c)` gives the index of
    // the first to finish and lets go of the rest, which run on and are
    // the collector's once done. The tasks are taken over (moved in): a
    // task is awaited by one awaiter, and the result of when_all is where
    // theirs are. They are spawned ones, or ones the caller runs some
    // other way: when_all and when_any wait, they do not start.
    namespace detail {
        // One task awaited: its result kept, or its exception, the first
        // one only; the others are awaited all the same. An awaiter over
        // the task's own, not a task of its own: a task per task awaited
        // cost a frame, a start and two hops of the scheduler each
        // (831 ns per task against about 300, measured)
        template<class T>
        class await_into {
        public:
            await_into(task<T>& t, std::optional<T>& out, std::exception_ptr& error) noexcept
            : _a(t.operator co_await())
            , _out(out)
            , _error(error) {
            }

            bool await_ready() const noexcept {
                return _a.await_ready();
            }

            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) noexcept {
                return _a.await_suspend(h);
            }

            void await_resume() {
                try {
                    _out.emplace(_a.await_resume());
                } catch (...) {
                    if (!_error) {
                        _error = std::current_exception();
                    }
                }
            }

        private:
            decltype(std::declval<task<T>&>().operator co_await()) _a;
            std::optional<T>& _out;
            std::exception_ptr& _error;
        };

        class await_into_void {
        public:
            await_into_void(task<>& t, std::exception_ptr& error) noexcept
            : _a(t.operator co_await())
            , _error(error) {
            }

            bool await_ready() const noexcept {
                return _a.await_ready();
            }

            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) noexcept {
                return _a.await_suspend(h);
            }

            void await_resume() {
                try {
                    _a.await_resume();
                } catch (...) {
                    if (!_error) {
                        _error = std::current_exception();
                    }
                }
            }

        private:
            decltype(std::declval<task<>&>().operator co_await()) _a;
            std::exception_ptr& _error;
        };
    }

    template<class... T>
    task<std::tuple<T...>> when_all(task<T>... ts) {
        std::tuple<std::optional<T>...> results;
        std::exception_ptr error;
        co_await [&]<size_t... Is>(std::index_sequence<Is...>) -> task<> {
            (co_await detail::await_into(std::get<Is>(std::tie(ts...)), std::get<Is>(results), error), ...);
        }(std::index_sequence_for<T...>());
        if (error) {
            std::rethrow_exception(error);
        }
        co_return [&]<size_t... Is>(std::index_sequence<Is...>) {
            return std::tuple<T...>{std::move(*std::get<Is>(results))...};
        }(std::index_sequence_for<T...>());
    }

    template<class... Void>
    requires (std::is_void_v<Void> && ...)
    task<> when_all(task<Void>... ts) {
        std::exception_ptr error;
        (co_await detail::await_into_void(ts, error), ...);
        if (error) {
            std::rethrow_exception(error);
        }
    }

    template<class T>
    task<vector<T>> when_all(vector<task<T>> ts) {
        vector<std::optional<T>> results(ts.size());
        std::exception_ptr error;
        for (size_t i = 0; i < ts.size(); ++i) {
            co_await detail::await_into(ts[i], results[i], error);
        }
        if (error) {
            std::rethrow_exception(error);
        }
        vector<T> out;
        out.reserve(ts.size());
        for (auto& r : results) {
            out.push_back(std::move(*r));
        }
        co_return out;
    }

    inline task<> when_all(vector<task<>> ts) {
        std::exception_ptr error;
        for (auto& t : ts) {
            co_await detail::await_into_void(t, error);
        }
        if (error) {
            std::rethrow_exception(error);
        }
    }

    namespace detail {
        // The first to finish reports its index and what it threw, if it
        // threw: the winner's exception is when_any's (as when_all's is a
        // task's), a loser's is dropped with the loser
        using Finished = pair<size_t, std::exception_ptr>;

        template<class T>
        task<> finish_into(task<T> t, tracked_ptr<channel<Finished>> done, size_t index) {
            std::exception_ptr error;
            try {
                co_await t;
            } catch (...) {
                error = std::current_exception();
            }
            done->try_send(Finished(index, std::move(error)));
        }

        inline task<size_t> first_finished(tracked_ptr<channel<Finished>> done) {
            auto [index, error] = *co_await done->async_receive();
            if (error) {
                std::rethrow_exception(error);
            }
            co_return index;
        }
    }

    // The index of the first task to finish, or what it threw; the others
    // let go of
    template<class... T>
    task<size_t> when_any(task<T>... ts) {
        static_assert(sizeof...(T) > 0, "when_any of nothing");
        tracked_ptr<channel<detail::Finished>> done = make_tracked<channel<detail::Finished>>(sizeof...(T));
        size_t i = 0;
        (go(detail::finish_into(std::move(ts), done, i++)), ...);
        return detail::first_finished(done);
    }

    // The same over a vector; a vector of nothing gives SIZE_MAX at once
    // (nothing can finish first)
    template<class T>
    task<size_t> when_any(vector<task<T>> ts) {
        if (ts.empty()) {
            return []() -> task<size_t> { co_return SIZE_MAX; }();
        }
        tracked_ptr<channel<detail::Finished>> done = make_tracked<channel<detail::Finished>>(ts.size());
        for (size_t i = 0; i < ts.size(); ++i) {
            go(detail::finish_into(std::move(ts[i]), done, i));
        }
        return detail::first_finished(done);
    }
}
