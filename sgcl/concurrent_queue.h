//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "atomic.h"
#include "config.h"
#include "detail/managed.h"
#include "make_tracked.h"
#include "tracked_ptr.h"

#include <utility>

namespace sgcl {
    // An unbounded lock-free FIFO queue shared by any number of producers
    // and consumers: the Michael–Scott queue (1996) in the form Java's
    // ConcurrentLinkedQueue gives it, written the way it is written for a
    // runtime with a collector. The nodes form a singly linked list; the
    // head addresses a node at or before the first element, the tail a
    // node at or before the last one, and both lag on purpose: a push
    // links the new node after the last one with a compare-exchange on
    // that node's link and swings the tail only when it found the tail a
    // node or more behind, a pop claims the first element with a
    // compare-exchange on its node's flag and swings the head only when
    // the element was a node or more past it. That halves the exchanges
    // on the two words every thread contends for (Java's "hop two nodes
    // at a time"); a thread finding a word behind walks the links to
    // where it should be, so no thread ever waits for another. A node
    // the head has passed is linked to itself, the sign for a walk that
    // it left the list, and so that an old head a thread still holds
    // retains nothing behind it.
    // No ABA, no counted pointers, no hazard pointers in the algorithm
    // and no free list: a node is never reused while a thread holds it,
    // and a node nobody holds is reclaimed by the collector. The container
    // holds two words, the atomic head and tail; it lives where a
    // tracked_ptr may (on a stack or inside a managed object), and the
    // gc:: one (gc/gc.h) anywhere; the nodes are managed objects linked by
    // atomic tracked_ptrs. An element is destroyed by the thread that
    // popped it, as std::queue's pop destroys it. Every operation is
    // lock-free; push and try_pop are linearizable at their
    // compare-exchange on a link and on a node's flag. pop() blocks on an
    // empty queue, on the atomic link of the last node, and every push
    // notifies. The head and the tail are kept a cache line apart
    // (config::CacheLineSize).
    template<class V, template<class> class Ptr>
    class concurrent_queue {
        // What a node holds (vector.h, detail/managed.h): V, or the word V
        // names as its tracked_type; the interface is V
        using T = detail::managed_t<V>;

        struct Node {
            Node() noexcept = default;

            template<class... A>
            explicit Node(std::in_place_t, A&&... a)
            : value(std::in_place, std::forward<A>(a)...) {
            }

            struct Taken {};

            explicit Node(Taken) noexcept
            : taken(true) {
            }

            atomic<tracked_ptr<Node>> next;
            optional<T> value;                 // disengaged once taken
            atomic<bool> taken = {false};   // the claim on the element: one pop wins it
        };

    public:
        using value_type = V;
        using size_type = size_t;

        // An empty queue: the head and the tail address one node whose
        // element is taken
        concurrent_queue()
        : concurrent_queue(make_tracked<Node>(typename Node::Taken{})) {
        }

        concurrent_queue(const concurrent_queue&) = delete;
        concurrent_queue& operator=(const concurrent_queue&) = delete;

        void push(const V& value) {
            emplace(value);
        }

        void push(V&& value) {
            emplace(std::move(value));
        }

        template<class... A>
        void emplace(A&&... a) {
            tracked_ptr<Node> node = make_tracked<Node>(std::in_place, std::forward<A>(a)...);
            tracked_ptr<Node> tail = _tail.load(std::memory_order_acquire);
            tracked_ptr<Node> p = tail;
            for (;;) {
                tracked_ptr<Node> q = p->next.load(std::memory_order_acquire);
                if (!q) {   // p is the last node
                    if (p->next.compare_exchange_weak(q, node, std::memory_order_release, std::memory_order_relaxed)) {
                        if (p != tail) {   // two or more nodes past the tail: swing it; a failure is another push's success
                            _tail.compare_exchange_weak(tail, node, std::memory_order_release, std::memory_order_relaxed);
                        }
                        p->next.notify_all();   // a consumer waits on the link of the last node
                        return;
                    }
                    continue;   // q holds what was linked meanwhile: walk on from it
                }
                if (q == p) {   // p left the list (an old head): start again from the tail, or the head if the tail is p too
                    tracked_ptr<Node> t = _tail.load(std::memory_order_acquire);
                    if (t != tail) {
                        tail = t;
                        p = t;
                    } else {
                        p = _head.load(std::memory_order_acquire);
                    }
                    continue;
                }
                if (p != tail) {   // two hops in: look at the tail again before the next
                    tracked_ptr<Node> t = _tail.load(std::memory_order_acquire);
                    if (t != tail) {
                        tail = t;
                        p = t;
                        continue;
                    }
                }
                p = q;
            }
        }

