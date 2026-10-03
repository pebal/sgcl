//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "coroutine.h"
#include "select.h"
#include "stop_token.h"
#include "timer.h"

#include <concepts>
#include <type_traits>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // `async::every(d, f)`: f called every d, in a task of its own, until
    // the stop_source it returns is stopped — the loop over tick(d) with a
    // select on the stop, which a program would otherwise write itself.
    // The ticks are tick's: the first after d, a tick that comes while f
    // still runs held (one), the others dropped, so a slow f is called
    // less often, never in a backlog. A call that has started ends; none
    // starts after the stop is seen. The tick channel is closed when the
    // loop ends, by the stop or by an exception of f (which goes where a
    // go'ed task's goes: on_unhandled), so its timer lets go.
    namespace detail {
        // The tick channel closed when the loop's frame goes, however it goes
        struct EveryTicks {
            channel<void> ticks;

            ~EveryTicks() {
                ticks.close();
            }
        };

        template<class F>
        task<> every_loop(duration d, F f, stop_token stop) {   // f and stop by value: parameters live in the frame
            EveryTicks t{tick(d)};
            for (;;) {
                size_t served = co_await select(t.ticks.on_receive([] {}), stop.on_stop([] {}));
                if (served != 0 || stop.stop_requested()) {
                    co_return;
                }
                if constexpr (TaskFactory<F>) {
                    co_await f();
                } else {
                    f();
                }
            }
        }
    }

    // f called every d until the source returned is stopped:
    // `auto s = async::every(1s, [] { ... }); ... s.request_stop();`.
    // f is a function, called on a worker, or a coroutine function whose
    // task is awaited before the next tick. A period of zero or less
    // never ticks, as tick's
    template<class F>
    requires std::invocable<F&>
    stop_source every(duration d, F f) {
        stop_source s;
        go(detail::every_loop(d, std::move(f), s.token()));
        return s;
    }

    // The same, stopped also when parent is: the source returned is its child
    template<class F>
    requires std::invocable<F&>
    stop_source every(duration d, F f, const stop_token& parent) {
        stop_source s(parent);
        go(detail::every_loop(d, std::move(f), s.token()));
        return s;
    }
}
