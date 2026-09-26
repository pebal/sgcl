//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "channel.h"
#include "coroutine.h"

#include <atomic>
#include <cassert>
#include <exception>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // A once: the first caller runs the function, the others wait for it
    // to finish. An operation, as every wait of the module: `co_await
    // o.call(f)` in a task (the others' tasks wait holding no worker),
    // `o.call(f).wait()` on a thread; `co_await o.call(t)` runs the task t
    // once, and so does a coroutine function with captures. A call that
    // throws has been made all the same, as a promise set with an
    // exception is set: the first caller gets the exception, every other
    // caller, then and later, gets it again, and called() is true
    class once {
    public:
        once() = default;
        once(const once&) = delete;
        once& operator=(const once&) = delete;

        template<class F> requires (!detail::TaskFactory<F>)
        auto call(F f) {
            return operation([this, f = std::move(f)](auto how) mutable -> decltype(auto) {
                if constexpr (detail::is_awaited<decltype(how)>) {
                    return _co_call(std::move(f));
                } else {
                    _call(f);
                }
            });
        }

        // The same for a task: `co_await o.call(t)`
        template<class T>
        task<> call(task<T> t) {
            if (_claim()) {
                try {
                    co_await t;
                } catch (...) {
                    _fail();
                    throw;
                }
                _done.close();
            } else {
                co_await _done.receive();
                _rethrow();
            }
        }

        // And for a coroutine function with captures, which a function's
        // form would call and drop the task it gave, never started
        template<detail::TaskFactory F>
        task<> call(F f) {
            return call(detail::task_of(std::move(f)));
        }

        bool called() const noexcept {
            return _done.closed();
        }

    private:
        // the two halves of call(f): a thread's and a task's
        template<class F>
        void _call(F& f) {
            assert(!detail::on_worker() && "call(f).wait() blocks the worker: co_await o.call(f) from a task");
            if (_claim()) {
                _run(f);
            } else {
                (void)_done.receive().wait();
                _rethrow();
            }
        }

        template<class F>
        task<> _co_call(F f) {
            if (_claim()) {
                _run(f);
            } else {
                co_await _done.receive();
                _rethrow();
            }
        }

        // The first caller's run: done when f returns or throws
        template<class F>
        void _run(F& f) {
            try {
                f();
            } catch (...) {
                _fail();
                throw;
            }
            _done.close();
        }

        // f threw: kept for the others, and the once done (the close is
        // the release the waiters' receive pairs with)
        void _fail() noexcept {
            _error = std::current_exception();
            _done.close();
        }

        void _rethrow() const {
            if (_error) {
                std::rethrow_exception(_error);
            }
        }

        bool _claim() noexcept {
            int e = 0;
            return _state.compare_exchange_strong(e, 1, std::memory_order_acq_rel, std::memory_order_acquire);
        }

        std::atomic<int> _state = {0};
        std::exception_ptr _error;
        channel<void> _done;
    };
}
