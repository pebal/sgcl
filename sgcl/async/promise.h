//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "channel.h"
#include "coroutine.h"

#include <atomic>
#include <cassert>
#include <exception>
#include <utility>

namespace sgcl {
    // A one-shot completion: a value (or an exception) that one side sets
    // once and any number of others wait for, the adapter between the
    // callback APIs of a platform (an I/O completion port, a dispatch
    // queue, JNI, a driver's completion routine, a C library with a
    // `void*` context) and `co_await`. The thread that has the result
    // calls set_value(v) or set_exception(e) and returns at once; a task
    // that waits writes `co_await p` and holds no thread until then; a
    // thread writes p.get() and blocks; a select takes p.on_ready(f) as a
    // case (select.h). Under it an event (event.h) with a value: a channel
    // of signals closed by the set, so that the three forms of the wait
    // are the channel's, lock-free, the waiters reclaimed by the
    // collector. No shared state and no std::future: the promise is one
    // object, the value lives in it, and a waiter reads it there; a
    // tracked_ptr value keeps its object as any member would. Not the
    // shape of std::promise (a promise and a future apart), on purpose:
    // a completion has one home, and whoever has a pointer to it may set
    // it or wait for it.
    //
    // Exactly one setter wins: the first set_value or set_exception
    // claims the promise with a compare-exchange and stores; a second is
    // an error (asserted in debug builds; ignored in release, the first
    // value stands). The promise lives where a tracked_ptr may (on a
    // stack or in a managed object) and is neither copied nor moved; a
    // foreign thread's callback holds it through a root_ptr (a `void*`
    // context) or a tracked_ptr captured in a managed object.
    namespace detail {
        // The state and the waiting, shared by promise<T> and promise<void>
        class PromiseBase {
        public:
            PromiseBase() = default;
            PromiseBase(const PromiseBase&) = delete;
            PromiseBase& operator=(const PromiseBase&) = delete;

            // Whether the value or the exception is in
            bool ready() const noexcept {
                return _done.closed();
            }

            // A case of a select: f() once the promise is ready
            template<class F>
            auto on_ready(F f) {
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

            void _wait() {
                _done.receive();
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

                explicit await_op(channel<void>& ch) noexcept
                : _op(ch.async_receive()) {
                }

                decltype(std::declval<channel<void>&>().async_receive()) _op;
            };

            await_op _await() noexcept {
                return await_op(_done);
            }

            std::exception_ptr _error;

        private:
            enum State : int { Pending, Claimed };

            std::atomic<int> _state = {Pending};
            channel<void> _done;
        };
    }

    template<class T = void>
    class promise : public detail::PromiseBase {
    public:
        using value_type = T;

        promise() = default;

        // The value in, the waiters woken; the first setter only
        void set_value(const T& v) {
            _set(T(v));
        }

        void set_value(T&& v) {
            _set(T(std::move(v)));
        }

        void set_exception(std::exception_ptr e) {
            if (_claim()) {
                _error = std::move(e);
                _publish();
            }
        }

        // The value, once it is in, or what was set as the exception,
        // rethrown; a reference into the promise, so that the value has
        // one home whoever reads it (a lone reader may move it out)
        T& result() {
            if (_error) {
                std::rethrow_exception(_error);
            }
            return *_value;
        }

        // Waits for the value, on this thread; not from a task on a
        // worker (co_await it there)
        T& get() {
            assert(!detail::on_worker() && "get() blocks the worker: co_await the promise from a task");
            _wait();
            return result();
        }

        // `co_await p`: the task suspended until the promise is ready, no
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
            friend class promise;

            explicit awaiter(promise& p) noexcept
            : _p(&p)
            , _op(p._await()) {
            }

            promise* _p;
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
    class promise<void> : public detail::PromiseBase {
    public:
        using value_type = void;

        promise() = default;

        void set_value() {
            if (_claim()) {
                _publish();
            }
        }

        void set_exception(std::exception_ptr e) {
            if (_claim()) {
                _error = std::move(e);
                _publish();
            }
        }

        // Rethrows what was set as the exception, if anything
        void result() {
            if (_error) {
                std::rethrow_exception(_error);
            }
        }

        void get() {
            assert(!detail::on_worker() && "get() blocks the worker: co_await the promise from a task");
            _wait();
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
            friend class promise;

            explicit awaiter(promise& p) noexcept
            : _p(&p)
            , _op(p._await()) {
            }

            promise* _p;
            await_op _op;
        };

        awaiter operator co_await() noexcept {
            return awaiter(*this);
        }
    };
}
