//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/mixin/mixin.h"
#include "detail/hamt.h"

#include <functional>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace sgcl::immutable {
    namespace detail {
        template<class Key, class Hash, class KeyEqual>
        struct SetTraits {
            using key_type = Key;
            using value_type = Key;
            using hasher = Hash;
            using key_equal = KeyEqual;

            SGCL_INLINE_HOT static const Key& key(const value_type& v) noexcept {
                return v;
            }
        };
    }

    // The immutable hash set: the hash array mapped trie of map
    // (detail/hamt.h) with the key as the element. Every
    // insert and erase returns a new set that shares all but the path it
    // changed with the old one, which stays as it was; a set held by any
    // number of threads is read by all of them without a lock, a version
    // published and replaced through a copy_on_write or an atomic. Two
    // words, the size and the root, living where a tracked_ptr may.
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class set   // read as any range; contains and find by the key, its own
    : public mixin::enumerable<set<Key, Hash, KeyEqual>>
    , public mixin::immutable<set<Key, Hash, KeyEqual>> {
        using Trie = detail::Hamt<detail::SetTraits<Key, Hash, KeyEqual>>;

        static_assert(sgcl::detail::nothrow_function_object<Hash, const Key&>, "sgcl::immutable::set: Hash must be noexcept");
        static_assert(sgcl::detail::nothrow_function_object<KeyEqual, const Key&, const Key&>, "sgcl::immutable::set: KeyEqual must be noexcept");

        // An insert copies the path to the key and makes the key; an
        // erase copies the path (detail/hamt.h)
        template<class... A>
        static constexpr bool NothrowInsert = Trie::template NothrowInsert<A...>;

        static constexpr bool NothrowErase = Trie::NothrowCopy;

        template<class K>
        static constexpr bool NothrowLookup = Trie::template NothrowLookup<K>;

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

        set() = default;

        SGCL_INLINE_HOT explicit set(const Hash& hash, const KeyEqual& equal = KeyEqual())
        : _trie(hash, equal) {
        }

        template<std::input_iterator InputIt>
        SGCL_INLINE_HOT set(InputIt first, InputIt last, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual()) noexcept(Trie::template NothrowBuild<InputIt>)
        : _trie(first, last, hash, equal) {
        }

        SGCL_INLINE_HOT set(std::initializer_list<value_type> ilist, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual()) noexcept(Trie::template NothrowBuild<const value_type*>)
        : set(ilist.begin(), ilist.end(), hash, equal) {
        }

        set(const set&) noexcept = default;
        set(set&&) noexcept = default;
        set& operator=(const set&) noexcept = default;
        set& operator=(set&&) noexcept = default;

        SGCL_INLINE_HOT const_iterator begin() const noexcept {
            return _trie.begin();
        }

        SGCL_INLINE_HOT const_iterator end() const noexcept {
            return _trie.end();
        }

        SGCL_INLINE_HOT const_iterator cbegin() const noexcept {
            return _trie.begin();
        }

        SGCL_INLINE_HOT const_iterator cend() const noexcept {
            return _trie.end();
        }

        SGCL_INLINE_HOT size_type size() const noexcept {
            return _trie.size();
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _trie.empty();
        }

        SGCL_INLINE_HOT hasher hash_function() const {
            return _trie.hash_function();
        }

        SGCL_INLINE_HOT key_equal key_eq() const {
            return _trie.key_eq();
        }

        // The element equal to the key, end() when there is none, as
        // every find of the library. A K other than the key type looks up
        // without building a key when the hash and the equality are
        // transparent.
        SGCL_INLINE_HOT const_iterator find(const Key& key) const noexcept {
            return _trie.find_at(key);
        }

        template<class K> requires sgcl::detail::TransparentLookup<Hash, KeyEqual>
        SGCL_INLINE_HOT const_iterator find(const K& key) const noexcept(NothrowLookup<K>) {
            return _trie.find_at(key);
        }

        SGCL_INLINE_HOT bool contains(const Key& key) const noexcept {
            return _trie.contains(key);
        }

        template<class K> requires sgcl::detail::TransparentLookup<Hash, KeyEqual>
        SGCL_INLINE_HOT bool contains(const K& key) const noexcept(NothrowLookup<K>) {
            return _trie.contains(key);
        }

        SGCL_INLINE_HOT size_type count(const Key& key) const noexcept {
            return _trie.contains(key) ? 1 : 0;
        }

        template<class K> requires sgcl::detail::TransparentLookup<Hash, KeyEqual>
        SGCL_INLINE_HOT size_type count(const K& key) const noexcept(NothrowLookup<K>) {
            return _trie.contains(key) ? 1 : 0;
        }

        // The set with the key: the path to it copied, the rest shared;
        // the same set when an equal one is there, as every insert keeps
        // what it finds
        SGCL_INLINE_HOT set insert(const Key& key) const noexcept(NothrowInsert<const Key&>) {
            return _trie.contains(key) ? *this : set(_trie.insert(key, key));
        }

        SGCL_INLINE_HOT set insert(Key&& key) const noexcept(NothrowInsert<Key&&>) {
            return _trie.contains(key) ? *this : set(_trie.insert(key, std::move(key)));
        }

        // The set without the key: the path copied, a node emptied
        // dropped; the same set when the key is absent
        SGCL_INLINE_HOT set erase(const Key& key) const noexcept(NothrowErase) {
            return set(_trie.erase(key));
        }

        template<class K> requires sgcl::detail::TransparentLookup<Hash, KeyEqual>
        SGCL_INLINE_HOT set erase(const K& key) const noexcept(NothrowLookup<K> && NothrowErase) {
            return set(_trie.erase(key));
        }

        // A set changed in place and frozen into a set, as map::builder
        // is for a map (map.h): thaw() costs nothing, a change copies a
        // node it shares once and changes the nodes it made in place,
        // freeze() is the set of what it holds and the builder goes on.
        // One thread's; moves, never copies.
        class builder {
        public:
            builder() = default;

            SGCL_INLINE_HOT explicit builder(const Hash& hash, const KeyEqual& equal = KeyEqual())
            : _trie(hash, equal) {
            }

            SGCL_INLINE_HOT builder(builder&& o) noexcept
            : _trie(std::move(o._trie)) {
                o._trie = Trie(_trie.hash_function(), _trie.key_eq());
            }

            SGCL_INLINE_HOT builder& operator=(builder&& o) noexcept {
                if (this != &o) {
                    _trie = std::move(o._trie);
                    o._trie = Trie(_trie.hash_function(), _trie.key_eq());
                }
                return *this;
            }

            builder(const builder&) = delete;
            builder& operator=(const builder&) = delete;

            SGCL_INLINE_HOT size_type size() const noexcept {
                return _trie.size();
            }

            SGCL_INLINE_HOT bool empty() const noexcept {
                return _trie.empty();
            }

            SGCL_INLINE_HOT bool contains(const Key& key) const noexcept {
                return _trie.contains(key);
            }

            template<class K> requires sgcl::detail::TransparentLookup<Hash, KeyEqual>
            SGCL_INLINE_HOT bool contains(const K& key) const noexcept(NothrowLookup<K>) {
                return _trie.contains(key);
            }

            // The key added when no equal one is there; true when it was added
            SGCL_INLINE_HOT bool insert(const Key& key) noexcept(NothrowInsert<const Key&>) {
                return !_trie.contains(key) && _trie.insert_in_place(key, key);
            }

            SGCL_INLINE_HOT bool insert(Key&& key) noexcept(NothrowInsert<Key&&>) {
                return !_trie.contains(key) && _trie.insert_in_place(key, std::move(key));
            }

            // The key taken out; false when it was absent
            SGCL_INLINE_HOT bool erase(const Key& key) noexcept(NothrowErase) {
                return _trie.erase_in_place(key);
            }

            template<class K> requires sgcl::detail::TransparentLookup<Hash, KeyEqual>
            SGCL_INLINE_HOT bool erase(const K& key) noexcept(NothrowLookup<K> && NothrowErase) {
                return _trie.erase_in_place(key);
            }

            // The set of what the builder holds now; the builder goes on
            SGCL_INLINE_HOT set freeze() noexcept {
                _trie.disown();
                return set(_trie);
            }

        private:
            friend class set;

            SGCL_INLINE_HOT explicit builder(const Trie& trie) noexcept
            : _trie(trie) {
            }

            Trie _trie;
        };

        // The builder over this set: nothing copied until it changes
        SGCL_INLINE_HOT builder thaw() const noexcept {
            return builder(_trie);
        }

        // The same elements: a lookup per element, by the hash and the
        // equality, which cannot throw
        SGCL_INLINE_HOT friend bool operator==(const set& a, const set& b) noexcept {
            return a._trie.equals(b._trie, [](const value_type&, const value_type&) noexcept { return true; });
        }

        SGCL_INLINE_HOT friend bool operator!=(const set& a, const set& b) noexcept {
            return !(a == b);
        }

    private:
        SGCL_INLINE_HOT explicit set(Trie trie) noexcept
        : _trie(std::move(trie)) {
        }

        Trie _trie;
    };

    template<std::input_iterator InputIt,
             class Hash = std::hash<typename std::iterator_traits<InputIt>::value_type>,
             class KeyEqual = std::equal_to<typename std::iterator_traits<InputIt>::value_type>>
    set(InputIt, InputIt, Hash = Hash(), KeyEqual = KeyEqual())
        -> set<typename std::iterator_traits<InputIt>::value_type, Hash, KeyEqual>;

    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    set(std::initializer_list<Key>, Hash = Hash(), KeyEqual = KeyEqual())
        -> set<Key, Hash, KeyEqual>;
}
