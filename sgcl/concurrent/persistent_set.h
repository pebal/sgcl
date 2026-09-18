//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/hamt.h"

#include <functional>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace sgcl {
    namespace detail {
        template<class Key, class Hash, class KeyEqual>
        struct PersistentSetTraits {
            using key_type = Key;
            using value_type = Key;
            using hasher = Hash;
            using key_equal = KeyEqual;

            static const Key& key(const value_type& v) noexcept {
                return v;
            }
        };
    }

    // The persistent hash set: the hash array mapped trie of
    // persistent_map (detail/hamt.h) with the key as the element. Every
    // insert and erase returns a new set that shares all but the path it
    // changed with the old one, which stays as it was; a set held by any
    // number of threads is read by all of them without a lock, a version
    // published and replaced through a copy_on_write or an atomic. Two
    // words, the size and the root, living where a tracked_ptr may.
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class persistent_set {
        using Trie = detail::Hamt<detail::PersistentSetTraits<Key, Hash, KeyEqual>>;

    public:
        using key_type = Key;
        using value_type = Key;
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

        persistent_set() = default;

        explicit persistent_set(const Hash& hash, const KeyEqual& equal = KeyEqual())
        : _trie(hash, equal) {
        }

        template<std::input_iterator InputIt>
        persistent_set(InputIt first, InputIt last, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual())
        : _trie(first, last, hash, equal) {
        }

        persistent_set(std::initializer_list<value_type> ilist, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual())
        : persistent_set(ilist.begin(), ilist.end(), hash, equal) {
        }

        persistent_set(const persistent_set&) noexcept = default;
        persistent_set(persistent_set&&) noexcept = default;
        persistent_set& operator=(const persistent_set&) noexcept = default;
        persistent_set& operator=(persistent_set&&) noexcept = default;

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

        // The element equal to the key, null when there is none. A K
        // other than the key type looks up without building a key when
        // the hash and the equality are transparent.
        const Key* find(const Key& key) const {
            return _trie.find(key);
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        const Key* find(const K& key) const {
            return _trie.find(key);
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

        // The set with the key: the path to it copied, the rest shared;
        // the element replaced by `key` when an equal one is there
        persistent_set insert(const Key& key) const {
            return persistent_set(_trie.insert(key, key));
        }

        persistent_set insert(Key&& key) const {
            return persistent_set(_trie.insert(key, std::move(key)));
        }

        // The set without the key: the path copied, a node emptied
        // dropped; the same set when the key is absent
        persistent_set erase(const Key& key) const {
            return persistent_set(_trie.erase(key));
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        persistent_set erase(const K& key) const {
            return persistent_set(_trie.erase(key));
        }

        // The same elements
        friend bool operator==(const persistent_set& a, const persistent_set& b) {
            return a._trie.equals(b._trie, [](const value_type&, const value_type&) { return true; });
        }

        friend bool operator!=(const persistent_set& a, const persistent_set& b) {
            return !(a == b);
        }

    private:
        explicit persistent_set(Trie trie) noexcept
        : _trie(std::move(trie)) {
        }

        Trie _trie;
    };

    template<std::input_iterator InputIt,
             class Hash = std::hash<typename std::iterator_traits<InputIt>::value_type>,
             class KeyEqual = std::equal_to<typename std::iterator_traits<InputIt>::value_type>>
    persistent_set(InputIt, InputIt, Hash = Hash(), KeyEqual = KeyEqual())
        -> persistent_set<typename std::iterator_traits<InputIt>::value_type, Hash, KeyEqual>;

    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    persistent_set(std::initializer_list<Key>, Hash = Hash(), KeyEqual = KeyEqual())
        -> persistent_set<Key, Hash, KeyEqual>;
}
