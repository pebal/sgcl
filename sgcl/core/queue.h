//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "deque.h"
#include "detail/nothrow_function.h"
#include "vector.h"

#include <algorithm>
#include <compare>
#include <concepts>
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

        SGCL_INLINE_HOT queue() noexcept(std::is_nothrow_default_constructible_v<Container> && std::is_nothrow_move_constructible_v<Container>)
        : queue(Container()) {
        }

        SGCL_INLINE_HOT explicit queue(const Container& cont) noexcept(std::is_nothrow_copy_constructible_v<Container>)
        : c(cont) {
        }

        SGCL_INLINE_HOT explicit queue(Container&& cont) noexcept(std::is_nothrow_move_constructible_v<Container>)
        : c(std::move(cont)) {
        }

        template<std::input_iterator InputIt>
        SGCL_INLINE_HOT queue(InputIt first, InputIt last)
        : c(first, last) {
        }

        SGCL_INLINE_HOT reference front() noexcept(noexcept(c.front())) {
            return c.front();
        }

        SGCL_INLINE_HOT const_reference front() const noexcept(noexcept(c.front())) {
            return c.front();
        }

        SGCL_INLINE_HOT reference back() noexcept(noexcept(c.back())) {
            return c.back();
        }

        SGCL_INLINE_HOT const_reference back() const noexcept(noexcept(c.back())) {
            return c.back();
        }

        SGCL_INLINE_HOT bool empty() const noexcept(noexcept(c.empty())) {
            return c.empty();
        }

        SGCL_INLINE_HOT size_type size() const noexcept(noexcept(c.size())) {
            return c.size();
        }

        SGCL_INLINE_HOT void push(const value_type& value) noexcept(noexcept(c.push_back(value))) {
            c.push_back(value);
        }

        SGCL_INLINE_HOT void push(value_type&& value) noexcept(noexcept(c.push_back(std::move(value)))) {
            c.push_back(std::move(value));
        }

        template<class... A>
        SGCL_INLINE_HOT decltype(auto) emplace(A&&... a) noexcept(noexcept(c.emplace_back(std::forward<A>(a)...))) {
            return c.emplace_back(std::forward<A>(a)...);
        }

        SGCL_INLINE_HOT void pop() noexcept(noexcept(c.pop_front())) {
            c.pop_front();
        }

        SGCL_INLINE_HOT void swap(queue& other) noexcept(std::is_nothrow_swappable_v<Container>) {
            using std::swap;
            swap(c, other.c);
        }

        // As the container compares, and only where it does
        SGCL_INLINE_HOT friend bool operator==(const queue& lhs, const queue& rhs) requires std::equality_comparable<Container> {
            return lhs.c == rhs.c;
        }

        SGCL_INLINE_HOT friend auto operator<=>(const queue& lhs, const queue& rhs) requires std::three_way_comparable<Container> {
            return lhs.c <=> rhs.c;
        }

    protected:
        Container c;
    };

    template<class T, class Container>
    SGCL_INLINE_HOT void swap(queue<T, Container>& lhs, queue<T, Container>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
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

        static_assert(detail::nothrow_function_object<Compare, const value_type&, const value_type&>, "sgcl::priority_queue: Compare must be noexcept");

        // An empty container is a heap already: nothing is moved
        SGCL_INLINE_HOT priority_queue() noexcept(std::is_nothrow_default_constructible_v<Container> && std::is_nothrow_default_constructible_v<Compare>)
        : c()
        , comp() {
        }

        SGCL_INLINE_HOT explicit priority_queue(const Compare& compare) noexcept(std::is_nothrow_default_constructible_v<Container> && std::is_nothrow_copy_constructible_v<Compare>)
        : c()
        , comp(compare) {
        }

        SGCL_INLINE_HOT priority_queue(const Compare& compare, const Container& cont) noexcept(_nothrow_made_of<const Container&>())
        : c(cont)
        , comp(compare) {
            std::make_heap(c.begin(), c.end(), comp);
        }

        SGCL_INLINE_HOT priority_queue(const Compare& compare, Container&& cont) noexcept(_nothrow_made_of<Container&&>())
        : c(std::move(cont))
        , comp(compare) {
            std::make_heap(c.begin(), c.end(), comp);
        }

        template<std::input_iterator InputIt>
        SGCL_INLINE_HOT priority_queue(InputIt first, InputIt last, const Compare& compare = Compare())
        : c(first, last)
        , comp(compare) {
            std::make_heap(c.begin(), c.end(), comp);
        }

        template<std::input_iterator InputIt>
        SGCL_INLINE_HOT priority_queue(InputIt first, InputIt last, const Compare& compare, const Container& cont)
        : c(cont)
        , comp(compare) {
            c.insert(c.end(), first, last);
            std::make_heap(c.begin(), c.end(), comp);
        }

        template<std::input_iterator InputIt>
        SGCL_INLINE_HOT priority_queue(InputIt first, InputIt last, const Compare& compare, Container&& cont)
        : c(std::move(cont))
        , comp(compare) {
            c.insert(c.end(), first, last);
            std::make_heap(c.begin(), c.end(), comp);
        }

        SGCL_INLINE_HOT const_reference top() const noexcept(noexcept(c.front())) {
            return c.front();
        }

        SGCL_INLINE_HOT bool empty() const noexcept(noexcept(c.empty())) {
            return c.empty();
        }

        SGCL_INLINE_HOT size_type size() const noexcept(noexcept(c.size())) {
            return c.size();
        }

        SGCL_INLINE_HOT void push(const value_type& value) noexcept(noexcept(c.push_back(value)) && _nothrow_heap()) {
            c.push_back(value);
            std::push_heap(c.begin(), c.end(), comp);
        }

        SGCL_INLINE_HOT void push(value_type&& value) noexcept(noexcept(c.push_back(std::move(value))) && _nothrow_heap()) {
            c.push_back(std::move(value));
            std::push_heap(c.begin(), c.end(), comp);
        }

        template<class... A>
        SGCL_INLINE_HOT void emplace(A&&... a) noexcept(noexcept(c.emplace_back(std::forward<A>(a)...)) && _nothrow_heap()) {
            c.emplace_back(std::forward<A>(a)...);
            std::push_heap(c.begin(), c.end(), comp);
        }

        SGCL_INLINE_HOT void pop() noexcept(noexcept(c.pop_back()) && _nothrow_heap()) {
            std::pop_heap(c.begin(), c.end(), comp);
            c.pop_back();
        }

        SGCL_INLINE_HOT void swap(priority_queue& other) noexcept(std::is_nothrow_swappable_v<Container> && std::is_nothrow_swappable_v<Compare>) {
            using std::swap;
            swap(c, other.c);
            swap(comp, other.comp);
        }

    protected:
        Container c;
        Compare comp;

        // The heap's steps move the elements (the comparator is noexcept)
        SGCL_INLINE_HOT static constexpr bool _nothrow_heap() noexcept {
            return std::is_nothrow_move_constructible_v<value_type> && std::is_nothrow_move_assignable_v<value_type>;
        }

        // A queue made of a container: the container copied or moved in,
        // the comparator copied, and the heap made of the elements
        template<class C>
        SGCL_INLINE_HOT static constexpr bool _nothrow_made_of() noexcept {
            return std::is_nothrow_constructible_v<Container, C> && std::is_nothrow_copy_constructible_v<Compare> && _nothrow_heap();
        }
    };

    template<class T, class Container, class Compare>
    SGCL_INLINE_HOT void swap(priority_queue<T, Container, Compare>& lhs, priority_queue<T, Container, Compare>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
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
