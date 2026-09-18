//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Dictionary<Key, Value> (a hash table, the nodes on the managed heap) and
// MultiDictionary<Key, Value> (any number of values under a key). Find hands
// back a pointer to the value, null when the key is absent; nothing
// throws, nothing is checked.
#pragma once

#include "../../containers/unordered_map.h"
#include "../../containers/unordered_multimap.h"
#include "../Core/Types.h"
#include "detail/Associative.h"

namespace Sgcl {
    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    class Dictionary : public detail::Map<sgcl::unordered_map<Key, Value, Hash, Equal>> {
        using Base = detail::Map<sgcl::unordered_map<Key, Value, Hash, Equal>>;

    public:
        using typename Base::InnerType;
        using typename Base::SizeType;

        Dictionary() = default;

        template<std::input_iterator It>
        Dictionary(It first, It last)
        : Base(InnerType(first, last)) {
        }

        Dictionary(std::initializer_list<typename InnerType::value_type> il)
        : Base(InnerType(il)) {
        }

        explicit Dictionary(InnerType c) noexcept
        : Base(std::move(c)) {
        }

        explicit Dictionary(SizeType buckets)
        : Base(InnerType(buckets)) {
        }

        Dictionary(const Dictionary&) = default;
        Dictionary(Dictionary&&) noexcept = default;
        Dictionary& operator=(const Dictionary&) = default;
        Dictionary& operator=(Dictionary&&) noexcept = default;

        // Room for `n` elements without a rehash
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

    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    class MultiDictionary : public detail::MultiMap<sgcl::unordered_multimap<Key, Value, Hash, Equal>> {
        using Base = detail::MultiMap<sgcl::unordered_multimap<Key, Value, Hash, Equal>>;

    public:
        using typename Base::InnerType;
        using typename Base::SizeType;

        MultiDictionary() = default;

        template<std::input_iterator It>
        MultiDictionary(It first, It last)
        : Base(InnerType(first, last)) {
        }

        MultiDictionary(std::initializer_list<typename InnerType::value_type> il)
        : Base(InnerType(il)) {
        }

        explicit MultiDictionary(InnerType c) noexcept
        : Base(std::move(c)) {
        }

        explicit MultiDictionary(SizeType buckets)
        : Base(InnerType(buckets)) {
        }

        MultiDictionary(const MultiDictionary&) = default;
        MultiDictionary(MultiDictionary&&) noexcept = default;
        MultiDictionary& operator=(const MultiDictionary&) = default;
        MultiDictionary& operator=(MultiDictionary&&) noexcept = default;

        // Room for `n` elements without a rehash
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
    Dictionary(std::initializer_list<Pair<Key, Value>>, size_t = 0, Hash = Hash(), Equal = Equal()) -> Dictionary<Key, Value, Hash, Equal>;
    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    MultiDictionary(std::initializer_list<Pair<Key, Value>>, size_t = 0, Hash = Hash(), Equal = Equal()) -> MultiDictionary<Key, Value, Hash, Equal>;
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

