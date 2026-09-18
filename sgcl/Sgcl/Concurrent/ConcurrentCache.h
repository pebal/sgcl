//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ConcurrentCache<Key, Value>: a key-value cache shared by any number of
// threads, bounded by a capacity and, if asked, a time to live, evicting
// the entries least recently used (Guava's Cache, Caffeine). Over the
// lock-free ConcurrentDictionary: TryGet is wait-free and never writes
// anything shared; Set evicts by sampling when the size is past the
// capacity. TryGet copies the value out.
#pragma once

#include "../../concurrent/concurrent_cache.h"
#include "../Core/Types.h"

namespace Sgcl {
    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    class ConcurrentCache {
    public:
        using KeyType = Key;
        using ValueType = Value;
        using InnerType = sgcl::concurrent_cache<Key, Value, Hash, Equal>;
        using SizeType = size_t;
        using Clock = typename InnerType::clock;
        using Duration = typename InnerType::duration;

        static constexpr unsigned DefaultSample = InnerType::DefaultSample;

        // A cache of `capacity` entries, with no time to live (zero) or
        // one; `sample` entries looked at per eviction
        explicit ConcurrentCache(SizeType capacity, Duration ttl = Duration::zero(), unsigned sample = DefaultSample)
        : _c(capacity, ttl, sample) {
        }

        ConcurrentCache(const ConcurrentCache&) = delete;
        ConcurrentCache& operator=(const ConcurrentCache&) = delete;

        // A copy of the value under the key, or None: absent, or older
        // than the time to live. A K other than the key type looks up
        // without building a key when Hash and Equal are transparent: a
        // string_view for a String
        template<class K = KeyType>
        Optional<ValueType> TryGet(const K& key) {
            return _c.get(key);
        }

        // The value set under the key, replacing one there; the eviction
        // when the size is past the capacity
        void Set(const KeyType& key, const ValueType& value) {
            _c.put(key, value);
        }

        void Set(const KeyType& key, ValueType&& value) {
            _c.put(key, std::move(value));
        }

        // The value under the key, made by factory() and set when it is
        // absent or stale; two threads that miss at once both call the
        // factory, one result wins and both get it
        template<class F>
        ValueType GetOrAdd(const KeyType& key, F&& factory) {
            return _c.get_or_compute(key, std::forward<F>(factory));
        }

        template<class K = KeyType>
        bool Remove(const K& key) {
            return _c.erase(key);
        }

        void Clear() {
            _c.clear();
        }

        SizeType Count() const noexcept {
            return _c.size();
        }

        bool IsEmpty() const noexcept {
            return _c.empty();
        }

        SizeType Capacity() const noexcept {
            return _c.capacity();
        }

        Duration TimeToLive() const noexcept {
            return _c.ttl();
        }

        unsigned SampleSize() const noexcept {
            return _c.sample_size();
        }

        // The gets that found a value and the gets that did not
        uint64_t Hits() const noexcept {
            return _c.hits();
        }

        uint64_t Misses() const noexcept {
            return _c.misses();
        }

        InnerType& Inner() noexcept {
            return _c;
        }

        const InnerType& Inner() const noexcept {
            return _c;
        }

    private:
        InnerType _c;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
