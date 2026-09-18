//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Set once, waited for by any number, the three forms of a wait a
// Channel has (sgcl::event)
#pragma once

#include "../../async/event.h"

#include <utility>

namespace Sgcl {
    // Set once, waited for by any number; a wait after the set does not wait
    class Event {
    public:
        using InnerType = sgcl::event;

        Event() = default;
        Event(const Event&) = delete;
        Event& operator=(const Event&) = delete;

        void Set() {
            _e.set();
        }

        bool IsSet() const noexcept {
            return _e.is_set();
        }

        void Wait() {
            _e.wait();
        }

        auto AsyncWait() noexcept {
            return _e.async_wait();
        }

        template<class F>
        auto OnSet(F f) {
            return _e.on_set(std::move(f));
        }

        InnerType& Inner() noexcept {
            return _e;
        }

    private:
        InnerType _e;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
