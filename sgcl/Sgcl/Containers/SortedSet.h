//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// SortedSet<T> (a red-black tree of unique values, in order) and
// SortedMultiSet<T> (any number of each).
#pragma once

#include "../../containers/multiset.h"
#include "../../containers/set.h"
#include "detail/Associative.h"

namespace Sgcl {
    template<class T, class Compare = std::less<T>>
    class SortedSet : public detail::Set<sgcl::set<T, Compare>> {
        using Base = detail::Set<sgcl::set<T, Compare>>;

    public:
        using typename Base::InnerType;
        using typename Base::SizeType;

        SortedSet() = default;

        template<std::input_iterator It>
        SortedSet(It first, It last)
        : Base(InnerType(first, last)) {
        }

        SortedSet(std::initializer_list<typename InnerType::value_type> il)
        : Base(InnerType(il)) {
        }

        explicit SortedSet(InnerType c) noexcept
        : Base(std::move(c)) {
        }

        explicit SortedSet(const Compare& cmp)
        : Base(InnerType(cmp)) {
        }

        SortedSet(const SortedSet&) = default;
        SortedSet(SortedSet&&) noexcept = default;
        SortedSet& operator=(const SortedSet&) = default;
        SortedSet& operator=(SortedSet&&) noexcept = default;

        // The least and the greatest (by the key)
        auto& First() noexcept {
            return *this->_c.begin();
        }

        const auto& First() const noexcept {
            return *this->_c.begin();
        }

        auto& Last() noexcept {
            return *std::prev(this->_c.end());
        }

        const auto& Last() const noexcept {
            return *std::prev(this->_c.end());
        }

        // The first element not before the key, and the first after it
        template<class K>
        auto LowerBound(const K& key) noexcept {
            return this->_c.lower_bound(key);
        }

        template<class K>
        auto LowerBound(const K& key) const noexcept {
            return this->_c.lower_bound(key);
        }

        template<class K>
        auto UpperBound(const K& key) noexcept {
            return this->_c.upper_bound(key);
        }

        template<class K>
        auto UpperBound(const K& key) const noexcept {
            return this->_c.upper_bound(key);
        }

        // The run of the key: from LowerBound to UpperBound
        template<class K>
        auto EqualRange(const K& key) noexcept {
            return Range<typename Base::Iterator>(this->_c.equal_range(key));
        }

        template<class K>
        auto EqualRange(const K& key) const noexcept {
            return Range<typename Base::ConstIterator>(this->_c.equal_range(key));
        }

        Compare KeyCompare() const {
            return this->_c.key_comp();
        }
    };

    template<class T, class Compare = std::less<T>>
    class SortedMultiSet : public detail::MultiSet<sgcl::multiset<T, Compare>> {
        using Base = detail::MultiSet<sgcl::multiset<T, Compare>>;

    public:
        using typename Base::InnerType;
        using typename Base::SizeType;

        SortedMultiSet() = default;

        template<std::input_iterator It>
        SortedMultiSet(It first, It last)
        : Base(InnerType(first, last)) {
        }

        SortedMultiSet(std::initializer_list<typename InnerType::value_type> il)
        : Base(InnerType(il)) {
        }

        explicit SortedMultiSet(InnerType c) noexcept
        : Base(std::move(c)) {
        }

        explicit SortedMultiSet(const Compare& cmp)
        : Base(InnerType(cmp)) {
        }

        SortedMultiSet(const SortedMultiSet&) = default;
        SortedMultiSet(SortedMultiSet&&) noexcept = default;
        SortedMultiSet& operator=(const SortedMultiSet&) = default;
        SortedMultiSet& operator=(SortedMultiSet&&) noexcept = default;

        // The least and the greatest (by the key)
        auto& First() noexcept {
            return *this->_c.begin();
        }

        const auto& First() const noexcept {
            return *this->_c.begin();
        }

        auto& Last() noexcept {
            return *std::prev(this->_c.end());
        }

        const auto& Last() const noexcept {
            return *std::prev(this->_c.end());
        }

        // The first element not before the key, and the first after it
        template<class K>
        auto LowerBound(const K& key) noexcept {
            return this->_c.lower_bound(key);
        }

        template<class K>
        auto LowerBound(const K& key) const noexcept {
            return this->_c.lower_bound(key);
        }

        template<class K>
        auto UpperBound(const K& key) noexcept {
            return this->_c.upper_bound(key);
        }

        template<class K>
        auto UpperBound(const K& key) const noexcept {
            return this->_c.upper_bound(key);
        }

        // The run of the key: from LowerBound to UpperBound
        template<class K>
        auto EqualRange(const K& key) noexcept {
            return Range<typename Base::Iterator>(this->_c.equal_range(key));
        }

        template<class K>
        auto EqualRange(const K& key) const noexcept {
            return Range<typename Base::ConstIterator>(this->_c.equal_range(key));
        }

        Compare KeyCompare() const {
            return this->_c.key_comp();
        }
    };

    // Deduction from an initializer list, as for the sorted sets of std
    template<class T, class Compare = std::less<T>>
    SortedSet(std::initializer_list<T>, Compare = Compare()) -> SortedSet<T, Compare>;
    template<class T, class Compare = std::less<T>>
    SortedMultiSet(std::initializer_list<T>, Compare = Compare()) -> SortedMultiSet<T, Compare>;
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

