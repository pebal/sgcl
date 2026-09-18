//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// SortedDictionary<Key, Value> (a red-black tree, the keys in order) and
// SortedMultiDictionary<Key, Value> (any number of values under a key).
#pragma once

#include "../../containers/map.h"
#include "../../containers/multimap.h"
#include "../Core/Types.h"
#include "detail/Associative.h"

namespace Sgcl {
    template<class Key, class Value, class Compare = std::less<Key>>
    class SortedDictionary : public detail::Map<sgcl::map<Key, Value, Compare>> {
        using Base = detail::Map<sgcl::map<Key, Value, Compare>>;

    public:
        using typename Base::InnerType;
        using typename Base::SizeType;

        SortedDictionary() = default;

        template<std::input_iterator It>
        SortedDictionary(It first, It last)
        : Base(InnerType(first, last)) {
        }

        SortedDictionary(std::initializer_list<typename InnerType::value_type> il)
        : Base(InnerType(il)) {
        }

        explicit SortedDictionary(InnerType c) noexcept
        : Base(std::move(c)) {
        }

        explicit SortedDictionary(const Compare& cmp)
        : Base(InnerType(cmp)) {
        }

        SortedDictionary(const SortedDictionary&) = default;
        SortedDictionary(SortedDictionary&&) noexcept = default;
        SortedDictionary& operator=(const SortedDictionary&) = default;
        SortedDictionary& operator=(SortedDictionary&&) noexcept = default;

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

    template<class Key, class Value, class Compare = std::less<Key>>
    class SortedMultiDictionary : public detail::MultiMap<sgcl::multimap<Key, Value, Compare>> {
        using Base = detail::MultiMap<sgcl::multimap<Key, Value, Compare>>;

    public:
        using typename Base::InnerType;
        using typename Base::SizeType;

        SortedMultiDictionary() = default;

        template<std::input_iterator It>
        SortedMultiDictionary(It first, It last)
        : Base(InnerType(first, last)) {
        }

        SortedMultiDictionary(std::initializer_list<typename InnerType::value_type> il)
        : Base(InnerType(il)) {
        }

        explicit SortedMultiDictionary(InnerType c) noexcept
        : Base(std::move(c)) {
        }

        explicit SortedMultiDictionary(const Compare& cmp)
        : Base(InnerType(cmp)) {
        }

        SortedMultiDictionary(const SortedMultiDictionary&) = default;
        SortedMultiDictionary(SortedMultiDictionary&&) noexcept = default;
        SortedMultiDictionary& operator=(const SortedMultiDictionary&) = default;
        SortedMultiDictionary& operator=(SortedMultiDictionary&&) noexcept = default;

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

    // Deduction from an initializer list, as for the sorted maps of std
    template<class Key, class Value, class Compare = std::less<Key>>
    SortedDictionary(std::initializer_list<Pair<Key, Value>>, Compare = Compare()) -> SortedDictionary<Key, Value, Compare>;
    template<class Key, class Value, class Compare = std::less<Key>>
    SortedMultiDictionary(std::initializer_list<Pair<Key, Value>>, Compare = Compare()) -> SortedMultiDictionary<Key, Value, Compare>;
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

