//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ConcurrentWeakDictionary<Key, Value> and ConcurrentWeakHashSet<Key>: the
// WeakDictionary and the WeakHashSet shared by any number of threads
// without a lock (a Java WeakHashMap over a ConcurrentHashMap): the keys
// are objects held weakly, an entry whose object the collector has found
// unreachable is gone from the next walk and freed by a sweep, which the
// inserting threads run by themselves every so many insertions and
// Sweep() runs on demand. The lookups are those of ConcurrentDictionary:
// TryGet copies the value out, Find hands back an iterator that holds
// the node and the object.
#pragma once

#include "../../concurrent/concurrent_weak_map.h"
#include "../../concurrent/concurrent_weak_set.h"
#include "../Core/Ptr.h"
#include "../Core/Types.h"

namespace Sgcl {
    template<class Key, class Value>
    class ConcurrentWeakDictionary {
    public:
        using KeyType = Ptr<Key>;
        using ValueType = Value;
        using InnerType = sgcl::concurrent_weak_map<Key, Value>;
        using SizeType = size_t;
        using Iterator = typename InnerType::iterator;

        ConcurrentWeakDictionary() = default;
        ConcurrentWeakDictionary(const ConcurrentWeakDictionary&) = delete;
        ConcurrentWeakDictionary& operator=(const ConcurrentWeakDictionary&) = delete;

        // Added unless the object is there: whether it was added; of two
        // threads adding the same object exactly one gets true
        bool Add(const KeyType& object, const Value& value) {
            return _m.insert(object.Inner(), value).second;
        }

        bool Add(const KeyType& object, Value&& value) {
            return _m.insert(object.Inner(), std::move(value)).second;
        }

        template<class... A>
        bool Emplace(const KeyType& object, A&&... a) {
            return _m.try_emplace(object.Inner(), std::forward<A>(a)...).second;
        }

        // The entry of the object, added from `a...` if absent: an iterator
        // that holds the node and the object, so that the value may be
        // used whatever the other threads do to the entry meanwhile
        template<class... A>
        Iterator GetOrAdd(const KeyType& object, A&&... a) {
            return _m.try_emplace(object.Inner(), std::forward<A>(a)...).first;
        }

        // A copy of the value of the object, or nothing: safe whatever
        // the other threads do to the entry meanwhile
        Optional<ValueType> TryGet(const KeyType& object) {
            auto it = _m.find(object.Inner());
            if (it == _m.end()) {
                return None;
            }
            return it->value;
        }

        // The entry of the object as an iterator (it->key, it->value),
        // End() when absent: the iterator holds the node and the object,
        // so the entry stays valid while the iterator exists, removed by
        // another thread or not
        Iterator Find(const KeyType& object) noexcept {
            return _m.find(object.Inner());
        }

        Iterator End() noexcept {
            return _m.end();
        }

        bool ContainsKey(const KeyType& object) const noexcept {
            return _m.contains(object.Inner());
        }

        bool Remove(const KeyType& object) {
            return _m.erase(object.Inner()) != 0;
        }

        // The entry an iterator names dropped, if it is still there: the
        // next live entry
        Iterator Remove(Iterator pos) {
            return _m.erase(pos);
        }

        // The entries of the dead objects freed: how many, or 0 at once
        // when another thread's sweep is under way
        SizeType Sweep() {
            return _m.sweep();
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

    template<class K, class V>
    auto begin(ConcurrentWeakDictionary<K, V>& m) noexcept {
        return m.Inner().begin();
    }

    template<class K, class V>
    auto end(ConcurrentWeakDictionary<K, V>& m) noexcept {
        return m.Inner().end();
    }

    template<class Key>
    class ConcurrentWeakHashSet {
    public:
        using ValueType = Ptr<Key>;
        using InnerType = sgcl::concurrent_weak_set<Key>;
        using SizeType = size_t;
        using Iterator = typename InnerType::iterator;

        ConcurrentWeakHashSet() = default;
        ConcurrentWeakHashSet(const ConcurrentWeakHashSet&) = delete;
        ConcurrentWeakHashSet& operator=(const ConcurrentWeakHashSet&) = delete;

        // Added unless the object is there: whether it was added; of two
        // threads adding the same object exactly one gets true
        bool Add(const ValueType& object) {
            return _s.insert(object.Inner()).second;
        }

        bool Contains(const ValueType& object) const noexcept {
            return _s.contains(object.Inner());
        }

        bool Remove(const ValueType& object) {
            return _s.erase(object.Inner()) != 0;
        }

        Iterator Remove(Iterator pos) {
            return _s.erase(pos);
        }

        // The entry of the object as an iterator (*it the object, held),
        // End() when absent
        Iterator Find(const ValueType& object) noexcept {
            return _s.find(object.Inner());
        }

        Iterator End() noexcept {
            return _s.end();
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

        void Clear() {
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
    auto begin(ConcurrentWeakHashSet<K>& s) noexcept {
        return s.Inner().begin();
    }

    template<class K>
    auto end(ConcurrentWeakHashSet<K>& s) noexcept {
        return s.Inner().end();
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
