//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ConcurrentPriorityQueue<T, Compare>: a lock-free priority queue over the
// skip list, the least element first (Java's PriorityBlockingQueue), shared
// by any number of threads. Dequeue waits for an element; TryDequeue and
// TryPeek hand back nothing when there is none at the moment.
#pragma once

#include "../../concurrent/concurrent_priority_queue.h"
#include "../Core/Types.h"

namespace Sgcl {
    template<class T, class Compare = std::less<T>>
    class ConcurrentPriorityQueue {
    public:
        using ValueType = T;
        using InnerType = sgcl::concurrent_priority_queue<T, Compare>;
        using SizeType = size_t;

        ConcurrentPriorityQueue() = default;

        explicit ConcurrentPriorityQueue(const Compare& cmp)
        : _q(cmp) {
        }

        template<std::input_iterator It>
        ConcurrentPriorityQueue(It first, It last, const Compare& cmp = Compare())
        : _q(first, last, cmp) {
        }

        ConcurrentPriorityQueue(std::initializer_list<T> il, const Compare& cmp = Compare())
        : _q(il, cmp) {
        }

        ConcurrentPriorityQueue(const ConcurrentPriorityQueue&) = delete;
        ConcurrentPriorityQueue& operator=(const ConcurrentPriorityQueue&) = delete;

        void Enqueue(const T& value) {
            _q.push(value);
        }

        void Enqueue(T&& value) {
            _q.push(std::move(value));
        }

        template<class... A>
        void Emplace(A&&... a) {
            _q.emplace(std::forward<A>(a)...);
        }

        // The least element, waiting for one when the queue is empty
        T Dequeue() {
            return _q.pop();
        }

        // The least element, or nothing when the queue is empty now
        Optional<T> TryDequeue() {
            return _q.try_pop();
        }

        // A copy of the least element, or nothing when the queue is empty now
        Optional<T> TryPeek() const {
            return _q.try_top();
        }

        bool IsEmpty() const noexcept {
            return _q.empty();
        }

        SizeType Count() const noexcept {
            return _q.size();
        }

        void Clear() {
            _q.clear();
        }

        Compare Comparer() const {
            return _q.value_comp();
        }

        InnerType& Inner() noexcept {
            return _q;
        }

        const InnerType& Inner() const noexcept {
            return _q;
        }

    private:
        InnerType _q;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
