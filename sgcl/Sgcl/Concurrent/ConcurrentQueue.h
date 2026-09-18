//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ConcurrentQueue<T> and ConcurrentStack<T>: lock-free, shared by any number of
// threads, no node reused while a thread holds it (the collector's
// doing). Dequeue and Pop wait for an element; TryDequeue and TryPop
// hand back nothing when there is none at the moment.
#pragma once

#include "../../concurrent/concurrent_queue.h"
#include "../../concurrent/concurrent_stack.h"
#include "../Core/Types.h"

namespace Sgcl {
    template<class T>
    class ConcurrentQueue {
    public:
        using ValueType = T;
        using InnerType = sgcl::concurrent_queue<T>;
        using SizeType = size_t;

        ConcurrentQueue() = default;
        ConcurrentQueue(const ConcurrentQueue&) = delete;
        ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;

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

        // The first element, waiting for one when the queue is empty
        T Dequeue() {
            return _q.pop();
        }

        // The first element, or nothing when the queue is empty now
        Optional<T> TryDequeue() {
            return _q.try_pop();
        }

        bool IsEmpty() const noexcept {
            return _q.empty();
        }

        SizeType Count() const noexcept {
            return _q.size();
        }

        void Clear() noexcept {
            _q.clear();
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

    template<class T>
    class ConcurrentStack {
    public:
        using ValueType = T;
        using InnerType = sgcl::concurrent_stack<T>;
        using SizeType = size_t;

        ConcurrentStack() = default;
        ConcurrentStack(const ConcurrentStack&) = delete;
        ConcurrentStack& operator=(const ConcurrentStack&) = delete;

        void Push(const T& value) {
            _s.push(value);
        }

        void Push(T&& value) {
            _s.push(std::move(value));
        }

        template<class... A>
        void Emplace(A&&... a) {
            _s.emplace(std::forward<A>(a)...);
        }

        // The top element, waiting for one when the stack is empty
        T Pop() {
            return _s.pop();
        }

        // The top element, or nothing when the stack is empty now
        Optional<T> TryPop() {
            return _s.try_pop();
        }

        bool IsEmpty() const noexcept {
            return _s.empty();
        }

        SizeType Count() const noexcept {
            return _s.size();
        }

        void Clear() noexcept {
            _s.clear();
        }

        InnerType& Inner() noexcept {
            return _s;
        }

        const InnerType& Inner() const noexcept {
            return _s;
        }

    private:
        InnerType _s;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

