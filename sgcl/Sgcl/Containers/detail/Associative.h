//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the dictionaries and the sets share: the one container inside and
// the methods every kind has, written once. The public classes add the
// constructors and what is theirs (Reserve for the hashed, First and
// Last for the sorted).
#pragma once

#include "../../Core/Range.h"
#include "../../Core/Types.h"

#include <utility>

namespace Sgcl::detail {
    template<class C>
    class Associative {
    public:
        using InnerType = C;
        using SizeType = size_t;
        using Iterator = typename C::iterator;
        using ConstIterator = typename C::const_iterator;

        SizeType Count() const noexcept {
            return _c.size();
        }

        bool IsEmpty() const noexcept {
            return _c.empty();
        }

        void Clear() noexcept {
            _c.clear();
        }

        template<class Pred>
        SizeType RemoveAll(Pred pred) {
            return erase_if(_c, pred);
        }

        // The element an iterator names removed: the iterator after it
        Iterator RemoveAt(ConstIterator pos) {
            return _c.erase(pos);
        }

        Iterator RemoveRange(ConstIterator first, ConstIterator last) {
            return _c.erase(first, last);
        }

        void Swap(Associative& o) noexcept {
            _c.swap(o._c);
        }

        InnerType& Inner() noexcept {
            return _c;
        }

        const InnerType& Inner() const noexcept {
            return _c;
        }

        friend bool operator==(const Associative& a, const Associative& b) {
            return a._c == b._c;
        }

        // The sorted ones: lexicographical, in the order of the keys
        friend auto operator<=>(const Associative& a, const Associative& b)
        requires requires(const C& c) { c <=> c; } {
            return a._c <=> b._c;
        }

    protected:
        Associative() = default;

        explicit Associative(InnerType c) noexcept
        : _c(std::move(c)) {
        }

        InnerType _c;
    };

    // A dictionary of unique keys
    template<class C>
    class Map : public Associative<C> {
    public:
        using KeyType = typename C::key_type;
        using ValueType = typename C::mapped_type;
        using PairType = typename C::value_type;
        using typename Associative<C>::SizeType;
        using typename Associative<C>::Iterator;
        using typename Associative<C>::ConstIterator;

        // Added unless the key is there: whether it was added
        bool Add(const KeyType& key, const ValueType& value) {
            return this->_c.try_emplace(key, value).second;
        }

        bool Add(const KeyType& key, ValueType&& value) {
            return this->_c.try_emplace(key, std::move(value)).second;
        }

        bool Add(KeyType&& key, ValueType&& value) {
            return this->_c.try_emplace(std::move(key), std::move(value)).second;
        }

        template<class... A>
        bool Emplace(const KeyType& key, A&&... a) {
            return this->_c.try_emplace(key, std::forward<A>(a)...).second;
        }

        template<class... A>
        bool Emplace(KeyType&& key, A&&... a) {
            return this->_c.try_emplace(std::move(key), std::forward<A>(a)...).second;
        }

        // Added, or the value under the key replaced
        void Set(const KeyType& key, const ValueType& value) {
            this->_c.insert_or_assign(key, value);
        }

        void Set(const KeyType& key, ValueType&& value) {
            this->_c.insert_or_assign(key, std::move(value));
        }

        // The value under the key, made (value-initialized) if absent
        ValueType& operator[](const KeyType& key) {
            return this->_c[key];
        }

        ValueType& operator[](KeyType&& key) {
            return this->_c[std::move(key)];
        }

        // The value under the key, null when the key is absent. A K other
        // than the key type looks up without building a key when the hash
        // and the equality (or the comparison) are transparent
        template<class K = KeyType>
        ValueType* Find(const K& key) noexcept {
            auto it = this->_c.find(key);
            return it == this->_c.end() ? nullptr : &it->second;
        }

        template<class K = KeyType>
        const ValueType* Find(const K& key) const noexcept {
            auto it = this->_c.find(key);
            return it == this->_c.end() ? nullptr : &it->second;
        }

        // The entry as an iterator, end() when absent
        template<class K = KeyType>
        Iterator FindEntry(const K& key) noexcept {
            return this->_c.find(key);
        }

        template<class K = KeyType>
        ConstIterator FindEntry(const K& key) const noexcept {
            return this->_c.find(key);
        }

        template<class K = KeyType>
        bool ContainsKey(const K& key) const {
            return this->_c.contains(key);
        }

        // The value under the key, moved out, and the entry removed;
        // None when the key is absent
        template<class K = KeyType>
        Optional<ValueType> Take(const K& key) {
            return this->_c.take(key);
        }

        template<class K = KeyType>
        bool Remove(const K& key) {
            return this->_c.erase(key) != 0;
        }

    protected:
        using Associative<C>::Associative;
    };

