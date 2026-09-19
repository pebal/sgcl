//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Counts work down to zero, the three forms of a wait a Channel has
// (sgcl::wait_group)
#pragma once

#include "../../async/wait_group.h"
#include "Coroutine.h"
#include "Scheduler.h"

#include <utility>

namespace Sgcl {
    // Add(n) counts the work, Done() counts it off, Wait() waits for zero
    class WaitGroup {
    public:
        using InnerType = sgcl::wait_group;

        WaitGroup() = default;
        WaitGroup(const WaitGroup&) = delete;
        WaitGroup& operator=(const WaitGroup&) = delete;

        void Add(long n = 1) noexcept {
            _g.add(n);
        }

        void Done() {
            _g.done();
        }

        long Count() const noexcept {
            return _g.count();
        }

        void Wait() {
            _g.wait();
        }

        // `co_await g.AsyncWait()`: the task resumed at zero
        Task<> AsyncWait() {
            return _g.async_wait();   // the inner task as a Task: no frame of its own (Timeout.h)
        }

        template<class F>
        auto OnDone(F f) {
            return _g.on_done(std::move(f));
        }

        InnerType& Inner() noexcept {
            return _g;
        }

    private:
        InnerType _g;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
