//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Promise<T>: a one-shot completion any thread or C callback fulfils
// (SetValue, SetException, once) and a task awaits (`co_await p`), a
// thread blocks on (Get) or a Select takes as a case (OnReady): the
// adapter between a platform's callbacks and co_await. The value lives
// in the promise; a Ptr value keeps its object.
#pragma once

#include "../../async/promise.h"
#include "Coroutine.h"
#include "Scheduler.h"

#include <exception>
#include <utility>

namespace Sgcl {
    template<class T = void>
    class Promise {
    public:
        using ValueType = T;
        using InnerType = sgcl::promise<T>;

        Promise() = default;
        Promise(const Promise&) = delete;
        Promise& operator=(const Promise&) = delete;

        // The value in, the waiters woken; the first setter only (a
        // second is asserted in debug builds and ignored in release)
        void SetValue(const T& v) {
            _p.set_value(v);
        }

        void SetValue(T&& v) {
            _p.set_value(std::move(v));
        }

        void SetException(std::exception_ptr e) {
            _p.set_exception(std::move(e));
        }

        bool IsReady() const noexcept {
            return _p.ready();
        }

        // Waits for the value on this thread (a task co_awaits instead):
        // the value, or what was set as the exception rethrown
        decltype(auto) Get() {
            return _p.get();
        }

        // The value of a ready promise, or the exception rethrown
        decltype(auto) Result() {
            return _p.result();
        }

        // `co_await p`: the task suspended until the promise is ready
        auto operator co_await() noexcept {
            return _p.operator co_await();
        }

        // A case of a Select: f() once the promise is ready
        template<class F>
        auto OnReady(F f) {
            return _p.on_ready(std::move(f));
        }

        InnerType& Inner() noexcept {
            return _p;
        }

        const InnerType& Inner() const noexcept {
            return _p;
        }

    private:
        InnerType _p;
    };

    // A completion without a value
    template<>
    class Promise<void> {
    public:
        using ValueType = void;
        using InnerType = sgcl::promise<void>;

        Promise() = default;
        Promise(const Promise&) = delete;
        Promise& operator=(const Promise&) = delete;

        void SetValue() {
            _p.set_value();
        }

        void SetException(std::exception_ptr e) {
            _p.set_exception(std::move(e));
        }

        bool IsReady() const noexcept {
            return _p.ready();
        }

        void Get() {
            _p.get();
        }

        void Result() {
            _p.result();
        }

        auto operator co_await() noexcept {
            return _p.operator co_await();
        }

        template<class F>
        auto OnReady(F f) {
            return _p.on_ready(std::move(f));
        }

        InnerType& Inner() noexcept {
            return _p;
        }

        const InnerType& Inner() const noexcept {
            return _p;
        }

    private:
        InnerType _p;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
