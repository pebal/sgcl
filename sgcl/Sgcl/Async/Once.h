//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The first caller runs it, the others wait for it, blocking or
// awaitable (sgcl::once)
#pragma once

#include "../../async/once.h"
#include "Coroutine.h"
#include "Scheduler.h"

#include <utility>

namespace Sgcl {
    // The first caller runs the function, the others wait for it to finish
    class Once {
    public:
        using InnerType = sgcl::once;

        Once() = default;
        Once(const Once&) = delete;
        Once& operator=(const Once&) = delete;

        template<class F>
        void Call(F f) {
            _o.call(std::move(f));
        }

        // `co_await o.AsyncCall(t)`: the task t run once
        template<class T>
        Task<> AsyncCall(Task<T> t) {
            return _o.async_call(std::move(t.Inner()));
        }

        bool IsCalled() const noexcept {
            return _o.called();
        }

        InnerType& Inner() noexcept {
            return _o;
        }

    private:
        InnerType _o;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
