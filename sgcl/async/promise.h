//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/handle_word.h"
#include "../core/aliases.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "channel.h"
#include "coroutine.h"

#include <atomic>
#include <cassert>
#include <exception>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // A one-shot completion: a value (or an exception) that one side sets
    // once and any number of others wait for, the adapter between the
    // callback APIs of a platform (an I/O completion port, a dispatch
    // queue, JNI, a driver's completion routine, a C library with a
    // `void*` context) and `co_await`. The thread that has the result
    // calls set_value(v) or set_exception(e) and returns at once; a task
    // that waits writes `co_await p` and holds no thread until then; a
    // thread writes p.wait() and blocks; a select takes p.on_done(f) as a
    // case (select.h). Under it an event (event.h) with a value: a channel
    // of signals closed by the set, so that the three forms of the wait
    // are the channel's, lock-free, the waiters reclaimed by the
    // collector. No std::future: the promise is one object, the value
    // lives in it, and a waiter reads it there; a tracked_ptr value keeps
    // its object as any member would. Not the shape of std::promise (a
    // promise and a future apart), on purpose: a completion has one home,
    // and whoever has a copy of the handle may set it or wait for it.
    //
    // Exactly one setter wins: the first set_value or set_exception
    // claims the promise with a compare-exchange and stores; a second is
    // an error (asserted in debug builds; ignored in release, the first
    // value stands). The promise is a handle (below): a copy is the same
    // promise; it lives where a tracked_ptr may (on a stack, in a task, in
    // a managed object), and a foreign thread's callback holds it through
    // a rooted<async::promise<T>> (a `void*` context).
    namespace detail {
        // The state and the waiting, shared by promise<T> and promise<void>
        class PromiseBase {
        public:
            PromiseBase() noexcept = default;
            PromiseBase(const PromiseBase&) = delete;
            PromiseBase& operator=(const PromiseBase&) = delete;

            // Whether the value or the exception is in
            bool done() const noexcept {
                return _done.closed();
            }

            // A case of a select: f() once the promise is done
            template<class F>
            auto on_done(F f) noexcept(std::is_nothrow_move_constructible_v<F>) {
                return _done.on_receive(std::move(f));
            }

        protected:
            // The claim of the one setter: true for the first
            bool _claim() noexcept {
                int e = Pending;
                bool first = _state.compare_exchange_strong(e, Claimed, std::memory_order_acq_rel, std::memory_order_acquire);
                assert(first && "a promise is set once");
                return first;
            }

            // The value or the exception is stored: the waiters woken
            void _publish() {
                _done.close();
            }

            // The thread's wait of wait() and result()
            void _block() {
                assert(!detail::on_worker() && "wait() blocks the worker: co_await the promise from a task");
                (void)_done.receive().wait();
            }

            // The awaitable's wait: the channel's, with nothing returned
            class await_op {
            public:
                bool await_ready() {
                    return _op.await_ready();
                }

                template<class P>
                bool await_suspend(std::coroutine_handle<P> h) {
                    return _op.await_suspend(h);
                }

                void await_resume() {
                    _op.await_resume();
                }

            private:
                friend class PromiseBase;

                explicit await_op(ChannelState<void>& ch) noexcept
                : _op(ch.receive()) {
                }

                decltype(std::declval<ChannelState<void>&>().receive()) _op;
            };

            await_op _await() noexcept {
                return await_op(_done);
            }

            std::exception_ptr _error;

        private:
            enum State : int { Pending, Claimed };

            std::atomic<int> _state = {Pending};
            ChannelState<void> _done{ChannelLinked{}, 0};   // linked by promise() (link())

        public:
            // the channel's lists linked: once, by the code that made the
            // state, right after make_tracked (ChannelState::link)
            void link() noexcept {
                _done.link();
            }
        };
    }

    namespace detail {
        template<class T>
        class PromiseState
        : public PromiseBase {
        public:
            using value_type = T;

            PromiseState() noexcept = default;

            // The value in, the waiters woken; the first setter only
            void set_value(const T& v) {
                _set(T(v));
            }

            void set_value(T&& v) {
                _set(T(std::move(v)));
            }

            void set_exception(std::exception_ptr e) {
                assert(e && "a promise is set with an exception, never with a null exception_ptr: result() would have neither");
                if (_claim()) {
                    _error = std::move(e);
                    _publish();
                }
            }

            // The value, or what was set as the exception, rethrown: waited
            // for first, on this thread, when the promise is not done yet (as
            // wait() does); a reference into the promise, so that the value
            // has one home whoever reads it (a lone reader may move it out)
            T& result() {
                if (!done()) {
                    _block();
                }
                if (_error) {
                    std::rethrow_exception(_error);
                }
                return *_value;
            }

            // Waits for the set, on this thread, and gives the value, or
            // rethrows what was set as the exception. Not from a task on a
            // worker (co_await it there)
            T& wait() {
                _block();
                return result();
            }

            // `co_await p`: the task suspended until the promise is done, no
            // thread held, then the value or the exception
            class awaiter {
            public:
                bool await_ready() {
                    return _op.await_ready();
                }

                template<class P>
                bool await_suspend(std::coroutine_handle<P> h) {
                    return _op.await_suspend(h);
                }

                T& await_resume() {
                    _op.await_resume();
                    return _p->result();
                }

            private:
                friend class PromiseState;

                explicit awaiter(PromiseState& p) noexcept
                : _p(&p)
                , _op(p._await()) {
                }

                PromiseState* _p;
                await_op _op;
            };

            awaiter operator co_await() noexcept {
                return awaiter(*this);
            }

        private:
            void _set(T v) {
                if (_claim()) {
                    try {
                        _value.emplace(std::move(v));
                    } catch (...) {
                        _error = std::current_exception();   // a move that throws: what the waiters get, the promise set all the same (claimed and never published, they would wait for good)
                    }
                    _publish();
                }
            }

            optional<T> _value;
        };

        // A promise of nothing: a completion without a value
        template<>
        class PromiseState<void>
        : public PromiseBase {
        public:
            using value_type = void;

            PromiseState() noexcept = default;

            void set_value() {
                if (_claim()) {
                    _publish();
                }
            }

            void set_exception(std::exception_ptr e) {
                assert(e && "a promise is set with an exception, never with a null exception_ptr: result() would have neither");
                if (_claim()) {
                    _error = std::move(e);
                    _publish();
                }
            }

            // Rethrows what was set as the exception, if anything: waited for
            // first, on this thread, when the promise is not done yet
            void result() {
                if (!done()) {
                    _block();
                }
                if (_error) {
                    std::rethrow_exception(_error);
                }
            }

            // Waits for the set, on this thread, and rethrows what was set as
            // the exception; not from a task on a worker
            void wait() {
                _block();
                result();
            }

            class awaiter {
            public:
                bool await_ready() {
                    return _op.await_ready();
                }

                template<class P>
                bool await_suspend(std::coroutine_handle<P> h) {
                    return _op.await_suspend(h);
                }

                void await_resume() {
                    _op.await_resume();
                    _p->result();
                }

            private:
                friend class PromiseState;

                explicit awaiter(PromiseState& p) noexcept
                : _p(&p)
                , _op(p._await()) {
                }

                PromiseState* _p;
                await_op _op;
            };

            awaiter operator co_await() noexcept {
                return awaiter(*this);
            }
        };
    }

    // The handle of a promise: one word, a tracked_ptr to the state
    // (detail::PromiseState, where the value lives), which copies share —
    // the side that sets and the sides that wait each hold one: a task's
    // parameter by value, a member of a managed object, a local; in a
    // global, a std container or a C callback's context, a
    // rooted<async::promise<T>>. Made empty (not set) by its constructor.
    template<class T = void>
    class promise {
        using State = detail::PromiseState<T>;

    public:
        using value_type = T;
        using awaiter = typename State::awaiter;

        promise() noexcept
        : _s(make_tracked<State>()) {
            _s->link();   // before the state is given to anyone
        }

        promise(const promise&) noexcept = default;
        promise(promise&&) noexcept = default;
        promise& operator=(const promise&) noexcept = default;
        promise& operator=(promise&&) noexcept = default;

        // The value in, the waiters woken; the first setter only
        void set_value(const T& v) const {
            _s->set_value(v);
        }

        void set_value(T&& v) const {
            _s->set_value(std::move(v));
        }

        void set_exception(std::exception_ptr e) const {
            _s->set_exception(std::move(e));
        }

        // Whether the value or the exception is in
        bool done() const noexcept {
            return _s->done();
        }

        // A case of a select: f() once the promise is done
        template<class F>
        auto on_done(F f) const noexcept(std::is_nothrow_move_constructible_v<F>) {
            return _s->on_done(std::move(f));
        }

        // The value, waited for first when the promise is not done; a
        // reference into the state, the value's one home
        T& result() const {
            return _s->result();
        }

        // Waits for the set, on this thread (not from a task on a worker:
        // co_await it there), and gives the value or rethrows
        T& wait() const {
            return _s->wait();
        }

        awaiter operator co_await() const noexcept {
            return _s->operator co_await();
        }

        // The same promise: the same state
        friend bool operator==(const promise& a, const promise& b) noexcept {
            return a._s == b._s;
        }

    private:
        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        promise(sgcl::detail::FromWord, const tracked_ptr<State>& w) noexcept
        : _s(w) {
        }

        tracked_ptr<State>& _handle_word() noexcept {
            return _s;
        }

        const tracked_ptr<State>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<State> _s;
    };

    // A promise of nothing: a completion without a value
    template<>
    class promise<void> {
        using State = detail::PromiseState<void>;

    public:
        using value_type = void;
        using awaiter = State::awaiter;

        promise() noexcept
        : _s(make_tracked<State>()) {
            _s->link();   // before the state is given to anyone
        }

        promise(const promise&) noexcept = default;
        promise(promise&&) noexcept = default;
        promise& operator=(const promise&) noexcept = default;
        promise& operator=(promise&&) noexcept = default;

        void set_value() const {
            _s->set_value();
        }

        void set_exception(std::exception_ptr e) const {
            _s->set_exception(std::move(e));
        }

        bool done() const noexcept {
            return _s->done();
        }

        template<class F>
        auto on_done(F f) const noexcept(std::is_nothrow_move_constructible_v<F>) {
            return _s->on_done(std::move(f));
        }

        void result() const {
            _s->result();
        }

        void wait() const {
            _s->wait();
        }

        awaiter operator co_await() const noexcept {
            return _s->operator co_await();
        }

        friend bool operator==(const promise& a, const promise& b) noexcept {
            return a._s == b._s;
        }

    private:
        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        promise(sgcl::detail::FromWord, const tracked_ptr<State>& w) noexcept
        : _s(w) {
        }

        tracked_ptr<State>& _handle_word() noexcept {
            return _s;
        }

        const tracked_ptr<State>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<State> _s;
    };
}
