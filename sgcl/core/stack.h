//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "deque.h"

#include <compare>
#include <concepts>
#include <iterator>
#include <type_traits>
#include <utility>

namespace sgcl {
    // std::stack over a managed container. The container holds tracked
    // pointers, so a stack lives where a tracked_ptr may: on a thread's
    // stack or inside a managed object.
    template<class T, class Container = deque<T>>
    class stack {
    public:
        using container_type = Container;
        using value_type = typename Container::value_type;
        using size_type = typename Container::size_type;
        using reference = typename Container::reference;
        using const_reference = typename Container::const_reference;

        stack() noexcept(std::is_nothrow_default_constructible_v<Container> && std::is_nothrow_move_constructible_v<Container>)
        : stack(Container()) {
        }

        explicit stack(const Container& cont) noexcept(std::is_nothrow_copy_constructible_v<Container>)
        : c(cont) {
        }

        explicit stack(Container&& cont) noexcept(std::is_nothrow_move_constructible_v<Container>)
        : c(std::move(cont)) {
        }

        template<std::input_iterator InputIt>
        stack(InputIt first, InputIt last)
        : c(first, last) {
        }

        reference top() noexcept(noexcept(c.back())) {
            return c.back();
        }

        const_reference top() const noexcept(noexcept(c.back())) {
            return c.back();
        }

        bool empty() const noexcept(noexcept(c.empty())) {
            return c.empty();
        }

        size_type size() const noexcept(noexcept(c.size())) {
            return c.size();
        }

        void push(const value_type& value) noexcept(noexcept(c.push_back(value))) {
            c.push_back(value);
        }

        void push(value_type&& value) noexcept(noexcept(c.push_back(std::move(value)))) {
            c.push_back(std::move(value));
        }

        template<class... A>
        decltype(auto) emplace(A&&... a) noexcept(noexcept(c.emplace_back(std::forward<A>(a)...))) {
            return c.emplace_back(std::forward<A>(a)...);
        }

        void pop() noexcept(noexcept(c.pop_back())) {
            c.pop_back();
        }

        void swap(stack& other) noexcept(std::is_nothrow_swappable_v<Container>) {
            using std::swap;
            swap(c, other.c);
        }

        // As the container compares, and only where it does
        friend bool operator==(const stack& lhs, const stack& rhs) requires std::equality_comparable<Container> {
            return lhs.c == rhs.c;
        }

        friend auto operator<=>(const stack& lhs, const stack& rhs) requires std::three_way_comparable<Container> {
            return lhs.c <=> rhs.c;
        }

    protected:
        Container c;
    };

    template<class T, class Container>
    inline void swap(stack<T, Container>& lhs, stack<T, Container>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    // Deduction, as for std::stack
    template<class Container>
    stack(Container) -> stack<typename Container::value_type, Container>;
    template<class InputIt>
    stack(InputIt, InputIt) -> stack<std::iter_value_t<InputIt>>;
}
