//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "detail/hamt.h"

#include <functional>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace sgcl {
    namespace detail {
        template<class Key, class T, class Hash, class KeyEqual>
        struct PersistentMapTraits {
            using key_type = Key;
            using mapped_type = T;
            using value_type = pair<const Key, T>;
            using hasher = Hash;
            using key_equal = KeyEqual;

            static const Key& key(const value_type& v) noexcept {
                return v.first;
            }
        };
    }

    // The persistent hash map of Clojure and Scala: a map every insert and
    // erase of which returns a new map and leaves the old one as it was,
    // the two sharing everything but the path that changed. A hash array
    // mapped trie (detail/hamt.h): 32-way nodes indexed by five bits of
    // the hash per level, each storing only the slots in use, so that a
    // lookup walks log32(n) nodes (four for a million elements) and an
    // insert or an erase copies those nodes, a few hundred bytes, and
    // shares the rest. Nothing is ever modified: a map held by any number
    // of threads is read by all of them without a lock, and a version is
    // published, and replaced by the next, through a copy_on_write or an
    // atomic (README: Persistent structures).
    //
    // The map is two words and its function objects: the size and a
    // tracked_ptr to the root. It lives where a tracked_ptr may, on a
    // stack or inside a managed object, and a copy of it is a copy of
    // those words. The nodes are managed objects that no version owns: a
    // node reached by ten versions is one node, collected once the last
    // of them is dropped. Elements are const through the map; a key or a
    // value holding tracked pointers is traced where it lives, in a node.
    // find hands back a pointer to the value, null when the key is
    // absent, as the Sgcl dictionaries do: the value is in a node the map
    // holds, valid while some version does.
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class persistent_map {
        using Trie = detail::Hamt<detail::PersistentMapTraits<Key, T, Hash, KeyEqual>>;

    public:
        using key_type = Key;
        using mapped_type = T;
        using value_type = typename Trie::value_type;
        using hasher = Hash;
        using key_equal = KeyEqual;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = const value_type&;
        using const_reference = const value_type&;
        using pointer = const value_type*;
        using const_pointer = const value_type*;
        using const_iterator = typename Trie::const_iterator;
        using iterator = const_iterator;

        persistent_map() = default;

        explicit persistent_map(const Hash& hash, const KeyEqual& equal = KeyEqual())
        : _trie(hash, equal) {
        }

        template<std::input_iterator InputIt>
        persistent_map(InputIt first, InputIt last, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual())
        : _trie(first, last, hash, equal) {
        }

        persistent_map(std::initializer_list<value_type> ilist, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual())
        : persistent_map(ilist.begin(), ilist.end(), hash, equal) {
        }

        persistent_map(const persistent_map&) noexcept = default;
        persistent_map(persistent_map&&) noexcept = default;
        persistent_map& operator=(const persistent_map&) noexcept = default;
        persistent_map& operator=(persistent_map&&) noexcept = default;

        const_iterator begin() const noexcept {
            return _trie.begin();
        }

        const_iterator end() const noexcept {
            return _trie.end();
        }

        const_iterator cbegin() const noexcept {
            return _trie.begin();
        }

        const_iterator cend() const noexcept {
            return _trie.end();
        }

        size_type size() const noexcept {
            return _trie.size();
        }

        bool empty() const noexcept {
            return _trie.empty();
        }

        hasher hash_function() const {
            return _trie.hash_function();
        }

        key_equal key_eq() const {
            return _trie.key_eq();
        }

        // The value under the key, null when the key is absent. A K
        // other than the key type looks up without building a key when
        // the hash and the equality are transparent (a string_view for a
        // string), as the module's other maps do.
        const T* find(const Key& key) const {
            auto v = _trie.find(key);
            return v ? &v->second : nullptr;
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        const T* find(const K& key) const {
            auto v = _trie.find(key);
            return v ? &v->second : nullptr;
        }

        bool contains(const Key& key) const {
            return _trie.contains(key);
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        bool contains(const K& key) const {
            return _trie.contains(key);
        }

        size_type count(const Key& key) const {
            return _trie.contains(key) ? 1 : 0;
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        size_type count(const K& key) const {
            return _trie.contains(key) ? 1 : 0;
        }

        // The value under the key; std::out_of_range when it is absent
        const T& at(const Key& key) const {
            auto v = _trie.find(key);
            if (!v) {
                throw std::out_of_range("sgcl::persistent_map::at");
            }
            return v->second;
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        const T& at(const K& key) const {
            auto v = _trie.find(key);
            if (!v) {
                throw std::out_of_range("sgcl::persistent_map::at");
            }
            return v->second;
        }

        // The map with `value` under `key`, added or in place of the value
        // there: the path to the element copied, log32(n) nodes, the rest
        // shared. The size grows by one when the key was absent.
        persistent_map insert(const Key& key, const T& value) const {
            return persistent_map(_trie.insert(key, key, value));
        }

        persistent_map insert(const Key& key, T&& value) const {
            return persistent_map(_trie.insert(key, key, std::move(value)));
        }

        persistent_map insert(Key&& key, const T& value) const {
            return persistent_map(_trie.insert(key, std::move(key), value));
        }

        persistent_map insert(Key&& key, T&& value) const {
            return persistent_map(_trie.insert(key, std::move(key), std::move(value)));
        }

        persistent_map insert(const value_type& value) const {
            return insert(value.first, value.second);
        }

        // The map with an element under `key` built from the arguments
        template<class... A>
        persistent_map emplace(const Key& key, A&&... a) const {
            return persistent_map(_trie.insert(key, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::forward<A>(a)...)));
        }

        // The map without the element under `key`: the path copied, a
        // node emptied dropped; the same map when the key is absent
        persistent_map erase(const Key& key) const {
            return persistent_map(_trie.erase(key));
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        persistent_map erase(const K& key) const {
            return persistent_map(_trie.erase(key));
        }

        // The same elements under the same keys
        friend bool operator==(const persistent_map& a, const persistent_map& b) {
            return a._trie.equals(b._trie, [](const value_type& x, const value_type& y) { return x.second == y.second; });
        }

        friend bool operator!=(const persistent_map& a, const persistent_map& b) {
            return !(a == b);
        }

    private:
        explicit persistent_map(Trie trie) noexcept
        : _trie(std::move(trie)) {
        }

        Trie _trie;
    };

    template<std::input_iterator InputIt,
             class Hash = std::hash<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>,
             class KeyEqual = std::equal_to<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>>
    persistent_map(InputIt, InputIt, Hash = Hash(), KeyEqual = KeyEqual())
        -> persistent_map<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
                          typename std::iterator_traits<InputIt>::value_type::second_type, Hash, KeyEqual>;

    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    persistent_map(std::initializer_list<pair<const Key, T>>, Hash = Hash(), KeyEqual = KeyEqual())
        -> persistent_map<Key, T, Hash, KeyEqual>;
}
