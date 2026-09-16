//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "atomic.h"
#include "config.h"
#include "detail/backoff.h"
#include "make_tracked.h"
#include "tracked_ptr.h"

#include <utility>

namespace sgcl {
    // A lock-free stack shared by any number of threads: the Treiber stack,
    // a compare-exchange on the head and nothing else, the textbook
    // algorithm as it is written for a runtime with a collector (Java's
    // ConcurrentLinkedDeque at one end). What makes it that short here is
    // what the collector guarantees: a node is never reused while a thread
    // holds it, so there is no ABA, no hazard pointer to publish, no epoch
    // to enter and no reclamation scheme of any kind in this file. The
    // container holds one word, the atomic head; it lives where a
    // tracked_ptr may (on a stack or inside a managed object). The nodes
    // are managed objects, linked
    // by plain tracked_ptrs: a node's link is written once, before the node
    // is published by the compare-exchange, and read after an acquire load
    // of the head. An element is moved out of its node by the thread that
    // popped it and destroyed there and then, as std::stack's pop destroys
    // it; the node is garbage from that moment and the collector reclaims
    // it. Every operation is lock-free; push and try_pop are linearizable
    // at their compare-exchange. pop() blocks on an empty stack, on the
    // atomic's wait, and every push notifies. A failed compare-exchange
    // backs off before the retry, exponentially up to config::BackoffMax
    // pauses (the backoff stack of Herlihy and Shavit): on one contended
    // word the retries of many threads otherwise cost more than the
    // operations, and the backoff turns the storm back into a queue of
    // near-serial exchanges.
    template<class T>
    class concurrent_stack {
        struct Node {
            template<class... A>
            explicit Node(std::in_place_t, A&&... a)
            : value(std::in_place, std::forward<A>(a)...) {
            }

            tracked_ptr<Node> next;
            optional<T> value;   // disengaged once popped: the node dies without an element
        };

    public:
        using value_type = T;
        using size_type = size_t;

        concurrent_stack() noexcept = default;
        concurrent_stack(const concurrent_stack&) = delete;
        concurrent_stack& operator=(const concurrent_stack&) = delete;

        void push(const T& value) {
            emplace(value);
        }

        void push(T&& value) {
            emplace(std::move(value));
        }

        template<class... A>
        void emplace(A&&... a) {
            tracked_ptr<Node> node = make_tracked<Node>(std::in_place, std::forward<A>(a)...);
            node->next = _head.load(std::memory_order_relaxed);
            detail::Backoff backoff;
            while (!_head.compare_exchange_weak(node->next, node, std::memory_order_release, std::memory_order_relaxed)) {
                backoff();
            }
            _head.notify_one();
        }

        // The top element, or nothing when the stack is empty at the
        // moment of the load.
        optional<T> try_pop() {
            tracked_ptr<Node> node = _head.load(std::memory_order_acquire);
            detail::Backoff backoff;
            while (node && !_head.compare_exchange_weak(node, node->next, std::memory_order_acquire, std::memory_order_acquire)) {
                backoff();
            }
            if (!node) {
                return nullopt;
            }
            optional<T> value(std::in_place, std::move(*node->value));
            node->value.reset();
            return value;
        }

        // The top element, waiting for one when the stack is empty
        T pop() {
            for (;;) {
                if (auto value = try_pop()) {
                    return std::move(*value);
                }
                _head.wait(nullptr, std::memory_order_acquire);
            }
        }

        bool empty() const noexcept {
            return !_head.load(std::memory_order_acquire);
        }

        // The number of elements at some moment of the walk: a count of
        // the nodes, linear, as std::forward_list::size would be
        size_type size() const noexcept {
            size_type n = 0;
            for (tracked_ptr<Node> node = _head.load(std::memory_order_acquire); node; node = node->next) {
                ++n;
            }
            return n;
        }

        // Pops every element there is: the head is taken whole and the
        // elements behind it are destroyed here, the nodes reclaimed by
        // the collector
        void clear() noexcept {
            tracked_ptr<Node> node = _head.load(std::memory_order_acquire);
            while (node && !_head.compare_exchange_weak(node, nullptr, std::memory_order_acquire, std::memory_order_acquire)) {
            }
            for (; node; node = node->next) {
                node->value.reset();
            }
        }

    private:
        atomic<tracked_ptr<Node>> _head;
    };
}
