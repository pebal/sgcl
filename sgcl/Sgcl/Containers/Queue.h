//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The adapters: Queue<T, Container> (first in, first out, over a Deque by
// default), PriorityQueue<T, Container, Compare> (the greatest first, a
// heap over a List by default) and Stack<T, Container> (last in, first
// out, over a Deque by default). The container is any of the interface's
// sequences with the operations the adapter needs. Dequeue and Pop hand
// the element back, where std only removes it; TryDequeue, TryPop and
// TryPeek hand back nothing on an empty adapter, where Dequeue, Pop and
// Peek must not be called.
#pragma once

#include "../../containers/queue.h"
#include "../../containers/stack.h"
#include "../Core/Types.h"
#include "Deque.h"
#include "List.h"

namespace Sgcl {
    template<class T, class Container = Deque<T>>
    class Queue {
    public:
        using ValueType = T;
        using ContainerType = Container;
        using InnerType = sgcl::queue<T, typename Container::InnerType>;
        using SizeType = size_t;

        Queue() = default;

        explicit Queue(const Container& c)
        : _q(c.Inner()) {
        }

        explicit Queue(Container&& c)
        : _q(std::move(c.Inner())) {
        }

        template<std::input_iterator It>
        Queue(It first, It last)
        : _q(first, last) {
        }

        Queue(std::initializer_list<T> il)
        : _q(il.begin(), il.end()) {
        }

        explicit Queue(InnerType q) noexcept
        : _q(std::move(q)) {
        }

        Queue(const Queue&) = default;
        Queue(Queue&&) noexcept = default;
        Queue& operator=(const Queue&) = default;
        Queue& operator=(Queue&&) noexcept = default;

        SizeType Count() const noexcept {
            return _q.size();
        }

        bool IsEmpty() const noexcept {
            return _q.empty();
        }

        void Enqueue(const T& value) {
            _q.push(value);
        }

        void Enqueue(T&& value) {
            _q.push(std::move(value));
        }

        template<class... A>
        decltype(auto) Emplace(A&&... a) {
            return _q.emplace(std::forward<A>(a)...);
        }

        // The first element, out of the queue
        T Dequeue() {
            T v = std::move(_q.front());
            _q.pop();
            return v;
        }

        // The same, or None when the queue is empty
        Optional<T> TryDequeue() {
            if (_q.empty()) {
                return None;
            }
            return Dequeue();
        }

        // The first element, still in the queue
        T& Peek() noexcept {
            return _q.front();
        }

        const T& Peek() const noexcept {
            return _q.front();
        }

        // A pointer to it, null when the queue is empty
        T* TryPeek() noexcept {
            return _q.empty() ? nullptr : &_q.front();
        }

        const T* TryPeek() const noexcept {
            return _q.empty() ? nullptr : &_q.front();
        }

        // The last element: the one enqueued most recently
        T& Last() noexcept {
            return _q.back();
        }

        const T& Last() const noexcept {
            return _q.back();
        }

        void Clear() noexcept {
            _q = InnerType();
        }

        void Swap(Queue& o) noexcept {
            _q.swap(o._q);
        }

        InnerType& Inner() noexcept {
            return _q;
        }

        const InnerType& Inner() const noexcept {
            return _q;
        }

        friend bool operator==(const Queue& a, const Queue& b) {
            return a._q == b._q;
        }

        friend auto operator<=>(const Queue& a, const Queue& b) {
            return a._q <=> b._q;
        }

    private:
        InnerType _q;
    };

    template<class T, class C>
    void swap(Queue<T, C>& a, Queue<T, C>& b) noexcept {
        a.Swap(b);
    }

    template<class T, class Container = List<T>, class Compare = std::less<T>>
    class PriorityQueue {
    public:
        using ValueType = T;
        using ContainerType = Container;
        using CompareType = Compare;
        using InnerType = sgcl::priority_queue<T, typename Container::InnerType, Compare>;
        using SizeType = size_t;

        PriorityQueue() = default;

        explicit PriorityQueue(const Compare& cmp)
        : _q(cmp) {
        }

        PriorityQueue(const Compare& cmp, const Container& c)
        : _q(cmp, c.Inner()) {
        }

        PriorityQueue(const Compare& cmp, Container&& c)
        : _q(cmp, std::move(c.Inner())) {
        }

        template<std::input_iterator It>
        PriorityQueue(It first, It last, const Compare& cmp = Compare())
        : _q(first, last, cmp) {
        }

        PriorityQueue(std::initializer_list<T> il, const Compare& cmp = Compare())
        : _q(il.begin(), il.end(), cmp) {
        }

        explicit PriorityQueue(InnerType q) noexcept
        : _q(std::move(q)) {
        }

