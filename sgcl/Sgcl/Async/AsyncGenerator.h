//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// AsyncGenerator<T>: a generator that may wait, co_yields values and
// co_awaits between them, consumed from a task with
// `while (auto v = co_await g.Next())`.
#pragma once

#include "../../async/async_generator.h"
#include "Coroutine.h"

#include <utility>

namespace Sgcl {
    // A generator that may wait: co_yields values and co_awaits between
    // them, consumed from a task with `while (auto v = co_await g.Next())`
    template<class T>
    class AsyncGenerator {
    public:
        using ValueType = T;
        using InnerType = sgcl::async_generator<T>;
        using promise_type = typename InnerType::promise_type;

        AsyncGenerator() noexcept = default;
        AsyncGenerator(AsyncGenerator&&) noexcept = default;
        AsyncGenerator& operator=(AsyncGenerator&&) noexcept = default;
        AsyncGenerator(const AsyncGenerator&) = delete;
        AsyncGenerator& operator=(const AsyncGenerator&) = delete;

        AsyncGenerator(InnerType&& g) noexcept
        : _g(std::move(g)) {
        }

        // `co_await g.Next()`: the next value, or None at the end; rethrows
        auto Next() noexcept {
            return _g.next();
        }

        bool IsDone() const noexcept {
            return _g.done();
        }

        InnerType& Inner() noexcept {
            return _g;
        }

        const InnerType& Inner() const noexcept {
            return _g;
        }

    private:
        InnerType _g;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
