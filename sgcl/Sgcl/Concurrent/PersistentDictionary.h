//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// PersistentDictionary<Key, Value> and PersistentSet<T>: the persistent
// hash map and set of Clojure and Scala, a hash array mapped trie. Every
// change (Set, Add, Remove) returns a new dictionary and leaves this one
// as it was, the two sharing everything but the path that changed; any
// number of threads read any version without a lock. Find hands back a
// pointer to the value, null when the key is absent. A value of two
// words; a version is published through a CopyOnWrite or an Atomic.
#pragma once

#include "../../concurrent/persistent_map.h"
#include "../../concurrent/persistent_set.h"
#include "../Core/Types.h"

#include <initializer_list>
#include <iterator>

namespace Sgcl {
    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    class PersistentDictionary {
    public:
        using KeyType = Key;
        using ValueType = Value;
        using InnerType = sgcl::persistent_map<Key, Value, Hash, Equal>;
        using PairType = typename InnerType::value_type;
        using SizeType = size_t;
        using Iterator = typename InnerType::const_iterator;
        using ConstIterator = typename InnerType::const_iterator;

        PersistentDictionary() = default;

        template<std::input_iterator It>
        PersistentDictionary(It first, It last)
        : _m(first, last) {
        }

        PersistentDictionary(std::initializer_list<PairType> il)
        : _m(il) {
        }

        explicit PersistentDictionary(InnerType m) noexcept
        : _m(std::move(m)) {
        }

        PersistentDictionary(const PersistentDictionary&) noexcept = default;
        PersistentDictionary(PersistentDictionary&&) noexcept = default;
        PersistentDictionary& operator=(const PersistentDictionary&) noexcept = default;
        PersistentDictionary& operator=(PersistentDictionary&&) noexcept = default;

        SizeType Count() const noexcept {
            return _m.size();
        }

        bool IsEmpty() const noexcept {
            return _m.empty();
        }

        Hash HashFunction() const {
            return _m.hash_function();
        }

        Equal KeyEqual() const {
            return _m.key_eq();
        }

        // The value under the key, null when the key is absent. A K other
        // than the key type looks up without building a key when the hash
        // and the equality are transparent: a string_view for a String
        template<class K = KeyType>
        const ValueType* Find(const K& key) const {
            return _m.find(key);
        }

        // A copy of the value under the key, or nothing
        template<class K = KeyType>
        Optional<ValueType> TryGet(const K& key) const {
            auto v = _m.find(key);
            if (!v) {
                return None;
            }
            return *v;
        }

        // The value under the key; std::out_of_range when absent
        template<class K = KeyType>
        const ValueType& At(const K& key) const {
            return _m.at(key);
        }

        template<class K = KeyType>
        bool ContainsKey(const K& key) const {
            return _m.contains(key);
        }

        // The dictionary with `value` under `key`, added or in place of
        // the value there: this one unchanged
        PersistentDictionary Set(const KeyType& key, const ValueType& value) const {
            return PersistentDictionary(_m.insert(key, value));
        }

        PersistentDictionary Set(const KeyType& key, ValueType&& value) const {
            return PersistentDictionary(_m.insert(key, std::move(value)));
        }

        PersistentDictionary Set(KeyType&& key, ValueType&& value) const {
            return PersistentDictionary(_m.insert(std::move(key), std::move(value)));
        }

        // The same, the value built from the arguments
        template<class... A>
        PersistentDictionary Emplace(const KeyType& key, A&&... a) const {
            return PersistentDictionary(_m.emplace(key, std::forward<A>(a)...));
        }

        // The dictionary without the key: this one unchanged, the same
        // contents when the key is absent
        template<class K = KeyType>
        PersistentDictionary Remove(const K& key) const {
            return PersistentDictionary(_m.erase(key));
        }

        InnerType& Inner() noexcept {
            return _m;
        }

        const InnerType& Inner() const noexcept {
            return _m;
        }

        friend bool operator==(const PersistentDictionary& a, const PersistentDictionary& b) {
            return a._m == b._m;
        }

        friend bool operator!=(const PersistentDictionary& a, const PersistentDictionary& b) {
            return !(a._m == b._m);
        }

    private:
        InnerType _m;
    };

    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    class PersistentSet {
    public:
        using ValueType = T;
        using InnerType = sgcl::persistent_set<T, Hash, Equal>;
        using SizeType = size_t;
        using Iterator = typename InnerType::const_iterator;
        using ConstIterator = typename InnerType::const_iterator;

        PersistentSet() = default;

        template<std::input_iterator It>
        PersistentSet(It first, It last)
        : _s(first, last) {
        }

        PersistentSet(std::initializer_list<T> il)
        : _s(il) {
        }

        explicit PersistentSet(InnerType s) noexcept
        : _s(std::move(s)) {
        }

        PersistentSet(const PersistentSet&) noexcept = default;
        PersistentSet(PersistentSet&&) noexcept = default;
        PersistentSet& operator=(const PersistentSet&) noexcept = default;
        PersistentSet& operator=(PersistentSet&&) noexcept = default;

        SizeType Count() const noexcept {
            return _s.size();
        }

        bool IsEmpty() const noexcept {
            return _s.empty();
        }

        Hash HashFunction() const {
            return _s.hash_function();
        }

        Equal KeyEqual() const {
            return _s.key_eq();
        }

        // Whether the value is in the set; a K other than T looks up
        // without building one when the hash and the equality are transparent
        template<class K = ValueType>
        bool Contains(const K& value) const {
            return _s.contains(value);
        }

        // The element equal to the value, null when there is none
        template<class K = ValueType>
        const ValueType* Find(const K& value) const {
            return _s.find(value);
        }

        // The set with the value: this one unchanged
        PersistentSet Add(const ValueType& value) const {
            return PersistentSet(_s.insert(value));
        }

        PersistentSet Add(ValueType&& value) const {
            return PersistentSet(_s.insert(std::move(value)));
        }

        // The set without the value: this one unchanged
        template<class K = ValueType>
        PersistentSet Remove(const K& value) const {
            return PersistentSet(_s.erase(value));
        }

        InnerType& Inner() noexcept {
            return _s;
        }

        const InnerType& Inner() const noexcept {
            return _s;
        }

        friend bool operator==(const PersistentSet& a, const PersistentSet& b) {
            return a._s == b._s;
        }

        friend bool operator!=(const PersistentSet& a, const PersistentSet& b) {
            return !(a._s == b._s);
        }

    private:
        InnerType _s;
    };

    template<class K, class V, class H, class E>
    auto begin(const PersistentDictionary<K, V, H, E>& d) noexcept {
        return d.Inner().begin();
    }

    template<class K, class V, class H, class E>
    auto end(const PersistentDictionary<K, V, H, E>& d) noexcept {
        return d.Inner().end();
    }

    template<class T, class H, class E>
    auto begin(const PersistentSet<T, H, E>& s) noexcept {
        return s.Inner().begin();
    }

    template<class T, class H, class E>
    auto end(const PersistentSet<T, H, E>& s) noexcept {
        return s.Inner().end();
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
