//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "detail/slot.h"
#include "make_tracked.h"
#include "root_ptr.h"

#include <functional>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sgcl {
    namespace detail {
        // What a thread runs: the callable and the arguments, copied as
        // std::thread copies them (decayed), called once as rvalues
        template<class F, class... Args>
        struct ThreadClosure {
            F f;
            std::tuple<Args...> args;

            template<class G, class... A>
            ThreadClosure(G&& g, A&&... a)
                : f(std::forward<G>(g))
                , args(std::forward<A>(a)...) {
            }

            void operator()() {
                std::apply([this](Args&... a) { std::invoke(std::move(f), std::move(a)...); }, args);
            }
        };
    }

    // The thread of the standard library with the closure kept where a
    // tracked_ptr may live. std::thread copies the callable and its
    // arguments to the unmanaged heap, where a tracked_ptr among them is
    // seen by no one (README: The rules, 1), so a program had to capture
    // by reference or hand a root_ptr over. Here the callable and the
    // arguments go into a managed node of their own (a Slot, as function
    // keeps its closure: detail/value_storage.h), and what travels through
    // std::thread's state is a root_ptr to the node, which lives anywhere:
    // the node is a root from the constructor to the end of the function,
    // with no window in which the new thread has not taken it yet. The
    // new thread calls the closure in the node and destroys it the moment
    // the call returns, as a function drops its closure; the node goes to
    // the collector, the cell with std::thread's state. Otherwise the
    // standard's thread: join, detach, joinable, get_id, native_handle,
    // swap, hardware_concurrency, the destructor that ends the program on
    // a thread still joinable; the exception a function lets out ends the
    // program as it does with std::thread. What it costs beside
    // std::thread: one managed node and one root cell per thread started.
    class thread {
        std::thread _thread;

        template<class F>
        static constexpr bool NotThread = !std::is_same_v<std::remove_cvref_t<F>, thread>;

    public:
        using id = std::thread::id;
        using native_handle_type = std::thread::native_handle_type;

        thread() noexcept = default;

        thread(const thread&) = delete;
        thread& operator=(const thread&) = delete;

        thread(thread&& o) noexcept = default;

        thread& operator=(thread&& o) noexcept {
            _thread = std::move(o._thread);   // ends the program when this one is joinable, as std does
            return *this;
        }

        template<class F, class... Args>
        requires NotThread<F> && std::is_invocable_v<std::decay_t<F>, std::decay_t<Args>...>
        explicit thread(F&& f, Args&&... args) {
            using Closure = detail::ThreadClosure<std::decay_t<F>, std::decay_t<Args>...>;
            using Node = detail::Slot<Closure>;
            static_assert(sizeof(Node) <= detail::PageDataSize, "a closure larger than a page is not supported");
            // The node made and the closure constructed in it first: a
            // constructor that throws leaves nothing behind but the node,
            // let go by its unique_ptr (slot.h)
            unique_ptr<Node> node = make_tracked<Node>();
            node->construct(std::forward<F>(f), std::forward<Args>(args)...);
            root_ptr<Node> root(std::move(node));
            _thread = std::thread([root = std::move(root)] {
                root->value();
                root->destroy();
            });
        }

        ~thread() = default;

        void swap(thread& o) noexcept {
            _thread.swap(o._thread);
        }

        bool joinable() const noexcept {
            return _thread.joinable();
        }

        void join() {
            _thread.join();
        }

        void detach() {
            _thread.detach();
        }

        id get_id() const noexcept {
            return _thread.get_id();
        }

        native_handle_type native_handle() {
            return _thread.native_handle();
        }

        static unsigned hardware_concurrency() noexcept {
            return std::thread::hardware_concurrency();
        }
    };

    inline void swap(thread& a, thread& b) noexcept {
        a.swap(b);
    }
}