        PriorityQueue(const PriorityQueue&) = default;
        PriorityQueue(PriorityQueue&&) noexcept = default;
        PriorityQueue& operator=(const PriorityQueue&) = default;
        PriorityQueue& operator=(PriorityQueue&&) noexcept = default;

        SizeType Count() const noexcept {
            return _q.size();
        }

        bool IsEmpty() const noexcept {
            return _q.empty();
        }

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

        // The greatest element, out of the queue
        T Dequeue() {
            T v = std::move(const_cast<T&>(_q.top()));
            _q.pop();
            return v;
        }

        // The same, or None when the queue is empty
        Optional<T> TryDequeue() {
            if (_q.empty()) {
                return None;
            }
            return Dequeue();
        }

        // The greatest element, still in the queue
        const T& Peek() const noexcept {
            return _q.top();
        }

        // A pointer to it, null when the queue is empty
        const T* TryPeek() const noexcept {
            return _q.empty() ? nullptr : &_q.top();
        }

        void Clear() noexcept {
            _q = InnerType();
        }

        void Swap(PriorityQueue& o) noexcept {
            _q.swap(o._q);
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

    template<class T, class C, class Cmp>
    void swap(PriorityQueue<T, C, Cmp>& a, PriorityQueue<T, C, Cmp>& b) noexcept {
        a.Swap(b);
    }

    template<class T, class Container = Deque<T>>
    class Stack {
    public:
        using ValueType = T;
        using ContainerType = Container;
        using InnerType = sgcl::stack<T, typename Container::InnerType>;
        using SizeType = size_t;

        Stack() = default;

        explicit Stack(const Container& c)
        : _s(c.Inner()) {
        }

        explicit Stack(Container&& c)
        : _s(std::move(c.Inner())) {
        }

        template<std::input_iterator It>
        Stack(It first, It last)
        : _s(first, last) {
        }

        Stack(std::initializer_list<T> il)
        : _s(il.begin(), il.end()) {
        }

        explicit Stack(InnerType s) noexcept
        : _s(std::move(s)) {
        }

        Stack(const Stack&) = default;
        Stack(Stack&&) noexcept = default;
        Stack& operator=(const Stack&) = default;
        Stack& operator=(Stack&&) noexcept = default;

        SizeType Count() const noexcept {
            return _s.size();
        }

        bool IsEmpty() const noexcept {
            return _s.empty();
        }

        void Push(const T& value) {
            _s.push(value);
        }

        void Push(T&& value) {
            _s.push(std::move(value));
        }

        template<class... A>
        decltype(auto) Emplace(A&&... a) {
            return _s.emplace(std::forward<A>(a)...);
        }

        // The top element, off the stack
        T Pop() {
            T v = std::move(_s.top());
            _s.pop();
            return v;
        }

        // The same, or None when the stack is empty
        Optional<T> TryPop() {
            if (_s.empty()) {
                return None;
            }
            return Pop();
        }

        // The top element, still on the stack
        T& Peek() noexcept {
            return _s.top();
        }

        const T& Peek() const noexcept {
            return _s.top();
        }

        // A pointer to it, null when the stack is empty
        T* TryPeek() noexcept {
            return _s.empty() ? nullptr : &_s.top();
        }

        const T* TryPeek() const noexcept {
            return _s.empty() ? nullptr : &_s.top();
        }

        void Clear() noexcept {
            _s = InnerType();
        }

        void Swap(Stack& o) noexcept {
            _s.swap(o._s);
        }

        InnerType& Inner() noexcept {
            return _s;
        }

        const InnerType& Inner() const noexcept {
            return _s;
        }

        friend bool operator==(const Stack& a, const Stack& b) {
            return a._s == b._s;
        }

        friend auto operator<=>(const Stack& a, const Stack& b) {
            return a._s <=> b._s;
        }

    private:
        InnerType _s;
    };

    template<class T, class C>
    void swap(Stack<T, C>& a, Stack<T, C>& b) noexcept {
        a.Swap(b);
    }

    // Deduction from the container given, or a range
    template<class Container>
    Queue(Container) -> Queue<typename Container::ValueType, Container>;
    template<class It>
    Queue(It, It) -> Queue<std::iter_value_t<It>>;
    template<class Container>
    Stack(Container) -> Stack<typename Container::ValueType, Container>;
    template<class It>
    Stack(It, It) -> Stack<std::iter_value_t<It>>;
    template<class Compare, class Container>
    PriorityQueue(Compare, Container) -> PriorityQueue<typename Container::ValueType, Container, Compare>;
    template<class It, class Compare = std::less<std::iter_value_t<It>>>
    PriorityQueue(It, It, Compare = Compare()) -> PriorityQueue<std::iter_value_t<It>, List<std::iter_value_t<It>>, Compare>;
    template<class It, class Compare, class Container>
    PriorityQueue(It, It, Compare, Container) -> PriorityQueue<typename Container::ValueType, Container, Compare>;
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

