//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// MSequence<Derived>: the algorithms of a sequence, written once (the
// core the whole library shares) and mixed into List, Array, Deque,
// LinkedList and ForwardList; begin and end of Derived are all it uses. A
// mixin (M): a static interface, no virtual method, no state; its
// constructor and destructor are protected, so it exists only as the base
// of the container that names itself as Derived.
#pragma once

#include "../../containers/detail/sequence.h"

#include <ranges>

namespace Sgcl {
    inline constexpr size_t NoIndex = sgcl::npos;

    template<class Derived>
    class MSequence {
    public:
        using SizeType = size_t;

        // Searching
        constexpr bool Contains(const auto& value) const {
            return sgcl::detail::Sequence::contains(_begin(), _end(), value);
        }

        // The position of the first element equal to value, NoIndex when none
        constexpr SizeType IndexOf(const auto& value) const {
            return sgcl::detail::Sequence::index_of(_begin(), _end(), value);
        }

        constexpr SizeType LastIndexOf(const auto& value) const {
            return sgcl::detail::Sequence::last_index_of(_begin(), _end(), value);
        }

        // The position of the first element the predicate accepts, NoIndex when none
        template<class Pred>
        constexpr SizeType FindIndex(Pred pred) const {
            return sgcl::detail::Sequence::find_index(_begin(), _end(), pred);
        }

        // The first element the predicate accepts (a pointer to it), null when none
        template<class Pred>
        constexpr auto Find(Pred pred) noexcept {
            return sgcl::detail::Sequence::find(_begin(), _end(), pred);
        }

        template<class Pred>
        constexpr auto Find(Pred pred) const noexcept {
            return sgcl::detail::Sequence::find(_begin(), _end(), pred);
        }

        // Whether some element satisfies the predicate; whether every one does
        template<class Pred>
        constexpr bool Exists(Pred pred) const {
            return sgcl::detail::Sequence::exists(_begin(), _end(), pred);
        }

        template<class Pred>
        constexpr bool All(Pred pred) const {
            return sgcl::detail::Sequence::all(_begin(), _end(), pred);
        }

        // The number of elements the predicate accepts
        template<class Pred>
        constexpr SizeType CountOf(Pred pred) const {
            return sgcl::detail::Sequence::count_of(_begin(), _end(), pred);
        }

        // Visiting
        template<class F>
        constexpr void ForEach(F f) {
            sgcl::detail::Sequence::for_each(_begin(), _end(), f);
        }

        template<class F>
        constexpr void ForEach(F f) const {
            sgcl::detail::Sequence::for_each(_begin(), _end(), f);
        }

        // The smallest and the largest element: undefined on an empty sequence, as First()
        constexpr const auto& Min() const {
            return sgcl::detail::Sequence::min(_begin(), _end());
        }

        template<class Compare>
        constexpr const auto& Min(Compare cmp) const {
            return sgcl::detail::Sequence::min(_begin(), _end(), cmp);
        }

        constexpr const auto& Max() const {
            return sgcl::detail::Sequence::max(_begin(), _end());
        }

        template<class Compare>
        constexpr const auto& Max(Compare cmp) const {
            return sgcl::detail::Sequence::max(_begin(), _end(), cmp);
        }

        // Writing
        constexpr void Fill(const auto& value) {
            sgcl::detail::Sequence::fill(_begin(), _end(), value);
        }

        // Ordering: Reverse needs a bidirectional sequence, Sort a random-access
        // one; the linked lists have their own, on the nodes
        constexpr void Reverse() noexcept {
            sgcl::detail::Sequence::reverse(_begin(), _end());
        }

        constexpr void Sort() {
            sgcl::detail::Sequence::sort(_begin(), _end());
        }

        template<class Compare>
        constexpr void Sort(Compare cmp) {
            sgcl::detail::Sequence::sort(_begin(), _end(), cmp);
        }

        constexpr bool IsSorted() const {
            return sgcl::detail::Sequence::is_sorted(_begin(), _end());
        }

        template<class Compare>
        constexpr bool IsSorted(Compare cmp) const {
            return sgcl::detail::Sequence::is_sorted(_begin(), _end(), cmp);
        }

        // On a sorted sequence (by <, or by the comparator given): the
        // position of the value, NoIndex when it is not there, the first
        // position not less than it and the first greater, O(log n)
        // comparisons. A sorted List with these is the flat dictionary:
        // the lookups of a dictionary with the memory of a list.
        constexpr SizeType BinarySearch(const auto& value) const {
            return sgcl::detail::Sequence::sorted_index_of(_begin(), _end(), value);
        }

        template<class Compare>
        constexpr SizeType BinarySearch(const auto& value, Compare cmp) const {
            return sgcl::detail::Sequence::sorted_index_of(_begin(), _end(), value, cmp);
        }

        constexpr auto LowerBound(const auto& value) {
            return sgcl::detail::Sequence::lower_bound(_begin(), _end(), value);
        }

        constexpr auto LowerBound(const auto& value) const {
            return sgcl::detail::Sequence::lower_bound(_begin(), _end(), value);
        }

        template<class Compare>
        constexpr auto LowerBound(const auto& value, Compare cmp) {
            return sgcl::detail::Sequence::lower_bound(_begin(), _end(), value, cmp);
        }

        template<class Compare>
        constexpr auto LowerBound(const auto& value, Compare cmp) const {
            return sgcl::detail::Sequence::lower_bound(_begin(), _end(), value, cmp);
        }

        constexpr auto UpperBound(const auto& value) {
            return sgcl::detail::Sequence::upper_bound(_begin(), _end(), value);
        }

        constexpr auto UpperBound(const auto& value) const {
            return sgcl::detail::Sequence::upper_bound(_begin(), _end(), value);
        }

        template<class Compare>
        constexpr auto UpperBound(const auto& value, Compare cmp) {
            return sgcl::detail::Sequence::upper_bound(_begin(), _end(), value, cmp);
        }

        template<class Compare>
        constexpr auto UpperBound(const auto& value, Compare cmp) const {
            return sgcl::detail::Sequence::upper_bound(_begin(), _end(), value, cmp);
        }

    protected:
        MSequence() = default;
        ~MSequence() = default;

    private:
        // The range of the container: its begin and end, the free functions
        constexpr auto _begin() noexcept { return std::ranges::begin(static_cast<Derived&>(*this)); }
        constexpr auto _end() noexcept { return std::ranges::end(static_cast<Derived&>(*this)); }
        constexpr auto _begin() const noexcept { return std::ranges::begin(static_cast<const Derived&>(*this)); }
        constexpr auto _end() const noexcept { return std::ranges::end(static_cast<const Derived&>(*this)); }
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

