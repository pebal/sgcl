//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// WeakDictionary<Key, Value>, WeakMultiDictionary and WeakHashSet<Key>: the
// keys are objects held weakly (Java's WeakHashMap); an entry whose
// object the collector has found unreachable is gone from the next walk
// and freed by Sweep().
#pragma once

#include "../../containers/weak_map.h"
#include "../../containers/weak_set.h"
#include "../Core/Ptr.h"
#include "../Core/Range.h"

namespace Sgcl {
    template<class Key, class Value>
    class WeakDictionary {
    public:
        using KeyType = Ptr<Key>;
        using ValueType = Value;
        using InnerType = sgcl::weak_map<Key, Value>;
        using SizeType = size_t;
        using Iterator = typename InnerType::iterator;

        WeakDictionary() = default;
        WeakDictionary(WeakDictionary&&) noexcept = default;
        WeakDictionary& operator=(WeakDictionary&&) noexcept = default;
        WeakDictionary(const WeakDictionary&) = delete;
        WeakDictionary& operator=(const WeakDictionary&) = delete;

        // Added unless the object is there: whether it was added
        bool Add(const KeyType& object, const Value& value) {
            return _m.insert(object.Inner(), value).second;
        }

        bool Add(const KeyType& object, Value&& value) {
            return _m.insert(object.Inner(), std::move(value)).second;
        }

        template<class... A>
        bool Emplace(const KeyType& object, A&&... a) {
            return _m.emplace(object.Inner(), std::forward<A>(a)...).second;
        }

        void Set(const KeyType& object, const Value& value) {
            _m.insert_or_assign(object.Inner(), value);
        }

        void Set(const KeyType& object, Value&& value) {
            _m.insert_or_assign(object.Inner(), std::move(value));
        }

        Value& operator[](const KeyType& object) {
            return _m[object.Inner()];
        }

        Value* Find(const KeyType& object) {
            auto it = _m.find(object.Inner());
            return it == _m.end() ? nullptr : &it->value;
        }

        bool ContainsKey(const KeyType& object) const {
            return _m.contains(object.Inner());
        }

        bool Remove(const KeyType& object) {
            return _m.erase(object.Inner()) != 0;
        }

        // The entry an iterator names dropped: the next live entry
        Iterator RemoveAt(Iterator pos) {
            return _m.erase(pos);
        }

        // The entry as an iterator (it->key, it->value), end() when absent
        Iterator FindEntry(const KeyType& object) {
            return _m.find(object.Inner());
        }

        // The entries of the dead objects freed: how many
        SizeType Sweep() {
            return _m.sweep();
        }

        SizeType Count() const noexcept {
            return _m.size();
        }

        bool IsEmpty() const noexcept {
            return _m.empty();
        }

        void Clear() noexcept {
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

    template<class K, class V>
    auto begin(WeakDictionary<K, V>& m) noexcept {
        return m.Inner().begin();
    }

    template<class K, class V>
    auto end(WeakDictionary<K, V>& m) noexcept {
        return m.Inner().end();
    }

    template<class Key, class Value>
    class WeakMultiDictionary {
    public:
        using KeyType = Ptr<Key>;
        using ValueType = Value;
        using InnerType = sgcl::weak_multimap<Key, Value>;
        using SizeType = size_t;
        using Iterator = typename InnerType::iterator;

        WeakMultiDictionary() = default;
        WeakMultiDictionary(WeakMultiDictionary&&) noexcept = default;
        WeakMultiDictionary& operator=(WeakMultiDictionary&&) noexcept = default;
        WeakMultiDictionary(const WeakMultiDictionary&) = delete;
        WeakMultiDictionary& operator=(const WeakMultiDictionary&) = delete;

        void Add(const KeyType& object, const Value& value) {
            _m.insert(object.Inner(), value);
        }

        void Add(const KeyType& object, Value&& value) {
            _m.insert(object.Inner(), std::move(value));
        }

        template<class... A>
        void Emplace(const KeyType& object, A&&... a) {
            _m.emplace(object.Inner(), std::forward<A>(a)...);
        }

        Value* Find(const KeyType& object) {
            auto it = _m.find(object.Inner());
            return it == _m.end() ? nullptr : &it->value;
        }

        Range<Iterator> Values(const KeyType& object) {
            return Range<Iterator>(_m.equal_range(object.Inner()));
        }

        SizeType CountOf(const KeyType& object) const {
            return _m.count(object.Inner());
        }

        bool ContainsKey(const KeyType& object) const {
            return _m.contains(object.Inner());
        }

        SizeType Remove(const KeyType& object) {
            return _m.erase(object.Inner());
        }

        Iterator RemoveAt(Iterator pos) {
            return _m.erase(pos);
        }

        Iterator FindEntry(const KeyType& object) {
            return _m.find(object.Inner());
        }

        SizeType Sweep() {
            return _m.sweep();
        }

        SizeType Count() const noexcept {
            return _m.size();
        }

        bool IsEmpty() const noexcept {
            return _m.empty();
        }

        void Clear() noexcept {
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

    template<class K, class V>
    auto begin(WeakMultiDictionary<K, V>& m) noexcept {
        return m.Inner().begin();
    }

    template<class K, class V>
    auto end(WeakMultiDictionary<K, V>& m) noexcept {
        return m.Inner().end();
    }

    template<class Key>
    class WeakHashSet {
    public:
        using ValueType = Ptr<Key>;
        using InnerType = sgcl::weak_set<Key>;
        using SizeType = size_t;
        using Iterator = typename InnerType::iterator;

        WeakHashSet() = default;
        WeakHashSet(WeakHashSet&&) noexcept = default;
        WeakHashSet& operator=(WeakHashSet&&) noexcept = default;
        WeakHashSet(const WeakHashSet&) = delete;
        WeakHashSet& operator=(const WeakHashSet&) = delete;

        bool Add(const ValueType& object) {
            return _s.insert(object.Inner()).second;
        }

        bool Contains(const ValueType& object) const {
            return _s.contains(object.Inner());
        }

        bool Remove(const ValueType& object) {
            return _s.erase(object.Inner()) != 0;
        }

        Iterator RemoveAt(Iterator pos) {
            return _s.erase(pos);
        }

        Iterator FindEntry(const ValueType& object) {
            return _s.find(object.Inner());
        }

        SizeType Sweep() {
            return _s.sweep();
        }

        SizeType Count() const noexcept {
            return _s.size();
        }

        bool IsEmpty() const noexcept {
            return _s.empty();
        }

        void Clear() noexcept {
            _s.clear();
        }

        InnerType& Inner() noexcept {
            return _s;
        }

        const InnerType& Inner() const noexcept {
            return _s;
        }

    private:
        InnerType _s;
    };

    template<class K>
    auto begin(WeakHashSet<K>& s) noexcept {
        return s.Inner().begin();
    }

    template<class K>
    auto end(WeakHashSet<K>& s) noexcept {
        return s.Inner().end();
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

