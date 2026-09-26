//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "channel.h"
#include "operation.h"
#include "coroutine.h"

#include <cassert>
#include <coroutine>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
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

        // Waits for the set: `e.wait()` on a thread, `co_await e` in a
        // task, as a task is waited for
        void wait() {
            assert(!detail::on_worker() && "wait() blocks the worker: co_await the event from a task");
            (void)_ch.receive().wait();
        }

        class wait_op;
        wait_op operator co_await();

        // The awaiter of `co_await e`: the task resumed by the set
        class wait_op {
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

            explicit wait_op(channel<void>& ch) noexcept
            : _op(ch.receive()) {
            }

            decltype(std::declval<channel<void>&>().receive()) _op;
        };

        template<class F>
        auto on_set(F f) {
            return _ch.on_receive(std::move(f));
        }

    private:
        channel<void> _ch;
    };

    inline event::wait_op event::operator co_await() {
        return wait_op(_ch);
    }
}
