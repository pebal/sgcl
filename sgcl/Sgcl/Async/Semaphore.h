//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// n permits, the three forms of a wait a Channel has (sgcl::semaphore)
#pragma once

#include "../../async/semaphore.h"

#include <cstddef>
#include <utility>

namespace Sgcl {
    // n permits: Acquire() takes one, Release() gives one back
    class Semaphore {
    public:
        using InnerType = sgcl::semaphore;

        explicit Semaphore(size_t permits, size_t max = 0)
        : _s(permits, max) {
        }

        Semaphore(const Semaphore&) = delete;
        Semaphore& operator=(const Semaphore&) = delete;

        void Acquire() {
            _s.acquire();
        }

        bool TryAcquire() {
            return _s.try_acquire();
        }

        void Release() {
            _s.release();
        }

        auto AsyncAcquire() noexcept {
            return _s.async_acquire();
        }

        template<class F>
        auto OnAcquire(F f) {
            return _s.on_acquire(std::move(f));
        }

        size_t Available() const noexcept {
            return _s.available();
        }

        InnerType& Inner() noexcept {
            return _s;
        }

    private:
        InnerType _s;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
