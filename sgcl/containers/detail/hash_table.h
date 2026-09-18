//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/detail/maker.h"
#include "../../core/detail/slot.h"
#include "../../core/tracked_ptr.h"
#include "../../core/unique_ptr.h"
#include "anchor.h"
#include "transparent.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace sgcl::detail {
    // The link part of a node of a hash container. The nodes of one table
    // form a single chain; a bucket points to the node that precedes the
    // bucket's first node (libstdc++ layout), so the chain starts at a
    // sentinel node and an iterator is one node pointer. The sentinel is a
    // bare HashNodeBase: it carries no element, so no destructor could run
    // on one that was never constructed. The hash is cached: rehashing
    // never hashes a key again and a lookup compares hashes before keys.
    struct HashNodeBase {
        tracked_ptr<HashNodeBase> next;
        size_t hash;
    };

    // One element of a hash container. Every node reached from a link
    // other than the sentinel is one of these, so element access is a
    // static downcast. The element is constructed as soon as the node is
    // made and destroyed by the container at erase time.
    template<class V>
    struct HashNode : HashNodeBase {
        using value_type = V;

        Slot<V> slot;
    };

    // The node of an ordered hash container (ordered_map, ordered_set):
    // besides its place in the chain, every node is on one list in the
    // order the elements were inserted, closed through the sentinel
    // (before the first, after the last, itself when there is none).
    // Iteration follows this list, the chain serves the lookups only, and
    // rehashing touches the chain alone. Two more words per node.
    struct OrderedHashNodeBase : HashNodeBase {
        tracked_ptr<OrderedHashNodeBase> before;
        tracked_ptr<OrderedHashNodeBase> after;
    };

    template<class V>
    struct OrderedHashNode : OrderedHashNodeBase {
        using value_type = V;

        Slot<V> slot;
    };

    // One raw node pointer, trivially copyable: the container roots every
    // linked node, and a raw pointer in a frame is seen by the conservative
    // stack scan. An iterator to an erased element is invalid as in std.
    // Over an ordered table it walks the insertion order, both ways, and
    // its end is the sentinel; over the others the chain, and its end is
    // null.
    template<class Node, class V, bool Ordered = false>
    class HashIterator {
    public:
        using iterator_category = std::conditional_t<Ordered, std::bidirectional_iterator_tag, std::forward_iterator_tag>;
        using value_type = std::remove_const_t<V>;
        using difference_type = ptrdiff_t;
        using pointer = V*;
        using reference = V&;

        HashIterator() noexcept = default;

        reference operator*() const noexcept {
            return static_cast<Node*>(_node)->slot.value;
        }

        pointer operator->() const noexcept {
            return std::addressof(static_cast<Node*>(_node)->slot.value);
        }

        HashIterator& operator++() noexcept {
            if constexpr(Ordered) {
                _node = static_cast<OrderedHashNodeBase*>(_node)->after.get();
            } else {
                _node = _node->next.get();
            }
            return *this;
        }

        HashIterator operator++(int) noexcept {
            HashIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        HashIterator& operator--() noexcept requires Ordered {
            _node = static_cast<OrderedHashNodeBase*>(_node)->before.get();
            return *this;
        }

        HashIterator operator--(int) noexcept requires Ordered {
            HashIterator tmp = *this;
            --(*this);
            return tmp;
        }

        operator HashIterator<Node, const value_type, Ordered>() const noexcept requires (!std::is_const_v<V>) {
            return HashIterator<Node, const value_type, Ordered>(_node);
        }

    private:
        HashNodeBase* _node = nullptr;

        explicit HashIterator(HashNodeBase* node) noexcept
        : _node(node) {
        }

        friend bool operator==(const HashIterator& lhs, const HashIterator& rhs) noexcept {
            return lhs._node == rhs._node;
        }

        template<class, class, bool> friend class HashIterator;
        template<class> friend class HashTable;
    };

    // Stops at the end of its bucket: the following node belongs to another.
    template<class Node, class V>
    class HashLocalIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::remove_const_t<V>;
        using difference_type = ptrdiff_t;
        using pointer = V*;
        using reference = V&;

        HashLocalIterator() noexcept = default;

        reference operator*() const noexcept {
            return static_cast<Node*>(_node)->slot.value;
        }

        pointer operator->() const noexcept {
            return std::addressof(static_cast<Node*>(_node)->slot.value);
        }

        HashLocalIterator& operator++() noexcept {
            auto next = _node->next.get();
            if (next && (next->hash & _mask) != _bucket) {
                next = nullptr;
            }
            _node = next;
            return *this;
        }

        HashLocalIterator operator++(int) noexcept {
            HashLocalIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        operator HashLocalIterator<Node, const value_type>() const noexcept requires (!std::is_const_v<V>) {
            return HashLocalIterator<Node, const value_type>(_node, _bucket, _mask);
        }

    private:
        HashNodeBase* _node = nullptr;
        size_t _bucket = 0;
        size_t _mask = 0;

        HashLocalIterator(HashNodeBase* node, size_t bucket, size_t mask) noexcept
        : _node(node)
        , _bucket(bucket)
        , _mask(mask) {
        }

        friend bool operator==(const HashLocalIterator& lhs, const HashLocalIterator& rhs) noexcept {
            return lhs._node == rhs._node;
        }

        template<class, class> friend class HashLocalIterator;
        template<class> friend class HashTable;
    };

    // Owns one unlinked node. The element dies with the handle if it is not
    // inserted somewhere; the node itself is reclaimed by the collector.
    // Mapped is void for the sets.
    template<class Node, class Key, class Mapped>
    class NodeHandle {
        static constexpr bool IsMap = !std::is_void_v<Mapped>;

    public:
        using key_type = Key;
        using mapped_type = Mapped;
        using value_type = typename Node::value_type;

        NodeHandle() noexcept = default;

        NodeHandle(NodeHandle&& other) noexcept
        : _node(other._node) {
            other._node = nullptr;
        }

        NodeHandle& operator=(NodeHandle&& other) {
            if (this != &other) {
                _release();
                _node = other._node;
                other._node = nullptr;
            }
            return *this;
        }

        ~NodeHandle() {
            _release();
        }

        bool empty() const noexcept {
            return !_node;
        }

        explicit operator bool() const noexcept {
            return _node != nullptr;
        }

        key_type& key() const requires IsMap {
            return const_cast<key_type&>(_node->slot.value.first);
        }

        template<class M = Mapped> requires (!std::is_void_v<M>)
        M& mapped() const {
            return _node->slot.value.second;
        }

        value_type& value() const requires (!IsMap) {
            return _node->slot.value;
        }

        void swap(NodeHandle& other) noexcept {
            _node.swap(other._node);
        }

        friend void swap(NodeHandle& lhs, NodeHandle& rhs) noexcept {
            lhs.swap(rhs);
        }

    private:
        tracked_ptr<Node> _node;

        // Roots the node from the moment it is handed over: it is unlinked
        // from the table only after this.
        explicit NodeHandle(Node* node) noexcept
        : _node(node) {
        }

        // The element destroyed with the handle. A handle dying in a sweep,
        // inside a managed object nobody refers to any more, holds a node
        // that is garbage of the same sweep: that node destroys its
        // element when the sweep reaches it, so it is left alone here
        // (if_alive: the rule of a destructor's tracked_ptr members).
        void _release() {
            if (auto node = _node.if_alive()) {
                node->slot.destroy();
            }
            _node = nullptr;
        }

        template<class> friend class HashTable;
    };

    template<class Key, class T, class Hash, class Equal, bool Unique, bool Ordered = false>
    struct HashMapTraits {
        using key_type = Key;
        using mapped_type = T;
        using value_type = std::pair<const Key, T>;
        using hasher = Hash;
        using key_equal = Equal;
        static constexpr bool unique = Unique;
        static constexpr bool ordered = Ordered;   // iteration in insertion order (ordered_map)
        static constexpr bool const_iterators = false;

        static const Key& key(const value_type& v) noexcept {
            return v.first;
        }
    };

    template<class Key, class Hash, class Equal, bool Unique, bool Ordered = false>
    struct HashSetTraits {
        using key_type = Key;
        using mapped_type = void;
        using value_type = Key;
        using hasher = Hash;
        using key_equal = Equal;
        static constexpr bool unique = Unique;
        static constexpr bool ordered = Ordered;   // iteration in insertion order (ordered_set)
        static constexpr bool const_iterators = true;   // a key is never modified in place, as in std

        static const Key& key(const value_type& v) noexcept {
            return v;
        }
    };

    // Separate chaining over one chain of nodes (HashNode) behind a
    // sentinel (HashNodeBase). The bucket array is a managed array of node
    // pointers reached through one tracked_ptr; its length is a power of
    // two and the mask is a plain member, so a lookup never reads the
    // array's header. An empty table may have no array at all. Elements
    // are destroyed at erase time (Slot); an erased node loses its link so
    // that a stale iterator keeps no more than the node itself. Iterators
    // hold nothing but a raw node pointer and survive erasing of other
    // elements, rehashing, swapping and moving of the table. The paths
    // that only read the table (find, count, begin, ++, the lookup half of
    // an insert) construct no tracked_ptr: a linked node is rooted by the
    // table. A node that is unlinked and then still touched is held by a
    // tracked_ptr on the stack for the duration, because a raw pointer may
    // live only in a register, which the collector does not see (_erase,
    // extract, merge, clear).
    // An ordered table (Traits::ordered: ordered_map, ordered_set) keeps
    // the same chain and buckets, and threads every node on a second list
    // in insertion order through the sentinel (OrderedHashNodeBase): begin,
    // ++, --, end walk that list, an insert appends to it, an erase unlinks
    // from it, a copy reproduces it; the lookups, the rehash and the
    // bucket interface read the chain and never see it.
    template<class Traits>
    class HashTable {
        static constexpr bool ordered = Traits::ordered;

        using Node = std::conditional_t<ordered, OrderedHashNode<typename Traits::value_type>, HashNode<typename Traits::value_type>>;
        using Sentinel = std::conditional_t<ordered, OrderedHashNodeBase, HashNodeBase>;
        using NodePtr = tracked_ptr<Node>;          // an element node held by its maker or handle
        using LinkPtr = tracked_ptr<HashNodeBase>;  // a link, a bucket entry, the sentinel

    public:
        using key_type = typename Traits::key_type;
        using value_type = typename Traits::value_type;
        using hasher = typename Traits::hasher;
        using key_equal = typename Traits::key_equal;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using const_iterator = HashIterator<Node, const value_type, ordered>;
        using iterator = std::conditional_t<Traits::const_iterators, const_iterator, HashIterator<Node, value_type, ordered>>;
        using const_local_iterator = HashLocalIterator<Node, const value_type>;
        using local_iterator = std::conditional_t<Traits::const_iterators, const_local_iterator, HashLocalIterator<Node, value_type>>;
        using node_type = NodeHandle<Node, key_type, typename Traits::mapped_type>;

        static constexpr bool unique = Traits::unique;

        struct insert_return_type {
            iterator position;
            bool inserted;
            node_type node;
        };

        HashTable() {
        }

        explicit HashTable(size_type bucket_count, const hasher& hash = hasher(), const key_equal& equal = key_equal())
        : _hash(hash)
        , _equal(equal) {
            if (bucket_count) {
                _rehash(std::bit_ceil(bucket_count));
            }
        }

        template<std::input_iterator InputIt>
        HashTable(InputIt first, InputIt last, size_type bucket_count = 0, const hasher& hash = hasher(), const key_equal& equal = key_equal())
        : HashTable(bucket_count, hash, equal) {
            try {
                if constexpr(std::forward_iterator<InputIt>) {
                    auto needed = _buckets_for(std::distance(first, last));
                    if (needed > _bucket_count) {
                        rehash(needed);
                    }
                }
                insert(first, last);
            }
            catch (...) {
                clear();
                throw;
            }
        }

        HashTable(std::initializer_list<value_type> ilist, size_type bucket_count = 0, const hasher& hash = hasher(), const key_equal& equal = key_equal())
        : HashTable(ilist.begin(), ilist.end(), bucket_count, hash, equal) {
        }

        HashTable(const HashTable& other)
        : _max_load_factor(other._max_load_factor)
        , _hash(other._hash)
        , _equal(other._equal) {
            try {
                _copy_nodes(other);
            }
            catch (...) {
                clear();
                throw;
            }
        }

        HashTable(HashTable&& other)
        : _buckets(other._buckets)
        , _before_begin(other._before_begin)
        , _bucket_count(other._bucket_count)
        , _mask(other._mask)
        , _size(other._size)
        , _next_resize(other._next_resize)
        , _max_load_factor(other._max_load_factor)
        , _hash(std::move(other._hash))
        , _equal(std::move(other._equal)) {
            other._reset();
        }

        ~HashTable() {
            if (!sweeping) {
                clear();
            }
        }

        HashTable& operator=(const HashTable& other) {
            if (this != &other) {
                HashTable tmp(other);
                swap(tmp);
            }
            return *this;
        }

        // The hasher and the equality are moved first: one of them throwing
        // then leaves both tables as they were, not two over one chain
        HashTable& operator=(HashTable&& other) {
            if (this != &other) {
                hasher hash(std::move(other._hash));
                key_equal equal(std::move(other._equal));
                clear();
                _buckets = other._buckets;
                _before_begin = other._before_begin;
                _bucket_count = other._bucket_count;
                _mask = other._mask;
                _size = other._size;
                _next_resize = other._next_resize;
                _max_load_factor = other._max_load_factor;
                _hash = std::move(hash);
                _equal = std::move(equal);
                other._reset();
            }
            return *this;
        }

        HashTable& operator=(std::initializer_list<value_type> ilist) {
            HashTable tmp(ilist, 0, _hash, _equal);
            tmp._max_load_factor = _max_load_factor;
            swap(tmp);
            return *this;
        }

        // iterators

        iterator begin() noexcept {
            return iterator(_first());
        }

        const_iterator begin() const noexcept {
            return const_iterator(_first());
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        iterator end() noexcept {
            return iterator(_end_node());
        }

        const_iterator end() const noexcept {
            return const_iterator(_end_node());
        }

        const_iterator cend() const noexcept {
            return end();
        }

        // capacity

        bool empty() const noexcept {
            return _size == 0;
        }

        size_type size() const noexcept {
            return _size;
        }

        size_type max_size() const noexcept {
            return std::numeric_limits<difference_type>::max();
        }

        // modifiers

        // Keeps the buckets, the hasher, the comparator and max_load_factor.
        void clear() noexcept {
            auto sentinel = _before_begin.get();
            if (!sentinel) {
                return;
            }
            LinkPtr p = sentinel->next;
            sentinel->next = nullptr;
            auto buckets = _buckets.get();
            for (size_type i = 0; i < _bucket_count; ++i) {
                buckets[i] = nullptr;
            }
            LinkPtr next;   // one stack root for the loop, assigned per node
            while (p) {
                auto node = p.get();
                next = node->next;
                node->next = nullptr;
                if constexpr(ordered) {
                    _order(node)->before = nullptr;
                    _order(node)->after = nullptr;
                }
                _node(node)->slot.destroy();   // the last store to the node: Destroyed replaces the state the barrier set, and the sweep may free it from here on
                p = next;
            }
            if constexpr(ordered) {
                sentinel->before.reset(sentinel);
                sentinel->after.reset(sentinel);
            }
            _size = 0;
        }

        auto insert(const value_type& value) {
            return _insert(value);
        }

        auto insert(value_type&& value) {
            return _insert(std::move(value));
        }

        template<class P> requires std::is_constructible_v<value_type, P&&>
        auto insert(P&& value) {
            return emplace(std::forward<P>(value));
        }

        iterator insert(const_iterator, const value_type& value) {
            return _position(_insert(value));
        }

        iterator insert(const_iterator, value_type&& value) {
            return _position(_insert(std::move(value)));
        }

        template<class P> requires std::is_constructible_v<value_type, P&&>
        iterator insert(const_iterator, P&& value) {
            return _position(emplace(std::forward<P>(value)));
        }

        template<std::input_iterator InputIt>
        void insert(InputIt first, InputIt last) {
            for (; first != last; ++first) {
                _insert(*first);
            }
        }

        void insert(std::initializer_list<value_type> ilist) {
            insert(ilist.begin(), ilist.end());
        }

        auto insert(node_type&& nh) {
            if constexpr(unique) {
                if (nh.empty()) {
                    return insert_return_type{end(), false, node_type()};
                }
                auto node = nh._node.get();
                auto hash = _hash(_key(node));
                if (auto prev = _find_before(hash, _key(node))) {
                    return insert_return_type{iterator(prev->next.get()), false, std::move(nh)};
                }
                _link_new(hash, nh._node, nullptr);
                nh._node = nullptr;
                return insert_return_type{iterator(node), true, node_type()};
            } else {
                if (nh.empty()) {
                    return end();
                }
                auto node = nh._node.get();
                auto hash = _hash(_key(node));
                _link_new(hash, nh._node, _find_before(hash, _key(node)));
                nh._node = nullptr;
                return iterator(node);
            }
        }

        iterator insert(const_iterator, node_type&& nh) {
            return _position(insert(std::move(nh)));
        }

        template<class... A>
        auto emplace(A&&... a) {
            if constexpr(sizeof...(A) == 1 && (std::is_same_v<std::remove_cvref_t<A>, value_type> && ...)) {
                return _insert(std::forward<A>(a)...);
            } else {
                auto node = _make_node(std::forward<A>(a)...);
                return _insert_node(node);
            }
        }

        template<class... A>
        iterator emplace_hint(const_iterator, A&&... a) {
            return _position(emplace(std::forward<A>(a)...));
        }

        iterator erase(const_iterator pos) {
            return iterator(_wrap(_erase(pos._node)));
        }

        iterator erase(iterator pos) requires (!std::is_same_v<iterator, const_iterator>) {
            return erase(const_iterator(pos));
        }

        // Every node is linked until its own _erase roots it; `stop` stays
        // linked throughout.
        iterator erase(const_iterator first, const_iterator last) {
            auto node = first._node;
            auto stop = last._node;
            while (node != stop) {
                node = _erase(node);
            }
            return iterator(node);
        }

        size_type erase(const key_type& key) {
            return _erase_key(key);
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
            && (!std::is_convertible_v<K&&, iterator>) && (!std::is_convertible_v<K&&, const_iterator>)
        size_type erase(K&& key) {
            return _erase_key(key);
        }

        void swap(HashTable& other) noexcept(std::is_nothrow_swappable_v<hasher> && std::is_nothrow_swappable_v<key_equal>) {
            _buckets.swap(other._buckets);
            _before_begin.swap(other._before_begin);
            std::swap(_bucket_count, other._bucket_count);
            std::swap(_mask, other._mask);
            std::swap(_size, other._size);
            std::swap(_next_resize, other._next_resize);
            std::swap(_max_load_factor, other._max_load_factor);
            using std::swap;
            swap(_hash, other._hash);
            swap(_equal, other._equal);
        }

        // The handle roots the node before it is unlinked.
        node_type extract(const_iterator pos) {
            node_type nh(_node(pos._node));
            if (nh._node) {
                _unlink(nh._node.get());
            }
            return nh;
        }

        node_type extract(const key_type& key) {
            return _extract_key(key);
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
            && (!std::is_convertible_v<K&&, iterator>) && (!std::is_convertible_v<K&&, const_iterator>)
        node_type extract(K&& key) {
            return _extract_key(key);
        }

        // Relinks the nodes: no element is copied or destroyed. A node whose
        // key this table (if unique) already holds stays in the source. The
        // source's nodes must be this table's kind: an ordered table's carry
        // the order's two words, and a relink across the kinds would read
        // an element where the other kind keeps its links.
        template<class T> requires std::is_same_v<typename HashTable<T>::value_type, value_type> && (T::ordered == ordered)
        void merge(HashTable<T>& source) {
            if ((void*)&source == (void*)this) {
                return;
            }
            NodePtr p(_node(source._chain_first()));
            while (p) {
                NodePtr next(_node(p->next.get()));
                auto node = p.get();
                auto hash = _hash(_key(node));
                auto prev = _find_before(hash, _key(node));
                if (!unique || !prev) {
                    if (_size >= _next_resize) {
                        _grow();
                        prev = _find_before(hash, _key(node));
                    }
                    source._unlink(node);
                    _link_new(hash, p, prev);
                }
                p = next;
            }
        }

        template<class T> requires std::is_same_v<typename HashTable<T>::value_type, value_type> && (T::ordered == ordered)
        void merge(HashTable<T>&& source) {
            merge(source);
        }

        // lookup

        size_type count(const key_type& key) const {
            return _count(key);
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        size_type count(const K& key) const {
            return _count(key);
        }

        iterator find(const key_type& key) {
            return iterator(_wrap(_find(key)));
        }

        const_iterator find(const key_type& key) const {
            return const_iterator(_wrap(_find(key)));
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        iterator find(const K& key) {
            return iterator(_wrap(_find(key)));
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        const_iterator find(const K& key) const {
            return const_iterator(_wrap(_find(key)));
        }

        bool contains(const key_type& key) const {
            return _find(key) != nullptr;
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        bool contains(const K& key) const {
            return _find(key) != nullptr;
        }

        std::pair<iterator, iterator> equal_range(const key_type& key) {
            auto [first, last] = _equal_range(key);
            return {iterator(_wrap(first)), iterator(_wrap(last))};
        }

        std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
            auto [first, last] = _equal_range(key);
            return {const_iterator(_wrap(first)), const_iterator(_wrap(last))};
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        std::pair<iterator, iterator> equal_range(const K& key) {
            auto [first, last] = _equal_range(key);
            return {iterator(_wrap(first)), iterator(_wrap(last))};
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
            auto [first, last] = _equal_range(key);
            return {const_iterator(_wrap(first)), const_iterator(_wrap(last))};
        }

        // bucket interface

        size_type bucket_count() const noexcept {
            return _bucket_count;
        }

        size_type max_bucket_count() const noexcept {
            return max_size();
        }

        size_type bucket_size(size_type n) const {
            size_type count = 0;
            for (auto p = _bucket_first(n); p && (p->hash & _mask) == n; p = p->next.get()) {
                ++count;
            }
            return count;
        }

        size_type bucket(const key_type& key) const {
            return _hash(key) & _mask;
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        size_type bucket(const K& key) const {
            return _hash(key) & _mask;
        }

        local_iterator begin(size_type n) {
            return local_iterator(_bucket_first(n), n, _mask);
        }

        const_local_iterator begin(size_type n) const {
            return const_local_iterator(_bucket_first(n), n, _mask);
        }

        const_local_iterator cbegin(size_type n) const {
            return begin(n);
        }

        local_iterator end(size_type n) {
            return local_iterator(nullptr, n, _mask);
        }

        const_local_iterator end(size_type n) const {
            return const_local_iterator(nullptr, n, _mask);
        }

        const_local_iterator cend(size_type n) const {
            return end(n);
        }

        // hash policy

        float load_factor() const noexcept {
            return _bucket_count ? (float)_size / (float)_bucket_count : 0.0f;
        }

        float max_load_factor() const noexcept {
            return _max_load_factor;
        }

        // Values that are not positive (or not numbers) are ignored. The
        // table grows on the next insert if it is now overloaded.
        void max_load_factor(float z) {
            if (z > 0.0f) {
                _max_load_factor = z;
                _next_resize = _threshold(_bucket_count);
            }
        }

        // Rounds up to a power of two; never below what the elements need.
        void rehash(size_type count) {
            auto needed = std::max(count, _buckets_for(_size));
            if (needed) {
                needed = std::bit_ceil(needed);
                if (needed != _bucket_count) {
                    _rehash(needed);
                }
            }
        }

        void reserve(size_type count) {
            rehash(_buckets_for(count));
        }

        // observers

        hasher hash_function() const {
            return _hash;
        }

        key_equal key_eq() const {
            return _equal;
        }

    protected:
        using mapped_type = typename Traits::mapped_type;   // void for a set

        // The element node behind a link: every linked node but the
        // sentinel is one, and the sentinel is never reached this way.
        static Node* _node(HashNodeBase* p) noexcept {
            return static_cast<Node*>(p);
        }

        static const Node* _node(const HashNodeBase* p) noexcept {
            return static_cast<const Node*>(p);
        }

        static const key_type& _key(const HashNodeBase* node) noexcept {
            return Traits::key(_node(node)->slot.value);
        }

        static iterator _make_iterator(HashNodeBase* node) noexcept {
            return iterator(node);
        }

        static HashNodeBase* _node_of(const_iterator it) noexcept {
            return it._node;
        }

        // The first node of the chain every element is on (the buckets
        // point before their first node, the sentinel before the first of
        // all: libstdc++'s layout)
        HashNodeBase* _chain_first() const noexcept {
            auto sentinel = _before_begin.get();
            return sentinel ? sentinel->next.get() : nullptr;
        }

        // The first node of the iteration: of the insertion order in an
        // ordered table (the sentinel, which is the end, when there is
        // none), of the chain otherwise
        HashNodeBase* _first() const noexcept {
            if constexpr(ordered) {
                auto sentinel = _before_begin.get();
                return sentinel ? sentinel->after.get() : nullptr;
            } else {
                return _chain_first();
            }
        }

        // The node an end iterator holds: the sentinel of an ordered table
        // (so that --end() is the last element), null otherwise; a table
        // that has no sentinel yet is empty, and its begin is null too
        HashNodeBase* _end_node() const noexcept {
            if constexpr(ordered) {
                return _before_begin.get();
            } else {
                return nullptr;
            }
        }

        // A lookup's null as the end iterator's node
        HashNodeBase* _wrap(HashNodeBase* p) const noexcept {
            return p ? p : _end_node();
        }

        // The node after `node` in the iteration
        static HashNodeBase* _next(HashNodeBase* node) noexcept {
            if constexpr(ordered) {
                return _order(node)->after.get();
            } else {
                return node->next.get();
            }
        }

        static OrderedHashNodeBase* _order(HashNodeBase* node) noexcept {
            return static_cast<OrderedHashNodeBase*>(node);
        }

        // Moves a linked node to the back (the front) of the insertion
        // order: the element counts as inserted last (first) from now on.
        // One that is there already is left alone: a cache touching its
        // newest element again (most hits, in one with an eviction order)
        // pays a load, not the eight stores of a relink
        void _to_back(HashNodeBase* node) requires ordered {
            if (_order(node)->after.get() == _before_begin.get()) {
                return;
            }
            _order_unlink(node);
            _order_link(node, _before_begin.get());
        }

        void _to_front(HashNodeBase* node) requires ordered {
            if (_order(node)->before.get() == _before_begin.get()) {
                return;
            }
            _order_unlink(node);
            _order_link(node, _before_begin->after.get());
        }

        // The lookups: the node with the key (the first with it, in a multi
        // table), the run of the nodes with it, their count; nothing
        // constructed, the walk reads the links
        template<class K>
        Node* _find(const K& key) const {
            if (!_bucket_count) {
                return nullptr;
            }
            auto prev = _find_before(_hash(key), key);
            return prev ? _node(prev->next.get()) : nullptr;
        }

        // Emplaces with the key looked up first: the element is constructed
        // only when it is inserted (try_emplace, operator[]; the weak
        // containers, whose key is looked up as the object's pointer and
        // stored as a weak one). The key of a map's element from `key`,
        // the mapped value from `a...`; a set's element from `key` alone.
        template<class K, class... A>
        std::pair<Node*, bool> _try_emplace(K&& key, A&&... a) {
            auto hash = _hash(key);
            if (auto prev = _find_before(hash, key)) {
                return {_node(prev->next.get()), false};
            }
            return {_emplace_absent(hash, std::forward<K>(key), std::forward<A>(a)...), true};
        }

        // The miss of _try_emplace, out of line: the hit (a lookup) is the
        // hot path of operator[] and of the weak containers' insertions
        template<class K, class... A>
        SGCL_NOINLINE Node* _emplace_absent(size_t hash, K&& key, A&&... a) {
            auto node = _make_keyed(std::forward<K>(key), std::forward<A>(a)...);
            _link_fresh(hash, node, nullptr);
            return node.get();
        }

        // The mapped value moved out of the element with the key and the
        // element erased, in one walk of the bucket (take); nothing when
        // the key is absent
        template<class K>
        optional<mapped_type> _take(const K& key) requires (!std::is_void_v<mapped_type>) {
            if (!_bucket_count) {
                return nullopt;
            }
            auto prev = _find_before(_hash(key), key);
            if (!prev) {
                return nullopt;
            }
            auto node = prev->next.get();
            optional<mapped_type> value(std::move(_node(node)->slot.value.second));
            _erase_after(prev, node);
            return value;
        }

        // The elements of `other`, in its order, with its bucket count: the
        // chain is copied node by node, so the layout is reproduced without
        // a lookup.
        void _copy_nodes(const HashTable& other) {
            if (!other._bucket_count) {
                return;
            }
            _rehash(other._bucket_count);
            if constexpr(ordered) {
                // in the other's insertion order, each linked as a new node
                // (the keys are unique: no lookup); the chain comes out in
                // its own order
                auto stop = other._end_node();
                for (auto p = other._first(); p != stop; p = _next(p)) {
                    NodePtr node = _make_node(_node(p)->slot.value);
                    _link_new(p->hash, node, nullptr);
                }
                return;
            }
            auto buckets = _buckets.get();
            LinkPtr tail = _before_begin;
            for (auto p = other._chain_first(); p; p = p->next.get()) {
                NodePtr node = _make_node(_node(p)->slot.value);
                node->hash = p->hash;
                tail->next = node;
                auto bucket = p->hash & _mask;
                if (!buckets[bucket]) {
                    buckets[bucket] = tail;
                }
                tail = node;
                ++_size;
            }
        }

        // operator==: the same keys with the same values, in any order (a
        // multi table: every run of equivalent keys a permutation of the
        // other's)
        bool _equal_to(const HashTable& other) const {
            if (_size != other._size) {
                return false;
            }
            if constexpr(unique) {
                for (auto p = _chain_first(); p; p = p->next.get()) {
                    auto q = other._find(_key(p));
                    if (!q || !(_node(p)->slot.value == q->slot.value)) {
                        return false;
                    }
                }
            } else {
                for (auto p = _chain_first(); p;) {
                    auto last = _run_end(p);
                    auto [q, q_last] = other._equal_range(_key(p));
                    if (!q || _run_length(p, last) != _run_length(q, q_last) || !_is_permutation(p, last, q, q_last)) {
                        return false;
                    }
                    p = last;
                }
            }
            return true;
        }

        template<class Pred>
        // erase_if: one walk, the nodes the predicate takes unlinked and
        // destroyed on the way
        size_type _erase_if(Pred& pred) {
            size_type count = 0;
            auto stop = _end_node();
            for (auto p = _first(); p != stop;) {
                if (pred(_node(p)->slot.value)) {
                    p = _erase(p);
                    ++count;
                } else {
                    p = _next(p);
                }
            }
            return count;
        }

    private:
        static constexpr size_type MinBucketCount = 8;

        tracked_ptr<LinkPtr> _buckets;        // managed array: each entry the node before its bucket's first
        tracked_ptr<Sentinel> _before_begin;   // sentinel: its next is the first node of the chain; in an ordered table, its after the first of the order and its before the last
        size_type _bucket_count = 0;
        size_type _mask = 0;
        size_type _size = 0;
        size_type _next_resize = 0;        // the table grows when the size reaches it
        float _max_load_factor = 1.0f;
        [[no_unique_address]] hasher _hash;
        [[no_unique_address]] key_equal _equal;

        // Empty, with no array: the state of a moved-from table
        void _reset() noexcept {
            _buckets = nullptr;
            _before_begin = nullptr;
            _bucket_count = 0;
            _mask = 0;
            _size = 0;
            _next_resize = 0;
        }

        // The first node of bucket n, null for an empty one
        HashNodeBase* _bucket_first(size_type n) const noexcept {
            auto before = n < _bucket_count ? _buckets.get()[n].get() : nullptr;
            return before ? before->next.get() : nullptr;
        }

        // The iterator of whatever an insertion returned
        static iterator _position(const std::pair<iterator, bool>& result) noexcept {
            return result.first;
        }

        static iterator _position(const insert_return_type& result) noexcept {
            return result.position;
        }

        static iterator _position(const iterator& result) noexcept {
            return result;
        }

        // A node with its element constructed. If the constructor throws,
        // the slot marks the node Destroyed and the holder just drops it.
        template<class... A>
        NodePtr _make_node(A&&... a) {
            static_assert(sizeof(Array<sizeof(Node)>) <= PageDataSize, "Element is too large");
            auto node = unique_ptr<Node>(UniquePtr<Node>(_node(Maker<Node>::make_tracked().release())));
            node.get()->slot.construct(std::forward<A>(a)...);
            return NodePtr(std::move(node));
        }

        // A node whose element is made from a key and, in a map, the
        // arguments of its mapped value (_try_emplace): the pair piecewise,
        // a set's element from the key alone
        template<class K, class... A>
        NodePtr _make_keyed(K&& key, A&&... a) {
            if constexpr(std::is_void_v<mapped_type>) {
                static_assert(sizeof...(A) == 0, "a set's element is its key alone");
                return _make_node(std::forward<K>(key));
            } else {
                return _make_node(std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple(std::forward<A>(a)...));
            }
        }

        // The node before the first node with `key`, or null: found in the
        // bucket of `hash`, hashes compared first.
        template<class K>
        HashNodeBase* _find_before(size_t hash, const K& key) const {
            if (!_bucket_count) {
                return nullptr;
            }
            auto bucket = hash & _mask;
            auto prev = _buckets.get()[bucket].get();
            if (!prev) {
                return nullptr;
            }
            for (auto p = prev->next.get();;) {
                if (p->hash == hash && _equal(_key(p), key)) {
                    return prev;
                }
                auto next = p->next.get();   // one load per step: the link read once, advanced with
                if (!next || (next->hash & _mask) != bucket) {
                    return nullptr;
                }
                prev = p;
                p = next;
            }
        }

        // One past the run of nodes with the key of `first` (they are
        // adjacent in the chain).
        HashNodeBase* _run_end(const HashNodeBase* first) const {
            auto p = first->next.get();
            while (p && p->hash == first->hash && _equal(_key(p), _key(first))) {
                p = p->next.get();
            }
            return p;
        }

        // A run of equivalent keys, [first, last): its length, and whether
        // another run holds the same values in some order (_equal_to)
        static size_type _run_length(const HashNodeBase* first, const HashNodeBase* last) noexcept {
            size_type n = 0;
            for (auto p = first; p != last; p = p->next.get()) {
                ++n;
            }
            return n;
        }

        static bool _is_permutation(const HashNodeBase* first, const HashNodeBase* last, const HashNodeBase* other_first, const HashNodeBase* other_last) {
            for (auto p = first; p != last; p = p->next.get()) {
                auto& value = _node(p)->slot.value;
                size_type here = 0;
                for (auto q = first; q != last; q = q->next.get()) {
                    here += _node(q)->slot.value == value;
                }
                size_type there = 0;
                for (auto q = other_first; q != other_last; q = q->next.get()) {
                    there += _node(q)->slot.value == value;
                }
                if (here != there) {
                    return false;
                }
            }
            return true;
        }

        template<class K>
        std::pair<HashNodeBase*, HashNodeBase*> _equal_range(const K& key) const {
            auto first = _find(key);
            if (!first) {
                return {nullptr, nullptr};
            }
            if constexpr(unique) {
                return {first, _next(first)};
            } else {
                return {first, _run_end(first)};
            }
        }

        template<class K>
        size_type _count(const K& key) const {
            auto [first, last] = _equal_range(key);
            return _run_length(first, last);
        }

        // Whether the key of what an insertion was given is read in place:
        // of a value_type by the traits, of a pair with the key type first
        // (the std::pair<Key, T> of a range) its first. Anything else is
        // converted to a value_type once, before the lookup.
        template<class V>
        static constexpr bool _keyed = std::is_same_v<std::remove_cvref_t<V>, value_type>
            || (!std::is_void_v<mapped_type> && requires(const std::remove_cvref_t<V>& v) { requires std::is_same_v<std::remove_cvref_t<decltype(v.first)>, key_type>; });

        template<class V>
        static const key_type& _key_of(const V& value) noexcept {
            if constexpr(std::is_same_v<V, value_type>) {
                return Traits::key(value);
            } else {
                return value.first;
            }
        }

        template<class V>
        // The insertion behind insert and emplace: the node before an
        // equivalent key found first (nothing inserted in a unique table,
        // the new node next to it in a multi one), else a new node at the
        // front of its bucket, the table grown first when it is full. The
        // key is read from the argument, so a pair of a range is hashed
        // and looked up where it is and copied once, into the node.
        auto _insert(V&& value) {
            if constexpr(!_keyed<V>) {
                return _insert(value_type(std::forward<V>(value)));
            } else {
                const key_type& key = _key_of(value);
                auto hash = _hash(key);
                auto prev = _find_before(hash, key);
                if constexpr(unique) {
                    if (prev) {
                        return std::pair<iterator, bool>(iterator(prev->next.get()), false);
                    }
                    auto node = _make_node(std::forward<V>(value));
                    _link_fresh(hash, node, nullptr);
                    return std::pair<iterator, bool>(iterator(node.get()), true);
                } else {
                    auto node = _make_node(std::forward<V>(value));
                    _link_fresh(hash, node, prev);
                    return iterator(node.get());
                }
            }
        }

        // A node made before the lookup (emplace): with the key present in a
        // unique table the element dies at once and the node is garbage.
        auto _insert_node(const NodePtr& node) {
            size_t hash;
            HashNodeBase* prev;
            try {
                hash = _hash(_key(node.get()));
                prev = _find_before(hash, _key(node.get()));
            }
            catch (...) {
                node.get()->slot.destroy();
                throw;
            }
            if constexpr(unique) {
                if (prev) {
                    node.get()->slot.destroy();
                    return std::pair<iterator, bool>(iterator(prev->next.get()), false);
                }
                _link_fresh(hash, node, nullptr);
                return std::pair<iterator, bool>(iterator(node.get()), true);
            } else {
                _link_fresh(hash, node, prev);
                return iterator(node.get());
            }
        }

        // Links a node made for this insert: a failure to grow leaves the
        // table as it was and destroys the element.
        void _link_fresh(size_t hash, const NodePtr& node, HashNodeBase* prev) {
            try {
                _link_new(hash, node, prev);
            }
            catch (...) {
                node.get()->slot.destroy();
                throw;
            }
        }

        // Links a node (rooted by the caller): at the front of its bucket,
        // or before the first node of its key when `prev` precedes one
        // (multi tables keep equal keys adjacent). Grows first if needed;
        // nothing changes if that fails.
        void _link_new(size_t hash, const NodePtr& node, HashNodeBase* prev) {
            if (_size >= _next_resize) {
                _grow();
                if (prev) {
                    prev = _find_before(hash, _key(node.get()));
                }
            }
            node->hash = hash;
            if (prev) {
                node->next = prev->next;
                prev->next = node;
            } else {
                _link_front(hash & _mask, node);
            }
            if constexpr(ordered) {
                _order_link(node.get(), _before_begin.get());
            }
            ++_size;
        }

        // The insertion order of an ordered table: a node linked before
        // `at` (the sentinel: at the back), a node taken out
        void _order_link(HashNodeBase* node, OrderedHashNodeBase* at) noexcept requires ordered {
            auto n = _order(node);
            n->before = at->before;
            n->after.reset(at);
            at->before->after.reset(n);
            at->before.reset(n);
        }

        void _order_unlink(HashNodeBase* node) noexcept requires ordered {
            auto n = _order(node);
            n->before->after = n->after;
            n->after->before = n->before;
            n->before = nullptr;
            n->after = nullptr;
        }

        // A node at the front of its bucket: after the bucket's before-node,
        // or at the front of the whole list when the bucket was empty (the
        // bucket then points at the sentinel, and the bucket of the node
        // that was first is made to point at the new one)
        void _link_front(size_type bucket, const NodePtr& node) {
            auto buckets = _buckets.get();
            if (auto before = buckets[bucket].get()) {
                node->next = before->next;
                before->next = node;
            } else {
                node->next = _before_begin->next;
                _before_begin->next = node;
                if (node->next) {
                    buckets[node->next->hash & _mask] = node;
                }
                buckets[bucket] = _before_begin;
            }
        }

        // Unlinks `node` (rooted by the caller), leaving its element alone;
        // its own link is cleared.
        void _unlink(HashNodeBase* node) {
            auto bucket = node->hash & _mask;
            auto prev = _buckets.get()[bucket].get();
            while (prev->next.get() != node) {
                prev = prev->next.get();
            }
            _unlink_after(prev, node);
        }

        // The same with the predecessor in hand (a lookup by key returns
        // it): no walk of the bucket's chain.
        void _unlink_after(HashNodeBase* prev, HashNodeBase* node) {
            auto bucket = node->hash & _mask;
            auto buckets = _buckets.get();
            auto next = node->next.get();
            if (prev == buckets[bucket].get()) {
                if (!next || (next->hash & _mask) != bucket) {
                    if (next) {
                        buckets[next->hash & _mask] = buckets[bucket];
                    }
                    buckets[bucket] = nullptr;
                }
            } else if (next && (next->hash & _mask) != bucket) {
                buckets[next->hash & _mask].reset(prev);
            }
            prev->next = node->next;
            node->next = nullptr;
            if constexpr(ordered) {
                _order_unlink(node);
            }
            --_size;
        }

        // Returns the node that followed. The node is rooted here while it
        // is unlinked and its element destroyed.
        HashNodeBase* _erase(HashNodeBase* node) {
            if (!node) {
                return nullptr;
            }
            Anchor keep(node);
            auto next = _next(node);
            _unlink(node);
            _node(node)->slot.destroy();
            return next;
        }

        // The node after prev unlinked and its element destroyed
        void _erase_after(HashNodeBase* prev, HashNodeBase* node) {
            Anchor keep(node);
            _unlink_after(prev, node);
            _node(node)->slot.destroy();
        }

        template<class K>
        // Every node with the key erased, or extracted into a node handle
        // (the first one, for extract)
        size_type _erase_key(const K& key) {
            if (!_bucket_count) {
                return 0;
            }
            auto hash = _hash(key);
            auto prev = _find_before(hash, key);
            if (!prev) {
                return 0;
            }
            if constexpr(unique) {
                _erase_after(prev, prev->next.get());
                return 1;
            } else {
                size_type count = 0;
                HashNodeBase* node;
                while ((node = prev->next.get()) && node->hash == hash && _equal(_key(node), key)) {
                    _erase_after(prev, node);   // prev stays: the next one moves up behind it
                    ++count;
                }
                return count;
            }
        }

        template<class K>
        node_type _extract_key(const K& key) {
            node_type nh;
            if (_bucket_count) {
                if (auto prev = _find_before(_hash(key), key)) {
                    auto node = prev->next.get();
                    nh._node = NodePtr(_node(node));
                    _unlink_after(prev, node);
                }
            }
            return nh;
        }

        // The load factor's arithmetic: the buckets `count` elements need,
        // the elements `bucket_count` buckets hold, the growth (to a power
        // of two, at least double)
        size_type _buckets_for(size_type count) const noexcept {
            return (size_type)std::ceil((double)count / (double)_max_load_factor);
        }

        size_type _threshold(size_type bucket_count) const noexcept {
            auto t = (double)bucket_count * (double)_max_load_factor;
            return t < (double)max_size() ? (size_type)t : max_size();
        }

        void _grow() {
            auto needed = std::bit_ceil(_buckets_for(_size + 1));
            _rehash(std::max({needed, _bucket_count * 2, MinBucketCount}));
        }

        // A new bucket array of `count` (a power of two) entries; the nodes
        // are relinked in chain order, so equal keys stay adjacent and in
        // order. The rest of the old chain is rooted from the stack while
        // its head is moved. The sentinel, made on the first call, is a
        // bare HashNodeBase: it never carries an element.
        void _rehash(size_type count) {
            auto buckets = unique_ptr<LinkPtr>(Maker<LinkPtr[]>::make_tracked_data(count));
            if (!_before_begin) {
                _before_begin = unique_ptr<Sentinel>(Maker<Sentinel>::make_tracked());
                if constexpr(ordered) {   // the order list, empty: closed on the sentinel
                    auto sentinel = _before_begin.get();
                    sentinel->before.reset(sentinel);
                    sentinel->after.reset(sentinel);
                }
            }
            LinkPtr p = _before_begin->next;
            _before_begin->next = nullptr;
            _buckets = std::move(buckets);
            _bucket_count = count;
            _mask = count - 1;
            _next_resize = _threshold(count);
            auto b = _buckets.get();
            HashNodeBase* prev = nullptr;
            size_type prev_bucket = 0;
            LinkPtr next;   // one stack root for the loop, assigned per node
            while (p) {
                next = p->next;
                auto bucket = p->hash & _mask;
                if (prev && prev_bucket == bucket) {
                    p->next = prev->next;
                    prev->next = p;
                    if (p->next && (p->next->hash & _mask) != bucket) {
                        b[p->next->hash & _mask] = p;
                    }
                } else if (b[bucket]) {
                    p->next = b[bucket]->next;
                    b[bucket]->next = p;
                } else {
                    p->next = _before_begin->next;
                    _before_begin->next = p;
                    if (p->next) {
                        b[p->next->hash & _mask] = p;
                    }
                    b[bucket] = _before_begin;
                }
                prev = p.get();
                prev_bucket = bucket;
                p = next;
            }
        }

        template<class> friend class HashTable;
    };
}
