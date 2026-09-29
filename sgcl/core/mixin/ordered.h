//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../detail/type_info.h"
#include "../req.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <numeric>
#include <vector>

namespace sgcl::detail {
    // The stable sort of elements that hold tracked pointers: the standard's
    // stable_sort moves them into a buffer of operator new, memory the
    // collector never scans (The rules, 1). Here the positions are sorted
    // instead — plain numbers, in a plain buffer — by std::stable_sort,
    // and the elements are then moved once each into place along the
    // cycles of that permutation, one of them at a time in a local on the
    // stack. O(n log n) comparisons and n moves: faster than the standard's
    // on the elements themselves, whose moves cost a write barrier each.
    template<class It, class Less>
    void stable_sort_by_positions(It first, It last, Less& less) {
        size_t n = size_t(last - first);
        std::vector<size_t> order(n);
        std::iota(order.begin(), order.end(), size_t(0));
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return less(first[a], first[b]); });
        // order[j] is the position of the element that goes to j
        for (size_t i = 0; i < n; ++i) {
            if (order[i] == i) {
                continue;
            }
            auto held = std::move(first[i]);
            size_t j = i;
            for (;;) {
                size_t k = order[j];
                order[j] = j;
                if (k == i) {
                    first[j] = std::move(held);
                    break;
                }
                first[j] = std::move(first[k]);
                j = k;
            }
        }
    }
}

namespace sgcl::mixin {
    // ordered<Derived>: the order of the whole range — whether it is
    // sorted, the searches that assume it is, and the sorting that makes
    // it so — as members, and the declaration that the range has one:
    // req::ordered<R> is "R carries ordered and its elements are
    // comparable". Every method exists only for elements that are
    // ordered (req::comparable), or takes a comparator or a key and asks
    // nothing of them. A sorted vector with these is the flat map: the
    // lookups of a map with the memory of a vector. The sorts exist only
    // where the elements can be written (req::sequence) and reached by
    // position: an immutable vector is ordered (is_sorted,
    // binary_search) but not sorted in place. The linked lists have a
    // sort of their own, on the nodes, which hides these.
    template<class Derived>
    class ordered {
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

        // Sorting in place, where the elements can be written (req::sequence)
        // and reached by position: by <, by a comparator, or by a key
        // taken from the element (v.sort_by(&item::name)). One mixin holds
        // every overload of a name: a name in two bases is ambiguous.
        constexpr void sort() requires detail::ComparableElements<Derived> && req::sequence<Derived> && req::random_access<Derived> {
            std::ranges::sort(_begin(), _end(), detail::Less{});
        }

        template<class Compare>
        constexpr void sort(Compare cmp) requires detail::ElementOrder<Compare, Derived> && req::sequence<Derived> && req::random_access<Derived> {
            std::ranges::sort(_begin(), _end(), cmp);
        }

        template<class Proj>
        constexpr void sort_by(Proj proj) requires detail::ElementVisitor<Proj, const Derived> && req::sequence<Derived> && req::random_access<Derived> {
            std::ranges::sort(_begin(), _end(), detail::Less{}, proj);
        }

        // Stable; elements that may hold tracked pointers are sorted by
        // their positions (detail::stable_sort_by_positions), any other by
        // the standard's
        void stable_sort() requires detail::ComparableElements<Derived> && req::sequence<Derived> && req::random_access<Derived> {
            _stable_sort(detail::Less{});
        }

        template<class Compare>
        void stable_sort(Compare cmp) requires detail::ElementOrder<Compare, Derived> && req::sequence<Derived> && req::random_access<Derived> {
            _stable_sort(cmp);
        }

    protected:
        ordered() = default;
        ~ordered() = default;

    private:
        template<class Compare>
        void _stable_sort(Compare cmp) {
            using T = std::remove_cvref_t<decltype(*_begin())>;
            if constexpr (detail::TypeInfo<T>::MayContainTracked) {
                detail::stable_sort_by_positions(_begin(), _end(), cmp);
            } else {
                std::ranges::stable_sort(_begin(), _end(), cmp);
            }
        }

        constexpr auto _begin() noexcept { return static_cast<Derived&>(*this).begin(); }
        constexpr auto _end() noexcept { return static_cast<Derived&>(*this).end(); }
        constexpr auto _begin() const noexcept { return static_cast<const Derived&>(*this).begin(); }
        constexpr auto _end() const noexcept { return static_cast<const Derived&>(*this).end(); }
    };
}
