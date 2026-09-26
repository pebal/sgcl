//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"

#include <cassert>
#include <coroutine>
#include <type_traits>
#include <utility>

namespace sgcl::detail {}

namespace sgcl::async {
    namespace detail {
        using namespace sgcl::detail;

        // The two ways an operation is carried out: its awaitable, for a
        // coroutine, or its blocking form, for a thread
        struct awaited_t {};
        struct blocking_t {};
        inline constexpr awaited_t awaited = {};
        inline constexpr blocking_t blocking = {};

        template<class How>
        inline constexpr bool is_awaited = std::is_same_v<How, awaited_t>;

        // The awaiter of an awaitable: its operator co_await, member or
        // free, or the awaitable itself
        template<class A>
        decltype(auto) awaiter_of(A& a) {
            if constexpr (requires { a.operator co_await(); }) {
                return a.operator co_await();
            } else if constexpr (requires { operator co_await(a); }) {
                return operator co_await(a);
            } else {
                return (a);
            }
        }

        // A value made where it is to live, by a call: what lets optional
        // hold an awaiter that can be neither copied nor moved
        template<class F>
        struct MadeBy {
            F& f;
            operator std::invoke_result_t<F&, awaited_t>() {
                return f(awaited);
            }
        };
    }

    // An operation that may wait, one name for both ways of waiting:
    // `co_await op` in a task suspends the task and gives the worker back,
    // `op.wait()` on a thread blocks the thread. It is a description until
    // then (nothing is done by making it, hence nodiscard); the blocking
    // form goes to the operation's own blocking code, never through the
    // scheduler, so a thread that waits pays what the call alone costs.
    // F is called with detail::awaited for the awaitable (a task or an
    // awaiter) and with detail::blocking for the result of the wait.
    template<class F>
    class [[nodiscard]] operation {
        using Awaitable = std::invoke_result_t<F&, detail::awaited_t>;
        using AwaiterRef = decltype(detail::awaiter_of(std::declval<Awaitable&>()));
        static constexpr bool Owned = !std::is_lvalue_reference_v<AwaiterRef>;
        using Awaiter = std::remove_cvref_t<AwaiterRef>;

    public:
        explicit operation(F f)
        : _f(std::move(f)) {
        }

        operation(operation&& o)
        : _f(std::move(o._f))
#ifndef NDEBUG
        , _pending(std::exchange(o._pending, false))
#endif
        {
        }

        operation(const operation&) = delete;
        operation& operator=(const operation&) = delete;

        // An operation made and never carried out did nothing: a debug
        // build says so where it is dropped. A (void) cast silences
        // nodiscard, and `(void)f->close();` then closes nothing
        ~operation() {
            assert(!_pending && "an operation made and never carried out: co_await it in a task or call .wait() on a thread");
        }

        // The thread's way: blocks until the operation is done, and returns
        // its result
        decltype(auto) wait() && {
            _carried_out();
            return _f(detail::blocking);
        }

        // The coroutine's way (co_await): the awaitable made now, in place
        bool await_ready() {
            _carried_out();
            _awaitable.emplace(detail::MadeBy<F>{_f});
            if constexpr (Owned) {
                _awaiter.emplace(detail::awaiter_of(*_awaitable));
            }
            return _get().await_ready();
        }

        template<class H>
        decltype(auto) await_suspend(H h) {
            return _get().await_suspend(h);
        }

        decltype(auto) await_resume() {
            return _get().await_resume();
        }

    private:
        void _carried_out() noexcept {
#ifndef NDEBUG
            _pending = false;
#endif
        }

        Awaiter& _get() {
            if constexpr (Owned) {
                return *_awaiter;
            } else {
                return detail::awaiter_of(*_awaitable);
            }
        }

        F _f;
        optional<Awaitable> _awaitable;
        struct Empty {};
        [[no_unique_address]] std::conditional_t<Owned, optional<Awaiter>, Empty> _awaiter;
#ifndef NDEBUG
        bool _pending = true;
#endif
    };

    template<class F>
    operation(F) -> operation<F>;
}

namespace sgcl::async::detail {
    // An operation of two forms: co() the awaitable, block() the blocking
    // call; each captures what it needs
    template<class Co, class Block>
    auto either(Co co, Block block) {
        return operation([co = std::move(co), block = std::move(block)](auto how) mutable -> decltype(auto) {
            if constexpr (std::is_same_v<decltype(how), awaited_t>) {
                return co();
            } else {
                return block();
            }
        });
    }
}
