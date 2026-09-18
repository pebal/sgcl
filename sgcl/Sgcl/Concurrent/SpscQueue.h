//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// SpscQueue<T>: a wait-free ring of fixed capacity for exactly one
// producer thread and one consumer thread (Lamport's ring, each side
// caching the other's index). Enqueue waits for room and Dequeue for an
// element; TryEnqueue and TryDequeue hand back false or nothing when
// there is none at the moment.
#pragma once

#include "../../concurrent/spsc_queue.h"
#include "../Core/Types.h"

namespace Sgcl {
    template<class T>
    class SpscQueue {
    public:
        using ValueType = T;
        using InnerType = sgcl::spsc_queue<T>;
        using SizeType = size_t;

        // A queue of the capacity rounded up to a power of two
        explicit SpscQueue(SizeType capacity)
        : _q(capacity) {
        }

        SpscQueue(const SpscQueue&) = delete;
        SpscQueue& operator=(const SpscQueue&) = delete;

        // The element appended, or false when the queue is full
        bool TryEnqueue(const T& value) {
            return _q.try_push(value);
        }

        bool TryEnqueue(T&& value) {
            return _q.try_push(std::move(value));
        }

        template<class... A>
        bool TryEmplace(A&&... a) {
            return _q.try_emplace(std::forward<A>(a)...);
        }

        // The element appended, waiting for room while the queue is full
        void Enqueue(T value) {
            _q.push(std::move(value));
        }

        // The first element, waiting for one while the queue is empty
        T Dequeue() {
            return _q.pop();
        }

        // The first element, or nothing when the queue is empty
        Optional<T> TryDequeue() {
            return _q.try_pop();
        }

        bool IsEmpty() const noexcept {
            return _q.empty();
        }

        bool IsFull() const noexcept {
            return _q.full();
        }

        SizeType Count() const noexcept {
            return _q.size();
        }

        SizeType Capacity() const noexcept {
            return _q.capacity();
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
