//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "channel.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // The select of Go: a wait on several channels at once, for a thread
    // (`select(...)`, which blocks) or a coroutine (`co_await
    // select(...)`), each case a channel's `on_receive(f)` or
    // `on_send(v, f)` (channel.h) and at most one `otherwise(f)`, the
    // case taken when no other can be served at once (Go's default).
    // The case served runs its body, on the calling thread or, for a
    // coroutine, on the worker that resumes it, and the select returns the
    // case's index. Among several cases ready at once one is chosen at
    // random, so that no channel starves another.
    //
    // No lock, as the channel has none: the cases are one waiter per
    // channel, on that channel's list, all sharing one state
    // (detail::SelectState); the channel that serves a case wins it by a
    // compare-exchange on that state, and the other cases' waiters stay
    // on their lists as dead entries, dropped when reached, reclaimed by
    // the collector. A select that registered and then finds a case ready
    // cancels the state the same way and looks again.
    template<class F>
    class otherwise_case {
    public:
        static constexpr bool is_otherwise = true;

        explicit otherwise_case(F f)
        : _f(std::move(f)) {
        }

        void run() {
            _f();
        }

    private:
        F _f;
    };

    template<class F>
    otherwise_case<F> otherwise(F f) {
        return otherwise_case<F>(std::move(f));
    }

    namespace detail {
        template<class C>
        concept IsOtherwise = requires { C::is_otherwise; };

        // Fairness: the case looked at first is a random one
        inline uint32_t select_random() noexcept {
            static thread_local uint32_t x = 2463534242u ^ (uint32_t)(uintptr_t)&x;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            return x;
        }

        template<class... Cases>
        class Select {
            static constexpr size_t N = sizeof...(Cases);
            static constexpr size_t Otherwise = [] {
                size_t i = 0, found = N;
                ((IsOtherwise<Cases> ? found = i : 0, ++i), ...);
                return found;
            }();
            static_assert(N > 0, "a select needs a case");
            static_assert(((IsOtherwise<Cases> ? 1 : 0) + ...) <= 1, "one otherwise at most");

        public:
            explicit Select(Cases... cases)
            : _cases(std::move(cases)...) {
            }

            // The blocking form: the index of the case served
            size_t wait() {
                for (;;) {
                    if (_try_any()) {
                        return _result;
                    }
                    tracked_ptr state = make_tracked<SelectState>();
                    _register(state);
                    std::atomic_thread_fence(std::memory_order_seq_cst);   // the pushes before the looks (channel.h: _fence)
                    if (_any_ready() && _cancel(*state)) {
                        _take_back();
                        continue;
                    }
                    state->signal.wait(0, std::memory_order_acquire);
                    return _finish(state->winner);
                }
            }

            // The awaitable form: co_await gives the index of the case served
            bool await_ready() {
                return _try_any();
            }

            // The state is Registering from the pushes to the look, so
            // that no channel serves a case before the look is done
            // (channel.h: Waiter); once it is Pending another thread may
            // serve a case and hand the coroutine to the scheduler, and
            // nothing of the frame (this awaiter) is touched past that
            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) {
                for (;;) {
                    tracked_ptr state = make_tracked<SelectState>();
                    state->state.store(SelectState::Registering, std::memory_order_relaxed);
                    state->coro = h;
                    state->frame = frame_of(h);
                    _state = state;
                    auto locals = _prepare(state);
                    _push(locals);
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                    if (!_any_ready(locals)) {
                        state->state.store(SelectState::Pending, std::memory_order_release);
                        return true;   // suspended: served, or woken by a close, later
                    }
                    state->state.store(SelectState::Cancelled, std::memory_order_release);
                    _take_back();
                    _state = nullptr;
                    if (_try_any()) {
                        return false;
                    }
                }
            }

            size_t await_resume() {
                if (_state) {
                    return _finish(_state->winner);
                }
                return _result;
            }

        private:
            template<size_t I>
            using Case = std::tuple_element_t<I, std::tuple<Cases...>>;

            // One case tried without waiting: served (its body run, the
            // index kept) or not
            template<size_t I>
            bool _try() {
                auto& c = std::get<I>(_cases);
                if constexpr (IsOtherwise<Case<I>>) {
                    return false;
                } else {
                    auto& ch = c.ch();
                    if constexpr (Case<I>::is_send) {
                        if (ch._closed.load(std::memory_order_acquire)) {
                            _result = I;
                            c.run(false);
                            return true;
                        }
                        if (ch._try_send(c.value())) {
                            _result = I;
                            c.run(true);
                            return true;
                        }
                        return false;
                    } else {
                        auto v = ch._try_receive();
                        if (!v && ch._closed.load(std::memory_order_acquire)) {
                            v = ch._try_receive();
                            _result = I;
                            c.run(std::move(v));
                            return true;
                        }
                        if (v) {
                            _result = I;
                            c.run(std::move(v));
                            return true;
                        }
                        return false;
                    }
                }
            }

            // A switch over the cases: _try<I> needs I as a template
            // argument (std::get<I> on the tuple of cases), and i is a
            // value, so the fold spells out `if (i == 0) _try<0>(); else if
            // (i == 1) ...`, the `|| ...` stopping at the first match. The
            // value of the whole expression is nothing; served is the result.
            template<size_t... Is>
            bool _try_at(size_t i, std::index_sequence<Is...>) {
                bool served = false;
                (void)((i == Is && (served = _try<Is>(), true)) || ...);
                return served;
            }

            // The cases in a random order; then the otherwise, if there is one
            bool _try_any() {
                size_t start = select_random() % N;
                for (size_t k = 0; k < N; ++k) {
                    if (_try_at((start + k) % N, std::make_index_sequence<N>())) {
                        return true;
                    }
                }
                if constexpr (Otherwise < N) {
                    _result = Otherwise;
                    std::get<Otherwise>(_cases).run();
                    return true;
                }
                return false;
            }

            // A waiter per case, made and filled but not yet on a list, and
            // what the pushes and the looks need, apart from the frame
            template<class C>
            struct Local {
                typename C::channel_type* ch = nullptr;
                typename C::channel_type::WaiterPtr waiter;
            };

            struct NoLocal {};

            template<size_t I>
            auto _prepare_one(const tracked_ptr<SelectState>& state) {
                auto& c = std::get<I>(_cases);
                if constexpr (IsOtherwise<Case<I>>) {
                    return NoLocal{};
                } else {
                    Local<Case<I>> l;
                    l.ch = &c.ch();
                    l.waiter = make_tracked<typename Case<I>::channel_type::Waiter>();
                    l.waiter->select = state;
                    l.waiter->index = I;
                    if constexpr (Case<I>::is_send) {
                        l.waiter->value.emplace(std::move(c.value()));
                    }
                    c.waiter = l.waiter;
                    return l;
                }
            }

            template<size_t... Is>
            auto _prepare(const tracked_ptr<SelectState>& state, std::index_sequence<Is...>) {
                return std::tuple(_prepare_one<Is>(state)...);
            }

            auto _prepare(const tracked_ptr<SelectState>& state) {
                assert(_one_direction_per_channel() && "a select with a send and a receive on one channel would serve itself: each looks at the other's waiter and neither is taken (Go's blocks)");
                return _prepare(state, std::make_index_sequence<N>());
            }

            template<size_t I, class L>
            static void _push_one(L& l) {
                if constexpr (!std::is_same_v<L, NoLocal>) {
                    if constexpr (Case<I>::is_send) {
                        l.ch->_senders.push(l.waiter);
                    } else {
                        l.ch->_receivers.push(l.waiter);
                    }
                }
            }

            template<class Locals>
            static void _push(Locals& locals) {
                [&]<size_t... Is>(std::index_sequence<Is...>) {
                    (_push_one<Is>(std::get<Is>(locals)), ...);
                }(std::make_index_sequence<N>());
            }

            template<size_t I, class L>
            static bool _ready_one(const L& l) {
                if constexpr (std::is_same_v<L, NoLocal>) {
                    return false;
                } else if constexpr (Case<I>::is_send) {
                    return l.ch->_something_to_send_to();
                } else {
                    return l.ch->_something_to_receive();
                }
            }

            template<class Locals>
            static bool _any_ready(const Locals& locals) {
                return [&]<size_t... Is>(std::index_sequence<Is...>) {
                    return (_ready_one<Is>(std::get<Is>(locals)) || ...);
                }(std::make_index_sequence<N>());
            }

            // The thread's forms: the frame is its own stack, the cases are at hand
            void _register(const tracked_ptr<SelectState>& state) {
                auto locals = _prepare(state);
                _push(locals);
            }

            bool _any_ready() const {
                return [&]<size_t... Is>(std::index_sequence<Is...>) {
                    return (_ready_case<Is>() || ...);
                }(std::make_index_sequence<N>());
            }

            template<size_t I>
            bool _ready_case() const {
                if constexpr (IsOtherwise<Case<I>>) {
                    return false;
                } else {
                    auto& c = std::get<I>(_cases);
                    if constexpr (Case<I>::is_send) {
                        return c.ch()._something_to_send_to();
                    } else {
                        return c.ch()._something_to_receive();
                    }
                }
            }

            // Debug: no channel with a send case and a receive case at once
            template<size_t I>
            const void* _channel_of() const noexcept {
                if constexpr (IsOtherwise<Case<I>>) {
                    return nullptr;
                } else {
                    return &std::get<I>(_cases).ch();
                }
            }

            template<size_t I>
            static constexpr bool _is_send() noexcept {
                if constexpr (IsOtherwise<Case<I>>) {
                    return false;
                } else {
                    return Case<I>::is_send;
                }
            }

            bool _one_direction_per_channel() const noexcept {
                return [&]<size_t... Is>(std::index_sequence<Is...>) {
                    const void* channels[N] = {_channel_of<Is>()...};
                    bool sends[N] = {_is_send<Is>()...};
                    for (size_t i = 0; i < N; ++i) {
                        for (size_t j = i + 1; j < N; ++j) {
                            if (channels[i] && channels[i] == channels[j] && sends[i] != sends[j]) {
                                return false;
                            }
                        }
                    }
                    return true;
                }(std::make_index_sequence<N>());
            }

            static bool _cancel(SelectState& state) noexcept {
                int e = SelectState::Pending;
                return state.state.compare_exchange_strong(e, SelectState::Cancelled, std::memory_order_acq_rel, std::memory_order_acquire);
            }

            // After a cancel: the send cases' elements back from their waiters
            void _take_back() {
                [&]<size_t... Is>(std::index_sequence<Is...>) {
                    (_take_back_one<Is>(), ...);
                }(std::make_index_sequence<N>());
            }

            template<size_t I>
            void _take_back_one() {
                if constexpr (!IsOtherwise<Case<I>>) {
                    if constexpr (Case<I>::is_send) {
                        auto& c = std::get<I>(_cases);
                        c.value() = std::move(*c.waiter->value);
                        c.waiter = nullptr;
                    }
                }
            }

            // The case served while the select waited: its body, its index
            size_t _finish(size_t winner) {
                [&]<size_t... Is>(std::index_sequence<Is...>) {
                    (void)((winner == Is && (_finish_one<Is>(), true)) || ...);   // the switch of _try_at
                }(std::make_index_sequence<N>());
                return winner;
            }

            template<size_t I>
            void _finish_one() {
                if constexpr (!IsOtherwise<Case<I>>) {
                    auto& c = std::get<I>(_cases);
                    auto& w = *c.waiter;
                    if constexpr (Case<I>::is_send) {
                        c.run(!(w.closed && w.value));   // delivered, unless the channel closed with the element still in hand
                    } else {
                        if (w.value) {
                            c.run(std::move(w.value));
                        } else {
                            c.run(c.ch()._try_receive());   // woken by the close: what was sent before it, if anything is left
                        }
                    }
                }
            }

            std::tuple<Cases...> _cases;
            tracked_ptr<SelectState> _state;
            size_t _result = 0;
        };
    }

    // Waits until one case is served, runs its body and gives its index:
    // `co_await async::select(...)` in a task, `async::select(...).wait()`
    // on a thread
    template<class... Cases>
    [[nodiscard]] detail::Select<Cases...> select(Cases... cases) {
        return detail::Select<Cases...>(std::move(cases)...);
    }
}
