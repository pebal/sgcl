//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "detail/hash_table.h"
#include "mixin/mixin.h"

namespace sgcl {
    // A hash map iterated in insertion order: Java's LinkedHashMap, the
    // dict of Python, .NET's OrderedDictionary. The interface of
    // ordered_map (the same table under it, detail/hash_table.h, so the
    // lookups cost the same and the bucket interface is there), with the
    // elements on one more list, in the order they were inserted: begin
    // to end walks it, both ways (bidirectional iterators, rbegin), front
    // is the oldest element and back the newest, a copy keeps the order,
    // an erase takes an element out of it, a re-insert of a present key
    // leaves it where it was. to_back and to_front move an element to
    // the end (the start) of the order: touched last, or first, from now
    // on, which is what a cache with an eviction order wants (to_back on
    // a hit, erase(begin()) when full). Two words more per node than
    // ordered_map. The map and its node handles hold tracked pointers:
    // they live on a stack or inside a managed object; an iterator is
    // one raw node pointer and may live anywhere, invalid once its
    // element is erased, as in std.
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class ordered_map
    : public detail::HashTable<detail::HashMapTraits<Key, T, Hash, KeyEqual, true, true>>
    , public mixin::enumerable<ordered_map<Key, T, Hash, KeyEqual>>
    , public mixin::lookup<ordered_map<Key, T, Hash, KeyEqual>> {
        using Base = detail::HashTable<detail::HashMapTraits<Key, T, Hash, KeyEqual, true, true>>;

    public:
        using key_type = Key;
        using mapped_type = T;
        using value_type = typename Base::value_type;
        using size_type = typename Base::size_type;
        using iterator = typename Base::iterator;
        using const_iterator = typename Base::const_iterator;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        using Base::Base;

        // By the key, the container's own, in place of mixin::enumerable's walk
        using Base::contains;

        ordered_map& operator=(std::initializer_list<value_type> ilist) {
            Base::operator=(ilist);
            return *this;
        }

        // The order, from the newest element back to the oldest
        reverse_iterator rbegin() noexcept {
            return reverse_iterator(this->end());
        }

        const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(this->end());
        }

        const_reverse_iterator crbegin() const noexcept {
            return rbegin();
        }

        reverse_iterator rend() noexcept {
            return reverse_iterator(this->begin());
        }

        const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(this->begin());
        }

        const_reverse_iterator crend() const noexcept {
            return rend();
        }

        // The oldest and the newest element (the map not empty)
        value_type& front() noexcept {
            return *this->begin();
        }

        const value_type& front() const noexcept {
            return *this->begin();
        }

        value_type& back() noexcept {
            return *std::prev(this->end());
        }

        const value_type& back() const noexcept {
            return *std::prev(this->end());
        }

        // The element moved to the end (the start) of the order: as if
        // inserted last (first) from now on; the iterator stays valid
        void to_back(const_iterator pos) noexcept {
            Base::_to_back(Base::_node_of(pos));
        }

        void to_front(const_iterator pos) noexcept {
            Base::_to_front(Base::_node_of(pos));
        }

        template<class M>
        pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj) {
            return _insert_or_assign(key, std::forward<M>(obj));
        }

        template<class M>
        pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj) {
            return _insert_or_assign(std::move(key), std::forward<M>(obj));
        }

        template<class M>
        iterator insert_or_assign(const_iterator, const key_type& key, M&& obj) {
            return _insert_or_assign(key, std::forward<M>(obj)).first;
        }

        template<class M>
        iterator insert_or_assign(const_iterator, key_type&& key, M&& obj) {
            return _insert_or_assign(std::move(key), std::forward<M>(obj)).first;
        }

        // The mapped value is constructed in place from the arguments, and
        // only when the key is new.
        template<class... A>
        pair<iterator, bool> try_emplace(const key_type& key, A&&... a) {
            auto [node, inserted] = Base::_try_emplace(key, std::forward<A>(a)...);
            return {Base::_make_iterator(node), inserted};
        }

        template<class... A>
        pair<iterator, bool> try_emplace(key_type&& key, A&&... a) {
            auto [node, inserted] = Base::_try_emplace(std::move(key), std::forward<A>(a)...);
            return {Base::_make_iterator(node), inserted};
        }

        template<class... A>
        iterator try_emplace(const_iterator, const key_type& key, A&&... a) {
            return try_emplace(key, std::forward<A>(a)...).first;
        }

        template<class... A>
        iterator try_emplace(const_iterator, key_type&& key, A&&... a) {
            return try_emplace(std::move(key), std::forward<A>(a)...).first;
        }

        mapped_type& at(const key_type& key) {
            return _at(key);
        }

        const mapped_type& at(const key_type& key) const {
            return _at(key);
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        mapped_type& at(const K& key) {
            return _at(key);
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        const mapped_type& at(const K& key) const {
            return _at(key);
        }

        mapped_type& operator[](const key_type& key) {
            return Base::_try_emplace(key).first->slot.value.second;
        }

        mapped_type& operator[](key_type&& key) {
            return Base::_try_emplace(std::move(key)).first->slot.value.second;
        }

        // The value under the key, moved out, and the element erased;
        // nothing when the key is absent: what Java's remove and C#'s
        // Remove(key, out value) hand back
        optional<mapped_type> take(const key_type& key) {
            return Base::_take(key);
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        optional<mapped_type> take(const K& key) {
            return Base::_take(key);
        }

    private:
        template<class K, class M>
        pair<iterator, bool> _insert_or_assign(K&& key, M&& obj) {
            auto [node, inserted] = Base::_try_emplace(std::forward<K>(key), std::forward<M>(obj));
            if (!inserted) {
                node->slot.value.second = std::forward<M>(obj);
            }
            return {Base::_make_iterator(node), inserted};
        }

        template<class K>
        mapped_type& _at(const K& key) const {
            auto node = Base::_find(key);
            if (!node) {
                throw out_of_range("sgcl::ordered_map::at");
            }
            return node->slot.value.second;
        }

        friend bool operator==(const ordered_map& lhs, const ordered_map& rhs) {
            return lhs._equal_to(rhs);
        }

        friend void swap(ordered_map& lhs, ordered_map& rhs) noexcept(noexcept(lhs.swap(rhs))) {
            lhs.swap(rhs);
        }

        template<class K, class V, class H, class E, class Pred>
        friend size_t erase_if(ordered_map<K, V, H, E>& c, Pred pred);
    };

    template<std::input_iterator InputIt,
             class Hash = std::hash<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>,
             class KeyEqual = std::equal_to<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>>
    ordered_map(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
        -> ordered_map<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
                         typename std::iterator_traits<InputIt>::value_type::second_type, Hash, KeyEqual>;

    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    ordered_map(std::initializer_list<pair<Key, T>>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
        -> ordered_map<Key, T, Hash, KeyEqual>;

    template<class Key, class T, class Hash, class KeyEqual, class Pred>
    size_t erase_if(ordered_map<Key, T, Hash, KeyEqual>& c, Pred pred) {
        return c._erase_if(pred);
    }
}

namespace std {
    using sgcl::erase_if;
}
