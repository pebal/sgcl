//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "deque.h"

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

        stack()
        : stack(Container()) {
        }

        explicit stack(const Container& cont)
        : c(cont) {
        }

        explicit stack(Container&& cont)
        : c(std::move(cont)) {
        }

        template<std::input_iterator InputIt>
        stack(InputIt first, InputIt last)
        : c(first, last) {
        }

        reference top() {
            return c.back();
        }

        const_reference top() const {
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
            c.pop_back();
        }

        void swap(stack& other) noexcept(std::is_nothrow_swappable_v<Container>) {
            using std::swap;
            swap(c, other.c);
        }

    protected:
        Container c;

        friend bool operator==(const stack& lhs, const stack& rhs) {
            return lhs.c == rhs.c;
        }

        friend auto operator<=>(const stack& lhs, const stack& rhs) {
            return lhs.c <=> rhs.c;
        }
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
