//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ConcurrentDictionary<Key, Value> (a lock-free hash table, Java's
// ConcurrentHashMap) and ConcurrentSortedDictionary<Key, Value> (a
// lock-free skip list, the keys in order). Shared by any number of
// threads; a walk goes on while others insert and erase. TryGet copies
// the value out; Find hands back an iterator that holds the node.
#pragma once

#include "../../concurrent/concurrent_map.h"
#include "../../concurrent/concurrent_unordered_map.h"
#include "../Core/Types.h"

namespace Sgcl {
    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    class ConcurrentDictionary {
    public:
        using KeyType = Key;
        using ValueType = Value;
        using InnerType = sgcl::concurrent_unordered_map<Key, Value, Hash, Equal>;
        using PairType = typename InnerType::value_type;
        using SizeType = size_t;
        using Iterator = typename InnerType::iterator;
        using ConstIterator = typename InnerType::const_iterator;

        ConcurrentDictionary() = default;

        explicit ConcurrentDictionary(SizeType buckets)
        : _m(buckets) {
        }

        // The array grown to at least `n` buckets up front
        void Reserve(SizeType n) {
            _m.reserve(n);
        }

        SizeType BucketCount() const noexcept {
            return _m.bucket_count();
        }

        Hash HashFunction() const {
            return _m.hash_function();
        }

        Equal KeyEqual() const {
            return _m.key_eq();
        }

        template<std::input_iterator It>
        ConcurrentDictionary(It first, It last)
        : _m(first, last) {
        }

        ConcurrentDictionary(std::initializer_list<PairType> il)
        : _m(il) {
        }

        ConcurrentDictionary(const ConcurrentDictionary&) = delete;
        ConcurrentDictionary& operator=(const ConcurrentDictionary&) = delete;

        // Added unless the key is there: whether it was added
        bool Add(const KeyType& key, const ValueType& value) {
            return _m.try_emplace(key, value).second;
        }

        bool Add(const KeyType& key, ValueType&& value) {
            return _m.try_emplace(key, std::move(value)).second;
        }

        template<class... A>
        bool Emplace(const KeyType& key, A&&... a) {
            return _m.try_emplace(key, std::forward<A>(a)...).second;
        }

        // The entry under the key, added from `a...` if absent: an iterator
        // that holds the node, so that the value may be used whatever the
        // other threads do to the entry meanwhile
        template<class... A>
        Iterator GetOrAdd(const KeyType& key, A&&... a) {
            return _m.try_emplace(key, std::forward<A>(a)...).first;
        }

        // A copy of the value under the key, or nothing: safe whatever
        // the other threads do to the entry meanwhile
        template<class K = KeyType>
        Optional<ValueType> TryGet(const K& key) const {
            auto it = _m.find(key);
            if (it == _m.end()) {
                return None;
            }
            return it->second;
        }

        // The entry under the key as an iterator, End() when absent: the
        // iterator holds the node, so the entry stays valid while the
        // iterator exists, erased by another thread or not. A K other
        // than the key type looks up without building a key when the
        // hash and the equality (the comparison) are transparent: a
        // string_view for a String
        template<class K = KeyType>
        Iterator Find(const K& key) noexcept {
            return _m.find(key);
        }

        template<class K = KeyType>
        ConstIterator Find(const K& key) const noexcept {
            return _m.find(key);
        }

        Iterator End() noexcept {
            return _m.end();
        }

        ConstIterator End() const noexcept {
            return _m.end();
        }

        template<class K = KeyType>
        bool ContainsKey(const K& key) const noexcept {
            return _m.contains(key);
        }

        template<class K = KeyType> requires (!std::is_convertible_v<const K&, ConstIterator>)
        bool Remove(const K& key) {
            return _m.erase(key) != 0;
        }

        Iterator Remove(ConstIterator pos) {
            return _m.erase(pos);
        }

        SizeType Count() const noexcept {
            return _m.size();
        }

        bool IsEmpty() const noexcept {
            return _m.empty();
        }

        void Clear() {
            _m.clear();
        }

        InnerType& Inner() noexcept {
            return _m;
        }

        const InnerType& Inner() const noexcept {
            return _m;
        }

    private:
        InnerType _m;
    };

    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    auto begin(ConcurrentDictionary<Key,Value,Hash,Equal>& m) noexcept {
        return m.Inner().begin();
    }

    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    auto end(ConcurrentDictionary<Key,Value,Hash,Equal>& m) noexcept {
        return m.Inner().end();
    }

    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    auto begin(const ConcurrentDictionary<Key,Value,Hash,Equal>& m) noexcept {
        return m.Inner().begin();
    }

    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    auto end(const ConcurrentDictionary<Key,Value,Hash,Equal>& m) noexcept {
        return m.Inner().end();
    }