    // A dictionary of a key with any number of values
    template<class C>
    class MultiMap : public Associative<C> {
    public:
        using KeyType = typename C::key_type;
        using ValueType = typename C::mapped_type;
        using PairType = typename C::value_type;
        using typename Associative<C>::SizeType;
        using typename Associative<C>::Iterator;
        using typename Associative<C>::ConstIterator;

        // Always added, in front of its equals: the iterator of the new pair
        Iterator Add(const KeyType& key, const ValueType& value) {
            return this->_c.emplace(key, value);
        }

        Iterator Add(const KeyType& key, ValueType&& value) {
            return this->_c.emplace(key, std::move(value));
        }

        template<class... A>
        Iterator Emplace(A&&... a) {
            return this->_c.emplace(std::forward<A>(a)...);
        }

        // The first value under the key, null when the key is absent
        template<class K = KeyType>
        ValueType* Find(const K& key) noexcept {
            auto it = this->_c.find(key);
            return it == this->_c.end() ? nullptr : &it->second;
        }

        template<class K = KeyType>
        const ValueType* Find(const K& key) const noexcept {
            auto it = this->_c.find(key);
            return it == this->_c.end() ? nullptr : &it->second;
        }

        template<class K = KeyType>
        Iterator FindEntry(const K& key) noexcept {
            return this->_c.find(key);
        }

        template<class K = KeyType>
        ConstIterator FindEntry(const K& key) const noexcept {
            return this->_c.find(key);
        }

        // Every pair under the key, for a range-for
        template<class K = KeyType>
        Range<Iterator> Values(const K& key) {
            return Range<Iterator>(this->_c.equal_range(key));
        }

        template<class K = KeyType>
        Range<ConstIterator> Values(const K& key) const {
            return Range<ConstIterator>(this->_c.equal_range(key));
        }

        template<class K = KeyType>
        SizeType CountOf(const K& key) const {
            return this->_c.count(key);
        }

        template<class K = KeyType>
        bool ContainsKey(const K& key) const {
            return this->_c.contains(key);
        }

        // Every pair under the key removed: how many
        template<class K = KeyType>
        SizeType Remove(const K& key) {
            return this->_c.erase(key);
        }

    protected:
        using Associative<C>::Associative;
    };

    // A set of unique values
    template<class C>
    class Set : public Associative<C> {
    public:
        using ValueType = typename C::value_type;
        using typename Associative<C>::SizeType;
        using typename Associative<C>::Iterator;
        using typename Associative<C>::ConstIterator;

        bool Add(const ValueType& value) {
            return this->_c.insert(value).second;
        }

        bool Add(ValueType&& value) {
            return this->_c.insert(std::move(value)).second;
        }

        template<class... A>
        bool Emplace(A&&... a) {
            return this->_c.emplace(std::forward<A>(a)...).second;
        }

        template<class K = ValueType>
        bool Contains(const K& value) const {
            return this->_c.contains(value);
        }

        template<class K = ValueType>
        ConstIterator FindEntry(const K& value) const noexcept {
            return this->_c.find(value);
        }

        template<class K = ValueType>
        bool Remove(const K& value) {
            return this->_c.erase(value) != 0;
        }

    protected:
        using Associative<C>::Associative;
    };

    // A set with any number of each value
    template<class C>
    class MultiSet : public Associative<C> {
    public:
        using ValueType = typename C::value_type;
        using typename Associative<C>::SizeType;
        using typename Associative<C>::Iterator;
        using typename Associative<C>::ConstIterator;

        // Always added, in front of its equals: the iterator of the new element
        Iterator Add(const ValueType& value) {
            return this->_c.insert(value);
        }

        Iterator Add(ValueType&& value) {
            return this->_c.insert(std::move(value));
        }

        template<class... A>
        Iterator Emplace(A&&... a) {
            return this->_c.emplace(std::forward<A>(a)...);
        }

        template<class K = ValueType>
        bool Contains(const K& value) const {
            return this->_c.contains(value);
        }

        template<class K = ValueType>
        ConstIterator FindEntry(const K& value) const noexcept {
            return this->_c.find(value);
        }

        template<class K = ValueType>
        SizeType CountOf(const K& value) const {
            return this->_c.count(value);
        }

        template<class K = ValueType>
        Range<ConstIterator> Values(const K& value) const {
            return Range<ConstIterator>(this->_c.equal_range(value));
        }

        template<class K = ValueType>
        SizeType Remove(const K& value) {
            return this->_c.erase(value);
        }

    protected:
        using Associative<C>::Associative;
    };

    template<class C>
    auto begin(Associative<C>& c) noexcept {
        return c.Inner().begin();
    }

    template<class C>
    auto end(Associative<C>& c) noexcept {
        return c.Inner().end();
    }

    template<class C>
    auto begin(const Associative<C>& c) noexcept {
        return c.Inner().begin();
    }

    template<class C>
    auto end(const Associative<C>& c) noexcept {
        return c.Inner().end();
    }

    template<class C>
    void swap(Associative<C>& a, Associative<C>& b) noexcept {
        a.Swap(b);
    }
}

