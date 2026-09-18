//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "channel.h"
#include "coroutine.h"

#include <coroutine>
#include <utility>

namespace sgcl {
    // An event: set once, waited for by any number; a wait after the set does not wait
    class event {
    public:
        event() = default;
        event(const event&) = delete;
        event& operator=(const event&) = delete;

        void set() {
            _ch.close();
        }

        bool is_set() const noexcept {
            return _ch.closed();
        }

        void wait() {
            _ch.receive();
        }

        // `co_await e.async_wait()`: the task resumed by the set
        class async_wait_op {
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
            friend class event;

            explicit async_wait_op(channel<void>& ch) noexcept
            : _op(ch.async_receive()) {
            }

            decltype(std::declval<channel<void>&>().async_receive()) _op;
        };

        async_wait_op async_wait() noexcept {
            return async_wait_op(_ch);
        }

        template<class F>
        auto on_set(F f) {
            return _ch.on_receive(std::move(f));
        }

    private:
        channel<void> _ch;
    };
}
