//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "concepts.h"

#include <algorithm>
#include <functional>
#include <iterator>

namespace sgcl {
    // m_ordered<Derived>: the order of the whole range — whether it is
    // sorted, the searches that assume it is, and the sorting that makes
    // it so — as members, and the declaration that the range has one:
    // c_ordered<R> is "R carries m_ordered and its elements are
    // comparable". Every method exists only for elements that are
    // ordered (c_comparable), or takes a comparator or a key and asks
    // nothing of them. A sorted vector with these is the flat map: the
    // lookups of a map with the memory of a vector. The sorts exist only
    // where the elements can be written (c_sequence) and reached by
    // position: an immutable vector is ordered (is_sorted,
    // binary_search) but not sorted in place. The linked lists have a
    // sort of their own, on the nodes, which hides these.
    template<class Derived>
    class m_ordered {
    public:
        constexpr bool is_sorted() const requires detail::ComparableElements<Derived> {
            return std::ranges::is_sorted(_begin(), _end(), detail::Less{});
        }

        template<class Compare>
        constexpr bool is_sorted(Compare cmp) const requires detail::ElementOrder<Compare, Derived> {
            return std::ranges::is_sorted(_begin(), _end(), cmp);
        }

        // On a sorted range (by <, or by the comparator given): whether
        // the value is there, its position (npos when not), the first
        // position not less than it and the first greater; O(log n)
        // comparisons on a random-access range, O(n) steps otherwise
        constexpr bool binary_search(const auto& value) const requires detail::ComparableElements<Derived> {
            return std::ranges::binary_search(_begin(), _end(), value, detail::Less{});
        }

        template<class Compare>
        constexpr bool binary_search(const auto& value, Compare cmp) const requires detail::ElementOrder<Compare, Derived> {
            return std::ranges::binary_search(_begin(), _end(), value, cmp);
        }

        constexpr size_t sorted_index_of(const auto& value) const requires detail::ComparableElements<Derived> {
            auto it = std::ranges::lower_bound(_begin(), _end(), value, detail::Less{});
            return it != _end() && !(value < *it) ? size_t(std::ranges::distance(_begin(), it)) : npos;
        }

        template<class Compare>
        constexpr size_t sorted_index_of(const auto& value, Compare cmp) const requires detail::ElementOrder<Compare, Derived> {
            auto it = std::ranges::lower_bound(_begin(), _end(), value, cmp);
            return it != _end() && !cmp(value, *it) ? size_t(std::ranges::distance(_begin(), it)) : npos;
        }

        constexpr auto lower_bound(const auto& value) requires detail::ComparableElements<Derived> {
            return std::ranges::lower_bound(_begin(), _end(), value, detail::Less{});
        }

        constexpr auto lower_bound(const auto& value) const requires detail::ComparableElements<Derived> {
            return std::ranges::lower_bound(_begin(), _end(), value, detail::Less{});
        }

        template<class Compare>
        constexpr auto lower_bound(const auto& value, Compare cmp) requires detail::ElementOrder<Compare, Derived> {
            return std::ranges::lower_bound(_begin(), _end(), value, cmp);
        }

        template<class Compare>
        constexpr auto lower_bound(const auto& value, Compare cmp) const requires detail::ElementOrder<Compare, Derived> {
            return std::ranges::lower_bound(_begin(), _end(), value, cmp);
        }

        constexpr auto upper_bound(const auto& value) requires detail::ComparableElements<Derived> {
            return std::ranges::upper_bound(_begin(), _end(), value, detail::Less{});
        }

        constexpr auto upper_bound(const auto& value) const requires detail::ComparableElements<Derived> {
            return std::ranges::upper_bound(_begin(), _end(), value, detail::Less{});
        }

        template<class Compare>
        constexpr auto upper_bound(const auto& value, Compare cmp) requires detail::ElementOrder<Compare, Derived> {
            return std::ranges::upper_bound(_begin(), _end(), value, cmp);
        }

        template<class Compare>
        constexpr auto upper_bound(const auto& value, Compare cmp) const requires detail::ElementOrder<Compare, Derived> {
            return std::ranges::upper_bound(_begin(), _end(), value, cmp);
        }

        // Sorting in place, where the elements can be written (c_sequence)
        // and reached by position: by <, by a comparator, or by a key
        // taken from the element (v.sort_by(&item::name)). One mixin holds
        // every overload of a name: a name in two bases is ambiguous.
        constexpr void sort() requires detail::ComparableElements<Derived> && c_sequence<Derived> && c_random_access<Derived> {
            std::ranges::sort(_begin(), _end(), detail::Less{});
        }

        template<class Compare>
        constexpr void sort(Compare cmp) requires detail::ElementOrder<Compare, Derived> && c_sequence<Derived> && c_random_access<Derived> {
            std::ranges::sort(_begin(), _end(), cmp);
        }

        template<class Proj>
        constexpr void sort_by(Proj proj) requires detail::ElementVisitor<Proj, const Derived> && c_sequence<Derived> && c_random_access<Derived> {
            std::ranges::sort(_begin(), _end(), detail::Less{}, proj);
        }

        void stable_sort() requires detail::ComparableElements<Derived> && c_sequence<Derived> && c_random_access<Derived> {
            std::ranges::stable_sort(_begin(), _end(), detail::Less{});
        }

        template<class Compare>
        void stable_sort(Compare cmp) requires detail::ElementOrder<Compare, Derived> && c_sequence<Derived> && c_random_access<Derived> {
            std::ranges::stable_sort(_begin(), _end(), cmp);
        }

    protected:
        m_ordered() = default;
        ~m_ordered() = default;

    private:
        constexpr auto _begin() noexcept { return static_cast<Derived&>(*this).begin(); }
        constexpr auto _end() noexcept { return static_cast<Derived&>(*this).end(); }
        constexpr auto _begin() const noexcept { return static_cast<const Derived&>(*this).begin(); }
        constexpr auto _end() const noexcept { return static_cast<const Derived&>(*this).end(); }
    };
}
