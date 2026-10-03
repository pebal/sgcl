//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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

        // Whether awaiter_of (below) cannot throw: its operator co_await's
        // noexcept, or true for the awaitable itself
        template<class A>
        constexpr bool nothrow_awaiter_of() noexcept {
            if constexpr (requires (A& a) { a.operator co_await(); }) {
                return noexcept(std::declval<A&>().operator co_await());
            } else if constexpr (requires (A& a) { operator co_await(a); }) {
                return noexcept(operator co_await(std::declval<A&>()));
            } else {
                return true;
            }
        }

        // The awaiter of an awaitable: its operator co_await, member or
        // free, or the awaitable itself
        template<class A>
        decltype(auto) awaiter_of(A& a) noexcept(nothrow_awaiter_of<A>()) {
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
            operator std::invoke_result_t<F&, awaited_t>() noexcept(std::is_nothrow_invocable_v<F&, awaited_t>) {
                return f(awaited);
            }
        };
    }

    template<class F>
    class operation;

    namespace detail {
        // An operation of F: the only way to make one (operation's
        // constructor from F is private), since F is called with the tags
        // above, which are the library's own
        template<class F>
        operation<F> make_operation(F f) noexcept(std::is_nothrow_move_constructible_v<F>);
    }

    // An operation that may wait, one name for both ways of waiting:
    // `co_await op` in a task suspends the task and gives the worker back,
    // `op.wait()` on a thread blocks the thread. It is a description until
    // then (nothing is done by making it, hence nodiscard); the blocking
    // form goes to the operation's own blocking code, never through the
    // scheduler, so a thread that waits pays what the call alone costs.
    // F is called with detail::awaited for the awaitable (a task or an
    // awaiter) and with detail::blocking for the result of the wait; the
    // module's functions make it (detail::make_operation), a program only
    // gets one, moves it and carries it out.
    template<class F>
    class [[nodiscard]] operation {
        using Awaitable = std::invoke_result_t<F&, detail::awaited_t>;
        using AwaiterRef = decltype(detail::awaiter_of(std::declval<Awaitable&>()));
        static constexpr bool Owned = !std::is_lvalue_reference_v<AwaiterRef>;
        using Awaiter = std::remove_cvref_t<AwaiterRef>;

        // What the coroutine's way does before the awaiter's await_ready:
        // the awaitable made by F, and an awaiter owned beside it moved in
        static constexpr bool NothrowMade = std::is_nothrow_constructible_v<Awaitable, detail::MadeBy<F>>
            && (!Owned || (detail::nothrow_awaiter_of<Awaitable>() && std::is_nothrow_constructible_v<Awaiter, AwaiterRef>));

    public:
        operation(operation&& o) noexcept(std::is_nothrow_move_constructible_v<F>)
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
        decltype(auto) wait() && noexcept(std::is_nothrow_invocable_v<F&, detail::blocking_t>) {
            _carried_out();
            return _f(detail::blocking);
        }

        // The coroutine's way (co_await): the awaitable made now, in place
        bool await_ready() noexcept(NothrowMade && noexcept(std::declval<Awaiter&>().await_ready())) {
            _carried_out();
            _awaitable.emplace(detail::MadeBy<F>{_f});
            if constexpr (Owned) {
                _awaiter.emplace(detail::awaiter_of(*_awaitable));
            }
            return _get().await_ready();
        }

        template<class H>
        decltype(auto) await_suspend(H h) noexcept(noexcept(std::declval<Awaiter&>().await_suspend(h))) {
            return _get().await_suspend(h);
        }

        decltype(auto) await_resume() noexcept(noexcept(std::declval<Awaiter&>().await_resume())) {
            return _get().await_resume();
        }

    private:
        template<class G>
        friend operation<G> detail::make_operation(G f) noexcept(std::is_nothrow_move_constructible_v<G>);

        explicit operation(F f) noexcept(std::is_nothrow_move_constructible_v<F>)
        : _f(std::move(f)) {
        }

        void _carried_out() noexcept {
#ifndef NDEBUG
            _pending = false;
#endif
        }

        Awaiter& _get() noexcept(Owned || detail::nothrow_awaiter_of<Awaitable>()) {
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

}

namespace sgcl::async::detail {
    template<class F>
    operation<F> make_operation(F f) noexcept(std::is_nothrow_move_constructible_v<F>) {
        return operation<F>(std::move(f));
    }

    // An operation of two forms: co() the awaitable, block() the blocking
    // call; each captures what it needs
    template<class Co, class Block>
    auto either(Co co, Block block) noexcept(std::is_nothrow_move_constructible_v<Co> && std::is_nothrow_move_constructible_v<Block>) {
        return make_operation([co = std::move(co), block = std::move(block)](auto how) mutable -> decltype(auto) {
            if constexpr (std::is_same_v<decltype(how), awaited_t>) {
                return co();
            } else {
                return block();
            }
        });
    }
}
