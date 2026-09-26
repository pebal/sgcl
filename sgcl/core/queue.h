//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "deque.h"
#include "vector.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>

namespace sgcl {
    // std::queue and std::priority_queue over managed containers. The
    // containers hold tracked pointers, so an adapter lives where a
    // tracked_ptr may: on a thread's stack or inside a managed object.
    template<class T, class Container = deque<T>>
    class queue {
    public:
        using container_type = Container;
        using value_type = typename Container::value_type;
        using size_type = typename Container::size_type;
        using reference = typename Container::reference;
        using const_reference = typename Container::const_reference;

        queue()
        : queue(Container()) {
        }

        explicit queue(const Container& cont)
        : c(cont) {
        }

        explicit queue(Container&& cont)
        : c(std::move(cont)) {
        }

        template<std::input_iterator InputIt>
        queue(InputIt first, InputIt last)
        : c(first, last) {
        }

        reference front() {
            return c.front();
        }

        const_reference front() const {
            return c.front();
        }

        reference back() {
            return c.back();
        }

        const_reference back() const {
            return c.back();
        }

        bool empty() const {
            return c.empty();
        }

        size_type size() const {
            return c.size();
        }

        void push(const value_type& value) {
            c.push_back(value);
        }

        void push(value_type&& value) {
            c.push_back(std::move(value));
        }

        template<class... A>
        decltype(auto) emplace(A&&... a) {
            return c.emplace_back(std::forward<A>(a)...);
        }

        void pop() {
            c.pop_front();
        }

        void swap(queue& other) noexcept(std::is_nothrow_swappable_v<Container>) {
            using std::swap;
            swap(c, other.c);
        }

    protected:
        Container c;

        friend bool operator==(const queue& lhs, const queue& rhs) {
            return lhs.c == rhs.c;
        }

        friend auto operator<=>(const queue& lhs, const queue& rhs) {
            return lhs.c <=> rhs.c;
        }
    };

    template<class T, class Container>
    inline void swap(queue<T, Container>& lhs, queue<T, Container>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    template<class T, class Container = vector<T>, class Compare = std::less<typename Container::value_type>>
    class priority_queue {
    public:
        using container_type = Container;
        using value_compare = Compare;
        using value_type = typename Container::value_type;
        using size_type = typename Container::size_type;
        using reference = typename Container::reference;
        using const_reference = typename Container::const_reference;

        priority_queue()
        : priority_queue(Compare(), Container()) {
        }

        explicit priority_queue(const Compare& compare)
        : priority_queue(compare, Container()) {
        }

        priority_queue(const Compare& compare, const Container& cont)
        : c(cont)
        , comp(compare) {
            std::make_heap(c.begin(), c.end(), comp);
        }

        priority_queue(const Compare& compare, Container&& cont)
        : c(std::move(cont))
        , comp(compare) {
            std::make_heap(c.begin(), c.end(), comp);
        }

        template<std::input_iterator InputIt>
        priority_queue(InputIt first, InputIt last, const Compare& compare = Compare())
        : c(first, last)
        , comp(compare) {
            std::make_heap(c.begin(), c.end(), comp);
        }

        template<std::input_iterator InputIt>
        priority_queue(InputIt first, InputIt last, const Compare& compare, const Container& cont)
        : c(cont)
        , comp(compare) {
            c.insert(c.end(), first, last);
            std::make_heap(c.begin(), c.end(), comp);
        }

        template<std::input_iterator InputIt>
        priority_queue(InputIt first, InputIt last, const Compare& compare, Container&& cont)
        : c(std::move(cont))
        , comp(compare) {
            c.insert(c.end(), first, last);
            std::make_heap(c.begin(), c.end(), comp);
        }

        const_reference top() const {
            return c.front();
        }

        bool empty() const {
            return c.empty();
        }

        size_type size() const {
            return c.size();
        }

        void push(const value_type& value) {
            c.push_back(value);
            std::push_heap(c.begin(), c.end(), comp);
        }

        void push(value_type&& value) {
            c.push_back(std::move(value));
            std::push_heap(c.begin(), c.end(), comp);
        }

        template<class... A>
        void emplace(A&&... a) {
            c.emplace_back(std::forward<A>(a)...);
            std::push_heap(c.begin(), c.end(), comp);
        }

        void pop() {
            std::pop_heap(c.begin(), c.end(), comp);
            c.pop_back();
        }

        void swap(priority_queue& other) noexcept(std::is_nothrow_swappable_v<Container> && std::is_nothrow_swappable_v<Compare>) {
            using std::swap;
            swap(c, other.c);
            swap(comp, other.comp);
        }

    protected:
        Container c;
        Compare comp;
    };

    template<class T, class Container, class Compare>
    inline void swap(priority_queue<T, Container, Compare>& lhs, priority_queue<T, Container, Compare>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    // Deduction, as for std::queue and std::priority_queue
    template<class Container>
    queue(Container) -> queue<typename Container::value_type, Container>;
    template<class InputIt>
    queue(InputIt, InputIt) -> queue<std::iter_value_t<InputIt>>;
    template<class Compare, class Container>
    priority_queue(Compare, Container) -> priority_queue<typename Container::value_type, Container, Compare>;
    template<class InputIt, class Compare = std::less<std::iter_value_t<InputIt>>>
    priority_queue(InputIt, InputIt, Compare = Compare()) -> priority_queue<std::iter_value_t<InputIt>, vector<std::iter_value_t<InputIt>>, Compare>;
    template<class InputIt, class Compare, class Container>
    priority_queue(InputIt, InputIt, Compare, Container) -> priority_queue<typename Container::value_type, Container, Compare>;
}
