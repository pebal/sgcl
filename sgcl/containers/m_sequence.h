//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/sequence.h"

namespace sgcl {
    // m_sequence<Derived>: the algorithms of a sequence as members,
    // mixed into vector, array<T>, deque, list and forward_list. A mixin
    // (m_): a static interface, no virtual method, no state; its
    // constructor and destructor are protected, so it exists only as the
    // base of the container that names itself as Derived, which gives its
    // iterators through begin() and end().
    template<class Derived>
    class m_sequence {
    public:
        // Searching
        bool contains(const auto& value) const {
            return detail::Sequence::contains(_begin(), _end(), value);
        }

        // The position of the first element equal to value, npos when none
        size_t index_of(const auto& value) const {
            return detail::Sequence::index_of(_begin(), _end(), value);
        }

        size_t last_index_of(const auto& value) const {
            return detail::Sequence::last_index_of(_begin(), _end(), value);
        }

        // The position of the first element the predicate accepts, npos when none
        template<class Pred>
        size_t find_index(Pred pred) const {
            return detail::Sequence::find_index(_begin(), _end(), pred);
        }

        // The first element the predicate accepts (a pointer to it), null when none
        template<class Pred>
        auto find(Pred pred) noexcept {
            return detail::Sequence::find(_begin(), _end(), pred);
        }

        template<class Pred>
        auto find(Pred pred) const noexcept {
            return detail::Sequence::find(_begin(), _end(), pred);
        }

        // Whether some element satisfies the predicate; whether every one does
        template<class Pred>
        bool exists(Pred pred) const {
            return detail::Sequence::exists(_begin(), _end(), pred);
        }

        template<class Pred>
        bool all(Pred pred) const {
            return detail::Sequence::all(_begin(), _end(), pred);
        }

        // The number of elements the predicate accepts
        template<class Pred>
        size_t count_of(Pred pred) const {
            return detail::Sequence::count_of(_begin(), _end(), pred);
        }

        // Visiting
        template<class F>
        void for_each(F f) {
            detail::Sequence::for_each(_begin(), _end(), f);
        }

        template<class F>
        void for_each(F f) const {
            detail::Sequence::for_each(_begin(), _end(), f);
        }

        // The smallest and the largest element: undefined on an empty sequence, as front()
        const auto& min() const {
            return detail::Sequence::min(_begin(), _end());
        }

        template<class Compare>
        const auto& min(Compare cmp) const {
            return detail::Sequence::min(_begin(), _end(), cmp);
        }

        const auto& max() const {
            return detail::Sequence::max(_begin(), _end());
        }

        template<class Compare>
        const auto& max(Compare cmp) const {
            return detail::Sequence::max(_begin(), _end(), cmp);
        }

        // Writing
        void fill(const auto& value) {
            detail::Sequence::fill(_begin(), _end(), value);
        }

        // Ordering: reverse needs a bidirectional sequence, sort a random-access
        // one; the linked lists have their own, on the nodes
        void reverse() noexcept {
            detail::Sequence::reverse(_begin(), _end());
        }

        void sort() {
            detail::Sequence::sort(_begin(), _end());
        }

        template<class Compare>
        void sort(Compare cmp) {
            detail::Sequence::sort(_begin(), _end(), cmp);
        }

        bool is_sorted() const {
            return detail::Sequence::is_sorted(_begin(), _end());
        }

        template<class Compare>
        bool is_sorted(Compare cmp) const {
            return detail::Sequence::is_sorted(_begin(), _end(), cmp);
        }

        // On a sorted sequence (by <, or by the comparator given): whether
        // the value is there, its position (npos when not), the first
        // position not less than it and the first greater, O(log n)
        // comparisons. A sorted vector with these is the flat map: the
        // lookups of a map with the memory of a vector.
        bool binary_search(const auto& value) const {
            return detail::Sequence::binary_search(_begin(), _end(), value);
        }

        template<class Compare>
        bool binary_search(const auto& value, Compare cmp) const {
            return detail::Sequence::binary_search(_begin(), _end(), value, cmp);
        }

        size_t sorted_index_of(const auto& value) const {
            return detail::Sequence::sorted_index_of(_begin(), _end(), value);
        }

        template<class Compare>
        size_t sorted_index_of(const auto& value, Compare cmp) const {
            return detail::Sequence::sorted_index_of(_begin(), _end(), value, cmp);
        }

        auto lower_bound(const auto& value) {
            return detail::Sequence::lower_bound(_begin(), _end(), value);
        }

        auto lower_bound(const auto& value) const {
            return detail::Sequence::lower_bound(_begin(), _end(), value);
        }

        template<class Compare>
        auto lower_bound(const auto& value, Compare cmp) {
            return detail::Sequence::lower_bound(_begin(), _end(), value, cmp);
        }

        template<class Compare>
        auto lower_bound(const auto& value, Compare cmp) const {
            return detail::Sequence::lower_bound(_begin(), _end(), value, cmp);
        }

        auto upper_bound(const auto& value) {
            return detail::Sequence::upper_bound(_begin(), _end(), value);
        }

        auto upper_bound(const auto& value) const {
            return detail::Sequence::upper_bound(_begin(), _end(), value);
        }

        template<class Compare>
        auto upper_bound(const auto& value, Compare cmp) {
            return detail::Sequence::upper_bound(_begin(), _end(), value, cmp);
        }

        template<class Compare>
        auto upper_bound(const auto& value, Compare cmp) const {
            return detail::Sequence::upper_bound(_begin(), _end(), value, cmp);
        }

    protected:
        m_sequence() = default;
        ~m_sequence() = default;

    private:
        auto _begin() noexcept { return static_cast<Derived&>(*this).begin(); }
        auto _end() noexcept { return static_cast<Derived&>(*this).end(); }
        auto _begin() const noexcept { return static_cast<const Derived&>(*this).begin(); }
        auto _end() const noexcept { return static_cast<const Derived&>(*this).end(); }
    };
}
