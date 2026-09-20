//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "concepts.h"

#include <algorithm>
#include <iterator>

namespace sgcl {
    // m_enumerable<Derived>: the questions asked of the elements of a
    // range, as members of every container of the library that iterates,
    // and the declaration that it does: c_enumerable<R> is "R carries
    // m_enumerable". A mixin (m_): a static interface, no virtual method,
    // no state; its constructor and destructor are protected, so it
    // exists only as the base of the class that names itself as Derived
    // and gives its elements through begin() and end(). The questions
    // that compare elements exist only for elements that compare
    // (c_equatable, c_comparable, on each method); the ones that take a
    // predicate or a comparator ask nothing of the element. A container
    // whose own answer is better (a set's contains by the key, its min as
    // *begin()) hides the mixin's with a method of the same name — every
    // overload of the name with it, since a constrained overload of the
    // base and an unconstrained one of the class would be ambiguous.
    template<class Derived>
    class m_enumerable {
    public:
        // Searching by a predicate: the position of the first element
        // accepted (npos when none), a pointer to it (null when none)
        template<class Pred>
        constexpr size_t find_index(Pred pred) const requires detail::ElementPredicate<Pred, Derived> {
            auto it = std::ranges::find_if(_begin(), _end(), pred);
            return it == _end() ? npos : size_t(std::ranges::distance(_begin(), it));
        }

        template<class Pred>
        constexpr auto find_if(Pred pred) noexcept requires detail::ElementPredicate<Pred, Derived> {
            auto it = std::ranges::find_if(_begin(), _end(), pred);
            return it == _end() ? nullptr : &*it;
        }

        template<class Pred>
        constexpr auto find_if(Pred pred) const noexcept requires detail::ElementPredicate<Pred, Derived> {
            auto it = std::ranges::find_if(_begin(), _end(), pred);
            return it == _end() ? nullptr : &*it;
        }

        // Whether some element satisfies the predicate; whether every one
        // does; how many do
        template<class Pred>
        constexpr bool exists(Pred pred) const requires detail::ElementPredicate<Pred, Derived> {
            return std::ranges::find_if(_begin(), _end(), pred) != _end();
        }

        template<class Pred>
        constexpr bool all(Pred pred) const requires detail::ElementPredicate<Pred, Derived> {
            return std::ranges::all_of(_begin(), _end(), pred);
        }

        template<class Pred>
        constexpr size_t count_of(Pred pred) const requires detail::ElementPredicate<Pred, Derived> {
            return size_t(std::ranges::count_if(_begin(), _end(), pred));
        }

        // Visiting
        template<class F>
        constexpr void for_each(F f) requires detail::ElementVisitor<F, Derived> {
            std::ranges::for_each(_begin(), _end(), f);
        }

        template<class F>
        constexpr void for_each(F f) const requires detail::ElementVisitor<F, const Derived> {
            std::ranges::for_each(_begin(), _end(), f);
        }

        // Searching by a value: elements that compare equal
        constexpr bool contains(const auto& value) const requires detail::EquatableElements<Derived> {
            return std::ranges::find(_begin(), _end(), value) != _end();
        }

        // The position of the first element equal to value, npos when none
        constexpr size_t index_of(const auto& value) const requires detail::EquatableElements<Derived> {
            auto it = std::ranges::find(_begin(), _end(), value);
            return it == _end() ? npos : size_t(std::ranges::distance(_begin(), it));
        }

        constexpr size_t last_index_of(const auto& value) const requires detail::EquatableElements<Derived> {
            size_t found = npos, i = 0;
            for (auto it = _begin(); it != _end(); ++it, ++i) {
                if (*it == value) {
                    found = i;
                }
            }
            return found;
        }

        // The smallest and the largest element: undefined on an empty
        // range, as front(); by <, or by the comparator given. A reference
        // into the range, or a value where the iterator gives values
        // (range(n))
        constexpr decltype(auto) min() const requires detail::ComparableElements<Derived> {
            return *std::ranges::min_element(_begin(), _end(), detail::Less{});
        }

        template<class Compare>
        constexpr decltype(auto) min(Compare cmp) const requires detail::ElementOrder<Compare, Derived> {
            return *std::ranges::min_element(_begin(), _end(), cmp);
        }

        constexpr decltype(auto) max() const requires detail::ComparableElements<Derived> {
            return *std::ranges::max_element(_begin(), _end(), detail::Less{});
        }

        template<class Compare>
        constexpr decltype(auto) max(Compare cmp) const requires detail::ElementOrder<Compare, Derived> {
            return *std::ranges::max_element(_begin(), _end(), cmp);
        }

    protected:
        m_enumerable() = default;
        ~m_enumerable() = default;

    private:
        constexpr auto _begin() noexcept { return static_cast<Derived&>(*this).begin(); }
        constexpr auto _end() noexcept { return static_cast<Derived&>(*this).end(); }
        constexpr auto _begin() const noexcept { return static_cast<const Derived&>(*this).begin(); }
        constexpr auto _end() const noexcept { return static_cast<const Derived&>(*this).end(); }
    };
}
