//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// OrderedDictionary<Key, Value>: a Dictionary iterated in the order the
// entries were added (Java's LinkedHashMap, .NET's OrderedDictionary).
// The same table, the entries on one more list: First is the oldest
// entry and Last the newest, a copy keeps the order, a Set of a present
// key leaves it where it was, MoveToLast and MoveToFirst move an entry
// in the order (a cache with an eviction order: MoveToLast on a hit,
// RemoveFirst when full). Find hands back a pointer to the value, null
// when the key is absent.
#pragma once

#include "../../containers/ordered_map.h"
#include "../Core/Types.h"
#include "detail/Associative.h"

namespace Sgcl {
    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    class OrderedDictionary : public detail::Map<sgcl::ordered_map<Key, Value, Hash, Equal>> {
        using Base = detail::Map<sgcl::ordered_map<Key, Value, Hash, Equal>>;

    public:
        using typename Base::InnerType;
        using typename Base::SizeType;
        using typename Base::PairType;
        using typename Base::ConstIterator;

        OrderedDictionary() = default;

        template<std::input_iterator It>
        OrderedDictionary(It first, It last)
        : Base(InnerType(first, last)) {
        }

        OrderedDictionary(std::initializer_list<typename InnerType::value_type> il)
        : Base(InnerType(il)) {
        }

        explicit OrderedDictionary(InnerType c) noexcept
        : Base(std::move(c)) {
        }

        explicit OrderedDictionary(SizeType buckets)
        : Base(InnerType(buckets)) {
        }

        OrderedDictionary(const OrderedDictionary&) = default;
        OrderedDictionary(OrderedDictionary&&) noexcept = default;
        OrderedDictionary& operator=(const OrderedDictionary&) = default;
        OrderedDictionary& operator=(OrderedDictionary&&) noexcept = default;

        // The oldest and the newest entry (the dictionary not empty)
        PairType& First() noexcept {
            return this->_c.front();
        }

        const PairType& First() const noexcept {
            return this->_c.front();
        }

        PairType& Last() noexcept {
            return this->_c.back();
        }

        const PairType& Last() const noexcept {
            return this->_c.back();
        }

        // The oldest (the newest) entry removed: whether there was one
        bool RemoveFirst() {
            if (this->_c.empty()) {
                return false;
            }
            this->_c.erase(this->_c.begin());
            return true;
        }

        bool RemoveLast() {
            if (this->_c.empty()) {
                return false;
            }
            this->_c.erase(std::prev(this->_c.end()));
            return true;
        }

        // The entry moved to the end (the start) of the order: as if added
        // last (first) from now on; by its iterator (FindEntry) or by its
        // key, then whether the key was there
        void MoveToLast(ConstIterator pos) noexcept {
            this->_c.to_back(pos);
        }

        void MoveToFirst(ConstIterator pos) noexcept {
            this->_c.to_front(pos);
        }

        template<class K = Key> requires (!std::is_convertible_v<const K&, ConstIterator>)
        bool MoveToLast(const K& key) noexcept {
            auto it = this->_c.find(key);
            if (it == this->_c.end()) {
                return false;
            }
            this->_c.to_back(it);
            return true;
        }

        template<class K = Key> requires (!std::is_convertible_v<const K&, ConstIterator>)
        bool MoveToFirst(const K& key) noexcept {
            auto it = this->_c.find(key);
            if (it == this->_c.end()) {
                return false;
            }
            this->_c.to_front(it);
            return true;
        }

        // Room for `n` entries without a rehash
        void Reserve(SizeType n) {
            this->_c.reserve(n);
        }

        // The buckets: a power of two, doubled when the load reaches the maximum
        SizeType BucketCount() const noexcept {
            return this->_c.bucket_count();
        }

        float LoadFactor() const noexcept {
            return this->_c.load_factor();
        }

        float MaxLoadFactor() const noexcept {
            return this->_c.max_load_factor();
        }

        void MaxLoadFactor(float z) {
            this->_c.max_load_factor(z);
        }

        void Rehash(SizeType buckets) {
            this->_c.rehash(buckets);
        }

        Hash HashFunction() const {
            return this->_c.hash_function();
        }

        Equal KeyEqual() const {
            return this->_c.key_eq();
        }
    };

    // Deduction from an initializer list, as for the hash maps of std
    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    OrderedDictionary(std::initializer_list<Pair<Key, Value>>, size_t = 0, Hash = Hash(), Equal = Equal()) -> OrderedDictionary<Key, Value, Hash, Equal>;
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
