//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "channel.h"
#include "coroutine.h"

#include <atomic>
#include <utility>

namespace sgcl {
    // A once: the first caller runs the function, the others wait for it
    // to finish; `co_await o.async_call(t)` runs the task t once
    class once {
    public:
        once() = default;
        once(const once&) = delete;
        once& operator=(const once&) = delete;

        template<class F>
        void call(F f) {
            if (_claim()) {
                f();
                _done.close();
            } else {
                _done.receive();
            }
        }

        template<class T>
        task<> async_call(task<T> t) {
            if (_claim()) {
                co_await t;
                _done.close();
            } else {
                co_await _done.async_receive();
            }
        }

        bool called() const noexcept {
            return _done.closed();
        }

    private:
        bool _claim() noexcept {
            int e = 0;
            return _state.compare_exchange_strong(e, 1, std::memory_order_acq_rel, std::memory_order_acquire);
        }

        std::atomic<int> _state = {0};
        channel<void> _done;
    };
}
