//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/mixin/mixin.h"
#include "detail/hamt.h"

#include <functional>
#include <initializer_list>
#include <iterator>
#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace sgcl::immutable {
    namespace detail {
        template<class Key, class T, class Hash, class KeyEqual>
        struct MapTraits {
            using key_type = Key;
            using mapped_type = T;
            using value_type = pair<const Key, T>;
            using hasher = Hash;
            using key_equal = KeyEqual;

            SGCL_INLINE_HOT static const Key& key(const value_type& v) noexcept {
                return v.first;
            }
        };
    }

    // The immutable hash map (the persistent map of Clojure and Scala):
    // a map every insert and erase of which returns a new map and leaves
    // the old one as it was,
    // the two sharing everything but the path that changed. A hash array
    // mapped trie (detail/hamt.h): 32-way nodes indexed by five bits of
    // the hash per level, each storing only the slots in use, so that a
    // lookup walks log32(n) nodes (four for a million elements) and an
    // insert or an erase copies those nodes, a few hundred bytes, and
    // shares the rest. Nothing is ever modified: a map held by any number
    // of threads is read by all of them without a lock, and a version is
    // published, and replaced by the next, through a copy_on_write or an
    // atomic; a state of a program is such a map, and the next state a
    // new one, the two compared by their roots (the description in
    // docs/sgcl/immutable/README.md).
    //
    // The map is two words and its function objects: the size and a
    // tracked_ptr to the root. It lives where a tracked_ptr may, on a
    // stack or inside a managed object, and a copy of it is a copy of
    // those words. The nodes are managed objects that no version owns: a
    // node reached by ten versions is one node, collected once the last
    // of them is dropped. Elements are const through the map; a key or a
    // value holding tracked pointers is traced where it lives, in a node.
    // find hands back an iterator, end() when the key is absent, as every
    // find of the library does; try_get (mixin::lookup) the pointer to
    // the value, null when absent: the value is in a node the map holds,
    // valid while some version does. insert keeps an element that is
    // there, as every insert does; set puts the value in its place.
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class map   // read as any range; contains and find by the key, its own; the reads of a map by its key
    : public mixin::enumerable<map<Key, T, Hash, KeyEqual>>
    , public mixin::immutable<map<Key, T, Hash, KeyEqual>>
    , public mixin::lookup<map<Key, T, Hash, KeyEqual>> {
        using Trie = detail::Hamt<detail::MapTraits<Key, T, Hash, KeyEqual>>;

        static_assert(sgcl::detail::nothrow_function_object<Hash, const Key&>, "sgcl::immutable::map: Hash must be noexcept");
        static_assert(sgcl::detail::nothrow_function_object<KeyEqual, const Key&, const Key&>, "sgcl::immutable::map: KeyEqual must be noexcept");

        // An insert or a set copies the path to the element and makes the
        // element; an erase copies the path (detail/hamt.h)
        template<class... A>
        static constexpr bool NothrowInsert = Trie::template NothrowInsert<A...>;

        static constexpr bool NothrowErase = Trie::NothrowCopy;

        template<class K>
        static constexpr bool NothrowLookup = Trie::template NothrowLookup<K>;

        // An update calls f, makes the value of what it gives and sets it
        template<class F>
        SGCL_INLINE_HOT static constexpr bool _nothrow_update() noexcept {
            using R = std::invoke_result_t<F&, const T&>;
            return std::is_nothrow_invocable_v<F&, const T&> && std::is_nothrow_constructible_v<T, R>
                && std::is_nothrow_move_constructible_v<T> && NothrowInsert<const Key&, T&&>;
        }

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

        map() = default;

        SGCL_INLINE_HOT explicit map(const Hash& hash, const KeyEqual& equal = KeyEqual())
        : _trie(hash, equal) {
        }

        template<std::input_iterator InputIt>
        SGCL_INLINE_HOT map(InputIt first, InputIt last, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual()) noexcept(Trie::template NothrowBuild<InputIt>)
        : _trie(first, last, hash, equal) {
        }

        SGCL_INLINE_HOT map(std::initializer_list<value_type> ilist, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual()) noexcept(Trie::template NothrowBuild<const value_type*>)
        : map(ilist.begin(), ilist.end(), hash, equal) {
        }

        map(const map&) noexcept = default;
        map(map&&) noexcept = default;
        map& operator=(const map&) noexcept = default;
        map& operator=(map&&) noexcept = default;

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

        // The element under the key, end() when the key is absent; ++
        // goes on in the order begin() walks. A K other than the key type
        // looks up without building a key when the hash and the equality
        // are transparent (a string_view for a string), as the module's
        // other maps do. try_get is the value's pointer, which is cheaper:
        // an iterator carries its path of nodes
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

        // The value under the key; out_of_range when it is absent
        SGCL_INLINE_HOT const T& at(const Key& key) const {
            auto v = _trie.find(key);
            if (!v) {
                throw out_of_range("sgcl::immutable::map::at");
            }
            return v->second;
        }

        template<class K> requires sgcl::detail::TransparentLookup<Hash, KeyEqual>
        SGCL_INLINE_HOT const T& at(const K& key) const {
            auto v = _trie.find(key);
            if (!v) {
                throw out_of_range("sgcl::immutable::map::at");
            }
            return v->second;
        }

        // The map with `value` under `key` when the key is absent, the
        // same map when it is there, as every insert of the library keeps
        // what it finds: the path to the element copied, log32(n) nodes,
        // the rest shared
        SGCL_INLINE_HOT map insert(const Key& key, const T& value) const noexcept(NothrowInsert<const Key&, const T&>) {
            return _trie.contains(key) ? *this : map(_trie.insert(key, key, value));
        }

        SGCL_INLINE_HOT map insert(const Key& key, T&& value) const noexcept(NothrowInsert<const Key&, T&&>) {
            return _trie.contains(key) ? *this : map(_trie.insert(key, key, std::move(value)));
        }

        SGCL_INLINE_HOT map insert(Key&& key, const T& value) const noexcept(NothrowInsert<Key&&, const T&>) {
            return _trie.contains(key) ? *this : map(_trie.insert(key, std::move(key), value));
        }

        SGCL_INLINE_HOT map insert(Key&& key, T&& value) const noexcept(NothrowInsert<Key&&, T&&>) {
            return _trie.contains(key) ? *this : map(_trie.insert(key, std::move(key), std::move(value)));
        }

        // The map with `value` under `key`, added or in place of the value
        // there (vector's set, insert_or_assign of the mutable maps). The
        // size grows by one when the key was absent.
        SGCL_INLINE_HOT map set(const Key& key, const T& value) const noexcept(NothrowInsert<const Key&, const T&>) {
            return map(_trie.insert(key, key, value));
        }

        SGCL_INLINE_HOT map set(const Key& key, T&& value) const noexcept(NothrowInsert<const Key&, T&&>) {
            return map(_trie.insert(key, key, std::move(value)));
        }

        SGCL_INLINE_HOT map set(Key&& key, const T& value) const noexcept(NothrowInsert<Key&&, const T&>) {
            return map(_trie.insert(key, std::move(key), value));
        }

        SGCL_INLINE_HOT map set(Key&& key, T&& value) const noexcept(NothrowInsert<Key&&, T&&>) {
            return map(_trie.insert(key, std::move(key), std::move(value)));
        }

        SGCL_INLINE_HOT map insert(const value_type& value) const noexcept(NothrowInsert<const Key&, const T&>) {
            return insert(value.first, value.second);
        }

        // The map with f(value) in place of the value under `key`: set of
        // what f gives of the old one, f called once; the same map when
        // the key is absent, as erase's (Clojure's update, immer's
        // update_if_exists). The value f gives is made before the path is
        // copied, so f may read this map
        template<class F>
        requires std::invocable<F&, const T&> && std::convertible_to<std::invoke_result_t<F&, const T&>, T>
        SGCL_INLINE_HOT map update(const Key& key, F f) const noexcept(_nothrow_update<F>()) {
            auto v = _trie.find(key);
            if (!v) {
                return *this;
            }
            T value(f(v->second));
            return map(_trie.insert(key, key, std::move(value)));
        }

        // The same with a value for an absent key: f(fallback) is put under
        // it, so a count is m.update(word, 0, [](int n) { return n + 1; })
        // (Clojure's update with fnil)
        template<class F>
        requires std::invocable<F&, const T&> && std::convertible_to<std::invoke_result_t<F&, const T&>, T>
        SGCL_INLINE_HOT map update(const Key& key, const T& fallback, F f) const noexcept(_nothrow_update<F>()) {
            auto v = _trie.find(key);
            T value(f(v ? v->second : fallback));
            return map(_trie.insert(key, key, std::move(value)));
        }

        // The map with an element under `key` built from the arguments,
        // when the key is absent (as insert)
        template<class... A>
        SGCL_INLINE_HOT map emplace(const Key& key, A&&... a) const noexcept(NothrowErase && std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_constructible_v<T, A...>) {
            if (_trie.contains(key)) {
                return *this;
            }
            return map(_trie.insert(key, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::forward<A>(a)...)));
        }

        // The map without the element under `key`: the path copied, a
        // node emptied dropped; the same map when the key is absent
        SGCL_INLINE_HOT map erase(const Key& key) const noexcept(NothrowErase) {
            return map(_trie.erase(key));
        }

        template<class K> requires sgcl::detail::TransparentLookup<Hash, KeyEqual>
        SGCL_INLINE_HOT map erase(const K& key) const noexcept(NothrowLookup<K> && NothrowErase) {
            return map(_trie.erase(key));
        }

        // A map changed in place, one element at a time, and frozen into a
        // map when it is done (Clojure's transient, immer's): thaw() is
        // the builder over this map's trie, which it shares, and costs
        // nothing; an insert or an erase changes in place the nodes the
        // builder made, and a node it shares with a map it copies once,
        // the first time a change goes through it, where a map's insert
        // copies the whole path every time and leaves the old one to the
        // collector. freeze() is the map of what the builder holds, the
        // builder's marks on its nodes cleared, so that the map is a
        // value like any other; the builder goes on, and its next change
        // copies again what the map now shares. A builder is one thread's
        // and moves but does not copy: two would change one node. It
        // lives where a tracked_ptr may.
        //
        // An element added or taken out moves the elements after it in
        // its node, which needs a move of value_type that cannot throw;
        // without one (a key whose copy can, such as a std::string, in a
        // pair whose key is const) the builder copies that one node
        // instead, and still not the path above it.
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

            // The value under the key, null when it is absent: valid until
            // the builder's next change (try_get, as a map's; a builder has
            // no iterators, its nodes changing under them)
            SGCL_INLINE_HOT const T* try_get(const Key& key) const noexcept {
                auto v = _trie.find(key);
                return v ? &v->second : nullptr;
            }

            template<class K> requires sgcl::detail::TransparentLookup<Hash, KeyEqual>
            SGCL_INLINE_HOT const T* try_get(const K& key) const noexcept(NothrowLookup<K>) {
                auto v = _trie.find(key);
                return v ? &v->second : nullptr;
            }

            SGCL_INLINE_HOT bool contains(const Key& key) const noexcept {
                return _trie.contains(key);
            }

            template<class K> requires sgcl::detail::TransparentLookup<Hash, KeyEqual>
            SGCL_INLINE_HOT bool contains(const K& key) const noexcept(NothrowLookup<K>) {
                return _trie.contains(key);
            }

            // `value` under `key` when the key is absent, as a map's
            // insert; true when it was added
            SGCL_INLINE_HOT bool insert(const Key& key, const T& value) noexcept(NothrowInsert<const Key&, const T&>) {
                return !_trie.contains(key) && _trie.insert_in_place(key, key, value);
            }

            SGCL_INLINE_HOT bool insert(const Key& key, T&& value) noexcept(NothrowInsert<const Key&, T&&>) {
                return !_trie.contains(key) && _trie.insert_in_place(key, key, std::move(value));
            }

            SGCL_INLINE_HOT bool insert(Key&& key, const T& value) noexcept(NothrowInsert<Key&&, const T&>) {
                return !_trie.contains(key) && _trie.insert_in_place(key, std::move(key), value);
            }

            SGCL_INLINE_HOT bool insert(Key&& key, T&& value) noexcept(NothrowInsert<Key&&, T&&>) {
                return !_trie.contains(key) && _trie.insert_in_place(key, std::move(key), std::move(value));
            }

            // `value` under `key`, added or in place of the value there;
            // true when the key was absent
            SGCL_INLINE_HOT bool set(const Key& key, const T& value) noexcept(NothrowInsert<const Key&, const T&>) {
                return _trie.insert_in_place(key, key, value);
            }

            SGCL_INLINE_HOT bool set(const Key& key, T&& value) noexcept(NothrowInsert<const Key&, T&&>) {
                return _trie.insert_in_place(key, key, std::move(value));
            }

            SGCL_INLINE_HOT bool set(Key&& key, const T& value) noexcept(NothrowInsert<Key&&, const T&>) {
                return _trie.insert_in_place(key, std::move(key), value);
            }

            SGCL_INLINE_HOT bool set(Key&& key, T&& value) noexcept(NothrowInsert<Key&&, T&&>) {
                return _trie.insert_in_place(key, std::move(key), std::move(value));
            }

            SGCL_INLINE_HOT bool insert(const value_type& value) noexcept(NothrowInsert<const Key&, const T&>) {
                return insert(value.first, value.second);
            }

            // An element under `key` built from the arguments, when the key
            // is absent
            template<class... A>
            SGCL_INLINE_HOT bool emplace(const Key& key, A&&... a) noexcept(NothrowErase && std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_constructible_v<T, A...>) {
                return !_trie.contains(key) && _trie.insert_in_place(key, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::forward<A>(a)...));
            }

            // The element under `key` taken out; false when it was absent
            SGCL_INLINE_HOT bool erase(const Key& key) noexcept(NothrowErase) {
                return _trie.erase_in_place(key);
            }

            template<class K> requires sgcl::detail::TransparentLookup<Hash, KeyEqual>
            SGCL_INLINE_HOT bool erase(const K& key) noexcept(NothrowLookup<K> && NothrowErase) {
                return _trie.erase_in_place(key);
            }

            // The map of what the builder holds now: the builder's marks
            // cleared on the nodes it made (a walk of those alone), the
            // trie shared by the two; the builder goes on
            SGCL_INLINE_HOT map freeze() noexcept {
                _trie.disown();
                return map(_trie);
            }

        private:
            friend class map;

            SGCL_INLINE_HOT explicit builder(const Trie& trie) noexcept
            : _trie(trie) {
            }

            Trie _trie;
        };

        // The builder over this map: nothing copied until it changes
        SGCL_INLINE_HOT builder thaw() const noexcept {
            return builder(_trie);
        }

        // The same elements under the same keys
        SGCL_INLINE_HOT friend bool operator==(const map& a, const map& b) requires req::equatable<T> {
            return a._trie.equals(b._trie, [](const value_type& x, const value_type& y) { return x.second == y.second; });
        }

        SGCL_INLINE_HOT friend bool operator!=(const map& a, const map& b) requires req::equatable<T> {
            return !(a == b);
        }

    private:
        friend class mixin::lookup<map>;   // _value_of: the pointer read of try_get, get and value_or

        SGCL_INLINE_HOT explicit map(Trie trie) noexcept
        : _trie(std::move(trie)) {
        }

        // The value under the key as a pointer, null when absent: what
        // mixin::lookup reads by, cheaper than find's iterator
        template<class K>
        SGCL_INLINE_HOT const T* _value_of(const K& key) const noexcept(NothrowLookup<K>) {
            auto v = _trie.find(key);
            return v ? &v->second : nullptr;
        }

        Trie _trie;
    };

    template<std::input_iterator InputIt,
             class Hash = std::hash<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>,
             class KeyEqual = std::equal_to<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>>
    map(InputIt, InputIt, Hash = Hash(), KeyEqual = KeyEqual())
        -> map<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
                          typename std::iterator_traits<InputIt>::value_type::second_type, Hash, KeyEqual>;

    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    map(std::initializer_list<pair<const Key, T>>, Hash = Hash(), KeyEqual = KeyEqual())
        -> map<Key, T, Hash, KeyEqual>;
}
