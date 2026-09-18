//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ConcurrentHashSet<T> (a lock-free hash table) and ConcurrentSortedSet<T>
// (a lock-free skip list, in order). Shared by any number of threads.
#pragma once

#include "../../concurrent/concurrent_set.h"
#include "../../concurrent/concurrent_unordered_set.h"

namespace Sgcl {
    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    class ConcurrentHashSet {
    public:
        using ValueType = T;
        using InnerType = sgcl::concurrent_unordered_set<T, Hash, Equal>;
        using SizeType = size_t;
        using Iterator = typename InnerType::iterator;
        using ConstIterator = typename InnerType::const_iterator;

        ConcurrentHashSet() = default;

        explicit ConcurrentHashSet(SizeType buckets)
        : _s(buckets) {
        }

        // The array grown to at least `n` buckets up front
        void Reserve(SizeType n) {
            _s.reserve(n);
        }

        SizeType BucketCount() const noexcept {
            return _s.bucket_count();
        }

        Hash HashFunction() const {
            return _s.hash_function();
        }

        Equal KeyEqual() const {
            return _s.key_eq();
        }

        template<std::input_iterator It>
        ConcurrentHashSet(It first, It last)
        : _s(first, last) {
        }

        ConcurrentHashSet(std::initializer_list<T> il)
        : _s(il) {
        }

        ConcurrentHashSet(const ConcurrentHashSet&) = delete;
        ConcurrentHashSet& operator=(const ConcurrentHashSet&) = delete;

        bool Add(const T& value) {
            return _s.insert(value).second;
        }

        bool Add(T&& value) {
            return _s.insert(std::move(value)).second;
        }

        template<class... A>
        bool Emplace(A&&... a) {
            return _s.emplace(std::forward<A>(a)...).second;
        }

        template<class K = T>
        bool Contains(const K& value) const noexcept {
            return _s.contains(value);
        }

        // The element as an iterator that holds its node, End() when absent
        template<class K = T>
        Iterator FindEntry(const K& value) noexcept {
            return _s.find(value);
        }

        template<class K = T>
        ConstIterator FindEntry(const K& value) const noexcept {
            return _s.find(value);
        }

        Iterator End() noexcept {
            return _s.end();
        }

        ConstIterator End() const noexcept {
            return _s.end();
        }

        Iterator RemoveAt(ConstIterator pos) {
            return _s.erase(pos);
        }

        template<class K = T> requires (!std::is_convertible_v<const K&, ConstIterator>)
        bool Remove(const K& value) {
            return _s.erase(value) != 0;
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

    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    auto begin(ConcurrentHashSet<T,Hash,Equal>& s) noexcept {
        return s.Inner().begin();
    }

    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    auto end(ConcurrentHashSet<T,Hash,Equal>& s) noexcept {
        return s.Inner().end();
    }

    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    auto begin(const ConcurrentHashSet<T,Hash,Equal>& s) noexcept {
        return s.Inner().begin();
    }

    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    auto end(const ConcurrentHashSet<T,Hash,Equal>& s) noexcept {
        return s.Inner().end();
    }

    template<class T, class Compare = std::less<T>>
    class ConcurrentSortedSet {
    public:
        using ValueType = T;
        using InnerType = sgcl::concurrent_set<T, Compare>;
        using SizeType = size_t;
        using Iterator = typename InnerType::iterator;
        using ConstIterator = typename InnerType::const_iterator;

        ConcurrentSortedSet() = default;

        explicit ConcurrentSortedSet(const Compare& cmp)
        : _s(cmp) {
        }

        Compare KeyCompare() const {
            return _s.key_comp();
        }

        template<class K = T>
        Iterator LowerBound(const K& value) noexcept {
            return _s.lower_bound(value);
        }

        template<class K = T>
        ConstIterator LowerBound(const K& value) const noexcept {
            return _s.lower_bound(value);
        }

        template<class K = T>
        Iterator UpperBound(const K& value) noexcept {
            return _s.upper_bound(value);
        }

        template<class K = T>
        ConstIterator UpperBound(const K& value) const noexcept {
            return _s.upper_bound(value);
        }

        template<std::input_iterator It>
        ConcurrentSortedSet(It first, It last)
        : _s(first, last) {
        }

        ConcurrentSortedSet(std::initializer_list<T> il)
        : _s(il) {
        }

        ConcurrentSortedSet(const ConcurrentSortedSet&) = delete;
        ConcurrentSortedSet& operator=(const ConcurrentSortedSet&) = delete;

        bool Add(const T& value) {
            return _s.insert(value).second;
        }

        bool Add(T&& value) {
            return _s.insert(std::move(value)).second;
        }

        template<class... A>
        bool Emplace(A&&... a) {
            return _s.emplace(std::forward<A>(a)...).second;
        }

        template<class K = T>
        bool Contains(const K& value) const noexcept {
            return _s.contains(value);
        }

        // The element as an iterator that holds its node, End() when absent
        template<class K = T>
        Iterator FindEntry(const K& value) noexcept {
            return _s.find(value);
        }

        template<class K = T>
        ConstIterator FindEntry(const K& value) const noexcept {
            return _s.find(value);
        }

        Iterator End() noexcept {
            return _s.end();
        }

        ConstIterator End() const noexcept {
            return _s.end();
        }

        Iterator RemoveAt(ConstIterator pos) {
            return _s.erase(pos);
        }

        template<class K = T> requires (!std::is_convertible_v<const K&, ConstIterator>)
        bool Remove(const K& value) {
            return _s.erase(value) != 0;
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

    template<class T, class Compare = std::less<T>>
    auto begin(ConcurrentSortedSet<T,Compare>& s) noexcept {
        return s.Inner().begin();
    }

    template<class T, class Compare = std::less<T>>
    auto end(ConcurrentSortedSet<T,Compare>& s) noexcept {
        return s.Inner().end();
    }

    template<class T, class Compare = std::less<T>>
    auto begin(const ConcurrentSortedSet<T,Compare>& s) noexcept {
        return s.Inner().begin();
    }

    template<class T, class Compare = std::less<T>>
    auto end(const ConcurrentSortedSet<T,Compare>& s) noexcept {
        return s.Inner().end();
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

