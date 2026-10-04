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

        SGCL_INLINE_HOT stack() noexcept(std::is_nothrow_default_constructible_v<Container> && std::is_nothrow_move_constructible_v<Container>)
        : stack(Container()) {
        }

        SGCL_INLINE_HOT explicit stack(const Container& cont) noexcept(std::is_nothrow_copy_constructible_v<Container>)
        : c(cont) {
        }

        SGCL_INLINE_HOT explicit stack(Container&& cont) noexcept(std::is_nothrow_move_constructible_v<Container>)
        : c(std::move(cont)) {
        }

        template<std::input_iterator InputIt>
        SGCL_INLINE_HOT stack(InputIt first, InputIt last)
        : c(first, last) {
        }

        SGCL_INLINE_HOT reference top() noexcept(noexcept(c.back())) {
            return c.back();
        }

        SGCL_INLINE_HOT const_reference top() const noexcept(noexcept(c.back())) {
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

        SGCL_INLINE_HOT void pop() noexcept(noexcept(c.pop_back())) {
            c.pop_back();
        }

        SGCL_INLINE_HOT void swap(stack& other) noexcept(std::is_nothrow_swappable_v<Container>) {
            using std::swap;
            swap(c, other.c);
        }

        // As the container compares, and only where it does
        SGCL_INLINE_HOT friend bool operator==(const stack& lhs, const stack& rhs) requires std::equality_comparable<Container> {
            return lhs.c == rhs.c;
        }

        SGCL_INLINE_HOT friend auto operator<=>(const stack& lhs, const stack& rhs) requires std::three_way_comparable<Container> {
            return lhs.c <=> rhs.c;
        }

    protected:
        Container c;
    };

    template<class T, class Container>
    SGCL_INLINE_HOT void swap(stack<T, Container>& lhs, stack<T, Container>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    // Deduction, as for std::stack
    template<class Container>
    stack(Container) -> stack<typename Container::value_type, Container>;
    template<class InputIt>
    stack(InputIt, InputIt) -> stack<std::iter_value_t<InputIt>>;
}
