//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "channel.h"
#include "coroutine.h"
#include "signal.h"
#include "stop_token.h"

#include <concepts>
#include <csignal>
#include <type_traits>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // The entry of a program: `int main() { return async::run(program()); }`
    // runs the task on the scheduler, waits for it on the calling thread
    // and gives its value back (what it threw, rethrown). Given the
    // coroutine function instead, `async::run([](async::stop_token stop)
    // -> async::task<int> { ... })`, run hands it a token that the first
    // Ctrl-C (SIGINT) or SIGTERM stops, the way Go's signal.NotifyContext
    // does: the program winds down on its own terms, and the next Ctrl-C
    // does what it did before run — by default ends the process, which
    // the shell reports as 130 (143 for SIGTERM). Nothing else registered
    // for the two signals is touched: the registration run makes is its
    // own and is dropped when the task is done.
    namespace detail {
        // run(f)'s watch of the signals: the first one drops the
        // registration (the disposition from before run back, unless
        // another channel is registered for the number) and stops the
        // source; the channel closed by run ends it without a signal
        inline task<> run_watch(channel<int> sig, stop_source src) {
            if (co_await sig.receive()) {
                signals_instance().forget(ChannelAccess::state(sig).get());
                src.request_stop();
            }
        }

        // The registration of run(f) dropped and its watch ended, however
        // run leaves (the task's value or its exception)
        class RunSignals {
        public:
            SGCL_INLINE_HOT explicit RunSignals(const channel<int>& sig) noexcept
            : _sig(sig) {
            }

            RunSignals(const RunSignals&) = delete;
            RunSignals& operator=(const RunSignals&) = delete;

            SGCL_INLINE_HOT ~RunSignals() {
                signals_instance().forget(ChannelAccess::state(_sig).get());
                _sig.close();
            }

        private:
            channel<int> _sig;
        };
    }

    // The task run on the scheduler and waited for on the calling thread:
    // its value, or what it threw. For a thread (main), never a task
    template<class T>
    SGCL_INLINE_HOT T run(task<T> t) {
        if constexpr (std::is_void_v<T>) {
            t.wait();
        } else {
            return std::move(t.wait());
        }
    }

    // The task f makes run the same way, f given a stop_token that the
    // first SIGINT or SIGTERM stops; the next one does what it did before
    // run (by default: the process ends, 130 in the shell)
    template<class F>
        requires std::invocable<F&, stop_token>
    SGCL_INLINE_HOT auto run(F&& f) {
        stop_source src;
        channel<int> sig = signals({SIGINT, SIGTERM});
        detail::RunSignals registered(sig);
        go(detail::run_watch(sig, src));
        return run(f(src.token()));
    }
}
