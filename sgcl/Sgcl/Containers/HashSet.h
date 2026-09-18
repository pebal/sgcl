//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// HashSet<T> (a hash table of unique values) and HashMultiSet<T> (any
// number of each).
#pragma once

#include "../../containers/unordered_multiset.h"
#include "../../containers/unordered_set.h"
#include "detail/Associative.h"

namespace Sgcl {
    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    class HashSet : public detail::Set<sgcl::unordered_set<T, Hash, Equal>> {
        using Base = detail::Set<sgcl::unordered_set<T, Hash, Equal>>;

    public:
        using typename Base::InnerType;
        using typename Base::SizeType;

        HashSet() = default;

        template<std::input_iterator It>
        HashSet(It first, It last)
        : Base(InnerType(first, last)) {
        }

        HashSet(std::initializer_list<typename InnerType::value_type> il)
        : Base(InnerType(il)) {
        }

        explicit HashSet(InnerType c) noexcept
        : Base(std::move(c)) {
        }

        explicit HashSet(SizeType buckets)
        : Base(InnerType(buckets)) {
        }

        HashSet(const HashSet&) = default;
        HashSet(HashSet&&) noexcept = default;
        HashSet& operator=(const HashSet&) = default;
        HashSet& operator=(HashSet&&) noexcept = default;

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

    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    class HashMultiSet : public detail::MultiSet<sgcl::unordered_multiset<T, Hash, Equal>> {
        using Base = detail::MultiSet<sgcl::unordered_multiset<T, Hash, Equal>>;

    public:
        using typename Base::InnerType;
        using typename Base::SizeType;

        HashMultiSet() = default;

        template<std::input_iterator It>
        HashMultiSet(It first, It last)
        : Base(InnerType(first, last)) {
        }

        HashMultiSet(std::initializer_list<typename InnerType::value_type> il)
        : Base(InnerType(il)) {
        }

        explicit HashMultiSet(InnerType c) noexcept
        : Base(std::move(c)) {
        }

        explicit HashMultiSet(SizeType buckets)
        : Base(InnerType(buckets)) {
        }

        HashMultiSet(const HashMultiSet&) = default;
        HashMultiSet(HashMultiSet&&) noexcept = default;
        HashMultiSet& operator=(const HashMultiSet&) = default;
        HashMultiSet& operator=(HashMultiSet&&) noexcept = default;

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

    // Deduction from an initializer list, as for the hash sets of std
    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    HashSet(std::initializer_list<T>, size_t = 0, Hash = Hash(), Equal = Equal()) -> HashSet<T, Hash, Equal>;
    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    HashMultiSet(std::initializer_list<T>, size_t = 0, Hash = Hash(), Equal = Equal()) -> HashMultiSet<T, Hash, Equal>;
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