        // The first element, or nothing when the queue is empty at the
        // moment of the walk
        optional<V> try_pop() {
            tracked_ptr<Node> last;
            return _pop(last);
        }

        // The first element, waiting for one when the queue is empty
        V pop() {
            for (;;) {
                tracked_ptr<Node> last;
                if (auto value = _pop(last)) {
                    return std::move(*value);
                }
                last->next.wait(nullptr, std::memory_order_acquire);   // the link a push notifies
            }
        }

        bool empty() const noexcept {
            return !_first();
        }

        // The number of elements at some moment of the walk: a count of
        // the nodes not taken, linear, as Java's is
        size_type size() const noexcept {
            size_type n = 0;
            for (tracked_ptr<Node> p = _first(); p; p = _after(p)) {
                if (!p->taken.load(std::memory_order_acquire)) {
                    ++n;
                }
            }
            return n;
        }

        // Pops every element there is
        void clear() noexcept {
            while (try_pop()) {
            }
        }

    private:
        explicit concurrent_queue(tracked_ptr<Node> first) noexcept
        : _head(first)
        , _tail(first) {
        }

        // The walk of Java's poll: from the head along the links to the
        // first element not taken, claimed with an exchange on its flag;
        // the head swung to it, or past it, once the walk went two nodes
        // or more. `last` is the last node of the list when nothing was
        // found: what pop() waits on.
        optional<V> _pop(tracked_ptr<Node>& last) {
        restart:
            tracked_ptr<Node> head = _head.load(std::memory_order_acquire);
            tracked_ptr<Node> p = head;
            for (;;) {
                if (!p->taken.load(std::memory_order_acquire)) {
                    bool expected = false;
                    if (p->taken.compare_exchange_strong(expected, true, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        optional<V> value(std::in_place, std::move(*p->value));
                        p->value.reset();
                        if (p != head) {
                            tracked_ptr<Node> q = p->next.load(std::memory_order_acquire);
                            _update_head(head, q ? q : p);
                        }
                        return value;
                    }
                }
                tracked_ptr<Node> q = p->next.load(std::memory_order_acquire);
                if (!q) {   // the last node, every element before it taken
                    if (p != head) {
                        _update_head(head, p);
                    }
                    last = p;
                    return nullopt;
                }
                if (q == p) {   // p left the list under this walk
                    goto restart;
                }
                p = q;
            }
        }

        // The head swung from h to p, and h linked to itself: off the
        // list, retaining nothing
        void _update_head(tracked_ptr<Node>& h, tracked_ptr<Node> p) noexcept {
            if (h != p && _head.compare_exchange_strong(h, p, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                h->next.store(h, std::memory_order_release);
            }
        }

        // The first node holding an element, or null; the node after p on
        // the list, or null (a self-linked node sends the walk back to
        // the head)
        tracked_ptr<Node> _first() const noexcept {
        restart:
            tracked_ptr<Node> p = _head.load(std::memory_order_acquire);
            for (;;) {
                if (!p->taken.load(std::memory_order_acquire)) {
                    return p;
                }
                tracked_ptr<Node> q = p->next.load(std::memory_order_acquire);
                if (!q) {
                    return q;
                }
                if (q == p) {
                    goto restart;
                }
                p = q;
            }
        }

        tracked_ptr<Node> _after(const tracked_ptr<Node>& p) const noexcept {
            tracked_ptr<Node> q = p->next.load(std::memory_order_acquire);
            return q == p ? _first() : q;
        }

        // The head and the tail a cache line apart: consumers exchange on
        // the one, producers on the other, and neither line bounces for
        // the other side's traffic. Padding rather than alignas, so that
        // the queue asks no alignment of the object it is a member of.
        atomic<Ptr<Node>> _head;
        unsigned char _pad[config::CacheLineSize - sizeof(atomic<Ptr<Node>>)] = {};
        atomic<Ptr<Node>> _tail;
    };
}