    template<class Key, class Value, class Compare = std::less<Key>>
    class ConcurrentSortedDictionary {
    public:
        using KeyType = Key;
        using ValueType = Value;
        using InnerType = sgcl::concurrent_map<Key, Value, Compare>;
        using PairType = typename InnerType::value_type;
        using SizeType = size_t;
        using Iterator = typename InnerType::iterator;
        using ConstIterator = typename InnerType::const_iterator;

        ConcurrentSortedDictionary() = default;

        explicit ConcurrentSortedDictionary(const Compare& cmp)
        : _m(cmp) {
        }

        Compare KeyCompare() const {
            return _m.key_comp();
        }

        // The first entry not before the key, and the first after it
        template<class K = KeyType>
        Iterator LowerBound(const K& key) noexcept {
            return _m.lower_bound(key);
        }

        template<class K = KeyType>
        ConstIterator LowerBound(const K& key) const noexcept {
            return _m.lower_bound(key);
        }

        template<class K = KeyType>
        Iterator UpperBound(const K& key) noexcept {
            return _m.upper_bound(key);
        }

        template<class K = KeyType>
        ConstIterator UpperBound(const K& key) const noexcept {
            return _m.upper_bound(key);
        }

        template<std::input_iterator It>
        ConcurrentSortedDictionary(It first, It last)
        : _m(first, last) {
        }

        ConcurrentSortedDictionary(std::initializer_list<PairType> il)
        : _m(il) {
        }

        ConcurrentSortedDictionary(const ConcurrentSortedDictionary&) = delete;
        ConcurrentSortedDictionary& operator=(const ConcurrentSortedDictionary&) = delete;

        // Added unless the key is there: whether it was added
        bool Add(const KeyType& key, const ValueType& value) {
            return _m.try_emplace(key, value).second;
        }

        bool Add(const KeyType& key, ValueType&& value) {
            return _m.try_emplace(key, std::move(value)).second;
        }

        template<class... A>
        bool Emplace(const KeyType& key, A&&... a) {
            return _m.try_emplace(key, std::forward<A>(a)...).second;
        }

        // The entry under the key, added from `a...` if absent: an iterator
        // that holds the node, so that the value may be used whatever the
        // other threads do to the entry meanwhile
        template<class... A>
        Iterator GetOrAdd(const KeyType& key, A&&... a) {
            return _m.try_emplace(key, std::forward<A>(a)...).first;
        }

        // A copy of the value under the key, or nothing: safe whatever
        // the other threads do to the entry meanwhile
        template<class K = KeyType>
        Optional<ValueType> TryGet(const K& key) const {
            auto it = _m.find(key);
            if (it == _m.end()) {
                return None;
            }
            return it->second;
        }

        // The entry under the key as an iterator, End() when absent: the
        // iterator holds the node, so the entry stays valid while the
        // iterator exists, erased by another thread or not. A K other
        // than the key type looks up without building a key when the
        // hash and the equality (the comparison) are transparent: a
        // string_view for a String
        template<class K = KeyType>
        Iterator Find(const K& key) noexcept {
            return _m.find(key);
        }

        template<class K = KeyType>
        ConstIterator Find(const K& key) const noexcept {
            return _m.find(key);
        }

        Iterator End() noexcept {
            return _m.end();
        }

        ConstIterator End() const noexcept {
            return _m.end();
        }

        template<class K = KeyType>
        bool ContainsKey(const K& key) const noexcept {
            return _m.contains(key);
        }

        template<class K = KeyType> requires (!std::is_convertible_v<const K&, ConstIterator>)
        bool Remove(const K& key) {
            return _m.erase(key) != 0;
        }

        Iterator Remove(ConstIterator pos) {
            return _m.erase(pos);
        }

        SizeType Count() const noexcept {
            return _m.size();
        }

        bool IsEmpty() const noexcept {
            return _m.empty();
        }

        void Clear() {
            _m.clear();
        }

        InnerType& Inner() noexcept {
            return _m;
        }

        const InnerType& Inner() const noexcept {
            return _m;
        }

    private:
        InnerType _m;
    };

    template<class Key, class Value, class Compare = std::less<Key>>
    auto begin(ConcurrentSortedDictionary<Key,Value,Compare>& m) noexcept {
        return m.Inner().begin();
    }

    template<class Key, class Value, class Compare = std::less<Key>>
    auto end(ConcurrentSortedDictionary<Key,Value,Compare>& m) noexcept {
        return m.Inner().end();
    }

    template<class Key, class Value, class Compare = std::less<Key>>
    auto begin(const ConcurrentSortedDictionary<Key,Value,Compare>& m) noexcept {
        return m.Inner().begin();
    }

    template<class Key, class Value, class Compare = std::less<Key>>
    auto end(const ConcurrentSortedDictionary<Key,Value,Compare>& m) noexcept {
        return m.Inner().end();
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

