//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../aliases.h"
#include "../tracked_ptr.h"
#include "../unique_ptr.h"
#include "anchor.h"
#include "maker.h"
#include "nothrow_function.h"
#include "slot.h"
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
    // The bucket of a hash in a table of mask + 1 buckets (a power of
    // two): the hash, xored with a product of its bits above the mask. A
    // key below the bucket count is its own bucket (the product of 0 is
    // 0), so consecutive keys stay in consecutive buckets and their nodes
    // are walked in order, as with the hash alone; keys that differ only
    // above the mask (a stride of a power of two, aligned pointers), which
    // the mask alone put into one bucket (a find of 43 ns became 18 us),
    // are spread by the product, whose both halves carry every bit of the
    // part. One shift and one multiplication, no loop: it runs for every
    // node a walk passes
    SGCL_INLINE_HOT size_t hash_bucket(size_t hash, size_t mask) noexcept {
        const unsigned width = std::countr_one(mask);   // below 64: the mask of one bucket is 0
        const __uint128_t high = (__uint128_t)(hash >> width) * 0x9E3779B97F4A7C15ull;
        return (hash ^ size_t(high) ^ size_t(high >> 64)) & mask;
    }

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

        SGCL_INLINE_HOT reference operator*() const noexcept {
            return static_cast<Node*>(_node)->slot.value;
        }

        SGCL_INLINE_HOT pointer operator->() const noexcept {
            return std::addressof(static_cast<Node*>(_node)->slot.value);
        }

        SGCL_INLINE_HOT HashIterator& operator++() noexcept {
            if constexpr(Ordered) {
                _node = static_cast<OrderedHashNodeBase*>(_node)->after.get();
            } else {
                _node = _node->next.get();
            }
            return *this;
        }

        SGCL_INLINE_HOT HashIterator operator++(int) noexcept {
            HashIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        SGCL_INLINE_HOT HashIterator& operator--() noexcept requires Ordered {
            _node = static_cast<OrderedHashNodeBase*>(_node)->before.get();
            return *this;
        }

        SGCL_INLINE_HOT HashIterator operator--(int) noexcept requires Ordered {
            HashIterator tmp = *this;
            --(*this);
            return tmp;
        }

        SGCL_INLINE_HOT operator HashIterator<Node, const value_type, Ordered>() const noexcept requires (!std::is_const_v<V>) {
            return HashIterator<Node, const value_type, Ordered>(_node);
        }

    private:
        HashNodeBase* _node = nullptr;

        SGCL_INLINE_HOT explicit HashIterator(HashNodeBase* node) noexcept
        : _node(node) {
        }

        SGCL_INLINE_HOT friend bool operator==(const HashIterator& lhs, const HashIterator& rhs) noexcept {
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

        SGCL_INLINE_HOT reference operator*() const noexcept {
            return static_cast<Node*>(_node)->slot.value;
        }

        SGCL_INLINE_HOT pointer operator->() const noexcept {
            return std::addressof(static_cast<Node*>(_node)->slot.value);
        }

        SGCL_INLINE_HOT HashLocalIterator& operator++() noexcept {
            auto next = _node->next.get();
            if (next && (hash_bucket(next->hash, _mask)) != _bucket) {
                next = nullptr;
            }
            _node = next;
            return *this;
        }

        SGCL_INLINE_HOT HashLocalIterator operator++(int) noexcept {
            HashLocalIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        SGCL_INLINE_HOT operator HashLocalIterator<Node, const value_type>() const noexcept requires (!std::is_const_v<V>) {
            return HashLocalIterator<Node, const value_type>(_node, _bucket, _mask);
        }

    private:
        HashNodeBase* _node = nullptr;
        size_t _bucket = 0;
        size_t _mask = 0;

        SGCL_INLINE_HOT HashLocalIterator(HashNodeBase* node, size_t bucket, size_t mask) noexcept
        : _node(node)
        , _bucket(bucket)
        , _mask(mask) {
        }

        SGCL_INLINE_HOT friend bool operator==(const HashLocalIterator& lhs, const HashLocalIterator& rhs) noexcept {
            return lhs._node == rhs._node;
        }

        template<class, class> friend class HashLocalIterator;
        template<class> friend class HashTable;
    };

    // Owns one unlinked node. The element dies with the handle if it is not
    // inserted somewhere; the node itself is reclaimed by the collector.
    // The part of the handle a map's and a set's share; NodeHandle below
    // is the handle, with a map's key and mapped value or a set's value,
    // as std's node handles have them.
    template<class Node>
    class HashNodeHandleBase {
    public:
        HashNodeHandleBase() noexcept = default;

        SGCL_INLINE_HOT HashNodeHandleBase(HashNodeHandleBase&& other) noexcept
        : _node(other._node) {
            other._node = nullptr;
        }

        SGCL_INLINE_HOT HashNodeHandleBase& operator=(HashNodeHandleBase&& other) noexcept {
            if (this != &other) {
                _release();
                _node = other._node;
                other._node = nullptr;
            }
            return *this;
        }

        SGCL_INLINE_HOT ~HashNodeHandleBase() {
            _release();
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return !_node;
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return _node != nullptr;
        }

    protected:
        tracked_ptr<Node> _node;

        // Roots the node from the moment it is handed over: it is unlinked
        // from the table only after this.
        SGCL_INLINE_HOT explicit HashNodeHandleBase(Node* node) noexcept
        : _node(node) {
        }

        // The element destroyed with the handle. A handle dying in a sweep,
        // inside a managed object nobody refers to any more, holds a node
        // that is garbage of the same sweep: that node destroys its
        // element when the sweep reaches it, so it is left alone here
        // (if_alive: the rule of a destructor's tracked_ptr members).
        SGCL_INLINE_HOT void _release() noexcept {
            if (auto node = _node.if_alive()) {
                node->slot.destroy();
            }
            _node = nullptr;
        }

        template<class> friend class HashTable;
    };

    // A map's handle: the key, writable out of any table, and the value
    template<class Node, class Key, class Mapped>
    class NodeHandle
    : public HashNodeHandleBase<Node> {
        using Base = HashNodeHandleBase<Node>;

    public:
        using key_type = Key;
        using mapped_type = Mapped;

        NodeHandle() noexcept = default;

        SGCL_INLINE_HOT key_type& key() const noexcept {
            return const_cast<key_type&>(this->_node->slot.value.first);
        }

        SGCL_INLINE_HOT mapped_type& mapped() const noexcept {
            return this->_node->slot.value.second;
        }

        SGCL_INLINE_HOT void swap(NodeHandle& other) noexcept {
            this->_node.swap(other._node);
        }

        SGCL_INLINE_HOT friend void swap(NodeHandle& lhs, NodeHandle& rhs) noexcept {
            lhs.swap(rhs);
        }

    private:
        SGCL_INLINE_HOT explicit NodeHandle(Node* node) noexcept
        : Base(node) {
        }

        template<class> friend class HashTable;
    };

    // A set's handle: the element, writable out of any table
    template<class Node, class Key>
    class NodeHandle<Node, Key, void>
    : public HashNodeHandleBase<Node> {
        using Base = HashNodeHandleBase<Node>;

    public:
        using value_type = Key;

        NodeHandle() noexcept = default;

        SGCL_INLINE_HOT value_type& value() const noexcept {
            return this->_node->slot.value;
        }

        SGCL_INLINE_HOT void swap(NodeHandle& other) noexcept {
            this->_node.swap(other._node);
        }

        SGCL_INLINE_HOT friend void swap(NodeHandle& lhs, NodeHandle& rhs) noexcept {
            lhs.swap(rhs);
        }

    private:
        SGCL_INLINE_HOT explicit NodeHandle(Node* node) noexcept
        : Base(node) {
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

        SGCL_INLINE_HOT static const Key& key(const value_type& v) noexcept {
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

        SGCL_INLINE_HOT static const Key& key(const value_type& v) noexcept {
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
        static constexpr bool unique = Traits::unique;

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

        SGCL_INLINE_HOT HashTable() noexcept(std::is_nothrow_default_constructible_v<hasher> && std::is_nothrow_default_constructible_v<key_equal>) {
        }

        SGCL_INLINE_HOT explicit HashTable(size_type bucket_count, const hasher& hash = hasher(), const key_equal& equal = key_equal()) noexcept(std::is_nothrow_copy_constructible_v<hasher> && std::is_nothrow_copy_constructible_v<key_equal>)
        : _hash(hash)
        , _equal(equal) {
            if (bucket_count) {
                _rehash(std::bit_ceil(std::min(bucket_count, MaxBuckets)));
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

        SGCL_INLINE_HOT HashTable(std::initializer_list<value_type> ilist, size_type bucket_count = 0, const hasher& hash = hasher(), const key_equal& equal = key_equal())
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

        SGCL_INLINE_HOT HashTable(HashTable&& other) noexcept(std::is_nothrow_move_constructible_v<hasher> && std::is_nothrow_move_constructible_v<key_equal>)
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

        SGCL_INLINE_HOT ~HashTable() {
            if (!sweeping) {
                clear();
            }
        }

        SGCL_INLINE_HOT HashTable& operator=(const HashTable& other) {
            if (this != &other) {
                HashTable tmp(other);
                swap(tmp);
            }
            return *this;
        }

        // The hasher and the equality are moved first: one of them throwing
        // then leaves both tables as they were, not two over one chain
        HashTable& operator=(HashTable&& other) noexcept(std::is_nothrow_move_constructible_v<hasher> && std::is_nothrow_move_constructible_v<key_equal>
                                                          && std::is_nothrow_move_assignable_v<hasher> && std::is_nothrow_move_assignable_v<key_equal>) {
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

        SGCL_INLINE_HOT HashTable& operator=(std::initializer_list<value_type> ilist) {
            HashTable tmp(ilist, 0, _hash, _equal);
            tmp._max_load_factor = _max_load_factor;
            swap(tmp);
            return *this;
        }

        // iterators

        SGCL_INLINE_HOT iterator begin() noexcept {
            return iterator(_first());
        }

        SGCL_INLINE_HOT const_iterator begin() const noexcept {
            return const_iterator(_first());
        }

        SGCL_INLINE_HOT const_iterator cbegin() const noexcept {
            return begin();
        }

        SGCL_INLINE_HOT iterator end() noexcept {
            return iterator(_end_node());
        }

        SGCL_INLINE_HOT const_iterator end() const noexcept {
            return const_iterator(_end_node());
        }

        SGCL_INLINE_HOT const_iterator cend() const noexcept {
            return end();
        }

        // capacity

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _size == 0;
        }

        SGCL_INLINE_HOT size_type size() const noexcept {
            return _size;
        }

        SGCL_INLINE_HOT size_type max_size() const noexcept {
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

        SGCL_INLINE_HOT auto insert(const value_type& value) noexcept(std::is_nothrow_copy_constructible_v<value_type>) {
            return _insert(value);
        }

        SGCL_INLINE_HOT auto insert(value_type&& value) noexcept(std::is_nothrow_move_constructible_v<value_type>) {
            return _insert(std::move(value));
        }

        template<class P> requires std::is_constructible_v<value_type, P&&>
        SGCL_INLINE_HOT auto insert(P&& value) noexcept(_nothrow_emplace<P&&>()) {
            return emplace(std::forward<P>(value));
        }

        SGCL_INLINE_HOT iterator insert(const_iterator, const value_type& value) noexcept(std::is_nothrow_copy_constructible_v<value_type>) {
            return _position(_insert(value));
        }

        SGCL_INLINE_HOT iterator insert(const_iterator, value_type&& value) noexcept(std::is_nothrow_move_constructible_v<value_type>) {
            return _position(_insert(std::move(value)));
        }

        template<class P> requires std::is_constructible_v<value_type, P&&>
        SGCL_INLINE_HOT iterator insert(const_iterator, P&& value) noexcept(_nothrow_emplace<P&&>()) {
            return _position(emplace(std::forward<P>(value)));
        }

        template<std::input_iterator InputIt>
        void insert(InputIt first, InputIt last) {
            for (; first != last; ++first) {
                _insert(*first);
            }
        }

        SGCL_INLINE_HOT void insert(std::initializer_list<value_type> ilist) {
            insert(ilist.begin(), ilist.end());
        }

        auto insert(node_type&& nh) noexcept {
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

        // The hint is not used. A key that is there already leaves the
        // element in the handle, as std's does: only the iterator to the
        // element that holds the key comes back
        SGCL_INLINE_HOT iterator insert(const_iterator, node_type&& nh) noexcept {
            if constexpr(unique) {
                auto result = insert(std::move(nh));
                if (!result.inserted) {
                    nh = std::move(result.node);
                }
                return result.position;
            } else {
                return insert(std::move(nh));
            }
        }

        template<class... A>
        SGCL_INLINE_HOT auto emplace(A&&... a) noexcept(_nothrow_emplace<A&&...>()) {
            if constexpr(sizeof...(A) == 1 && (std::is_same_v<std::remove_cvref_t<A>, value_type> && ...)) {
                return _insert(std::forward<A>(a)...);
            } else {
                auto node = _make_node(std::forward<A>(a)...);
                return _insert_node(node);
            }
        }

        template<class... A>
        SGCL_INLINE_HOT iterator emplace_hint(const_iterator, A&&... a) noexcept(_nothrow_emplace<A&&...>()) {
            return _position(emplace(std::forward<A>(a)...));
        }

        SGCL_INLINE_HOT iterator erase(const_iterator pos) noexcept {
            return iterator(_wrap(_erase(pos._node)));
        }

        SGCL_INLINE_HOT iterator erase(iterator pos) noexcept requires (!std::is_same_v<iterator, const_iterator>) {
            return erase(const_iterator(pos));
        }

        // Every node is linked until its own _erase roots it; `stop` stays
        // linked throughout.
        iterator erase(const_iterator first, const_iterator last) noexcept {
            auto node = first._node;
            auto stop = last._node;
            while (node != stop) {
                node = _erase(node);
            }
            return iterator(node);
        }

        SGCL_INLINE_HOT size_type erase(const key_type& key) noexcept {
            return _erase_key(key);
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
            && (!std::is_convertible_v<K&&, iterator>) && (!std::is_convertible_v<K&&, const_iterator>)
        SGCL_INLINE_HOT size_type erase(K&& key) noexcept(_nothrow_lookup<std::remove_cvref_t<K>>()) {
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
        SGCL_INLINE_HOT node_type extract(const_iterator pos) noexcept {
            node_type nh(_node(pos._node));
            if (nh._node) {
                _unlink(nh._node.get());
            }
            return nh;
        }

        SGCL_INLINE_HOT node_type extract(const key_type& key) noexcept {
            return _extract_key(key);
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
            && (!std::is_convertible_v<K&&, iterator>) && (!std::is_convertible_v<K&&, const_iterator>)
        SGCL_INLINE_HOT node_type extract(K&& key) noexcept(_nothrow_lookup<std::remove_cvref_t<K>>()) {
            return _extract_key(key);
        }

        // Relinks the nodes: no element is copied or destroyed. A node whose
        // key this table (if unique) already holds stays in the source. The
        // source is walked in its own order, so an ordered table appends
        // the source's nodes in the order of their insertions. The
        // source's nodes must be this table's kind, as std asks of a merge:
        // the same node handle, so a map's of the same Key and T, a set's of
        // the same Key (a set of pairs does not take a map's nodes), and an
        // ordered table's from an ordered one, whose nodes carry the
        // order's two words.
        template<class T> requires std::is_same_v<typename HashTable<T>::node_type, node_type>
        void merge(HashTable<T>& source) noexcept {
            if ((void*)&source == (void*)this) {
                return;
            }
            auto stop = source._end_node();
            auto first = source._first();
            NodePtr p(first == stop ? nullptr : _node(first));
            while (p) {
                auto after = HashTable<T>::_next(p.get());   // before the unlink changes the node's links
                NodePtr next(after == stop ? nullptr : _node(after));
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

        template<class T> requires std::is_same_v<typename HashTable<T>::node_type, node_type>
        SGCL_INLINE_HOT void merge(HashTable<T>&& source) noexcept {
            merge(source);
        }

        // lookup

        SGCL_INLINE_HOT size_type count(const key_type& key) const noexcept {
            return _count(key);
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        SGCL_INLINE_HOT size_type count(const K& key) const noexcept(_nothrow_lookup<K>()) {
            return _count(key);
        }

        SGCL_INLINE_HOT iterator find(const key_type& key) noexcept {
            return iterator(_wrap(_find(key)));
        }

        SGCL_INLINE_HOT const_iterator find(const key_type& key) const noexcept {
            return const_iterator(_wrap(_find(key)));
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        SGCL_INLINE_HOT iterator find(const K& key) noexcept(_nothrow_lookup<K>()) {
            return iterator(_wrap(_find(key)));
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        SGCL_INLINE_HOT const_iterator find(const K& key) const noexcept(_nothrow_lookup<K>()) {
            return const_iterator(_wrap(_find(key)));
        }

        SGCL_INLINE_HOT bool contains(const key_type& key) const noexcept {
            return _find(key) != nullptr;
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        SGCL_INLINE_HOT bool contains(const K& key) const noexcept(_nothrow_lookup<K>()) {
            return _find(key) != nullptr;
        }

        SGCL_INLINE_HOT std::pair<iterator, iterator> equal_range(const key_type& key) noexcept {
            auto [first, last] = _equal_range(key);
            return {iterator(_wrap(first)), iterator(_wrap(last))};
        }

        SGCL_INLINE_HOT std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const noexcept {
            auto [first, last] = _equal_range(key);
            return {const_iterator(_wrap(first)), const_iterator(_wrap(last))};
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        SGCL_INLINE_HOT std::pair<iterator, iterator> equal_range(const K& key) noexcept(_nothrow_lookup<K>()) {
            auto [first, last] = _equal_range(key);
            return {iterator(_wrap(first)), iterator(_wrap(last))};
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        SGCL_INLINE_HOT std::pair<const_iterator, const_iterator> equal_range(const K& key) const noexcept(_nothrow_lookup<K>()) {
            auto [first, last] = _equal_range(key);
            return {const_iterator(_wrap(first)), const_iterator(_wrap(last))};
        }

        // bucket interface

        SGCL_INLINE_HOT size_type bucket_count() const noexcept {
            return _bucket_count;
        }

        SGCL_INLINE_HOT size_type max_bucket_count() const noexcept {
            return max_size();
        }

        size_type bucket_size(size_type n) const noexcept {
            size_type count = 0;
            for (auto p = _bucket_first(n); p && (hash_bucket(p->hash, _mask)) == n; p = p->next.get()) {
                ++count;
            }
            return count;
        }

        SGCL_INLINE_HOT size_type bucket(const key_type& key) const noexcept {
            return hash_bucket(_hash(key), _mask);
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        SGCL_INLINE_HOT size_type bucket(const K& key) const noexcept(nothrow_function_object<const hasher, const K&>) {
            return hash_bucket(_hash(key), _mask);
        }

        SGCL_INLINE_HOT local_iterator begin(size_type n) noexcept {
            return local_iterator(_bucket_first(n), n, _mask);
        }

        SGCL_INLINE_HOT const_local_iterator begin(size_type n) const noexcept {
            return const_local_iterator(_bucket_first(n), n, _mask);
        }

        SGCL_INLINE_HOT const_local_iterator cbegin(size_type n) const noexcept {
            return begin(n);
        }

        SGCL_INLINE_HOT local_iterator end(size_type n) noexcept {
            return local_iterator(nullptr, n, _mask);
        }

        SGCL_INLINE_HOT const_local_iterator end(size_type n) const noexcept {
            return const_local_iterator(nullptr, n, _mask);
        }

        SGCL_INLINE_HOT const_local_iterator cend(size_type n) const noexcept {
            return end(n);
        }

        // hash policy

        SGCL_INLINE_HOT float load_factor() const noexcept {
            return _bucket_count ? (float)_size / (float)_bucket_count : 0.0f;
        }

        SGCL_INLINE_HOT float max_load_factor() const noexcept {
            return _max_load_factor;
        }

        // Values that are not positive (or not numbers) are ignored. The
        // table grows on the next insert if it is now overloaded.
        SGCL_INLINE_HOT void max_load_factor(float z) noexcept {
            if (z > 0.0f) {
                _max_load_factor = z;
                _next_resize = _threshold(_bucket_count);
            }
        }

        // Rounds up to a power of two; never below what the elements need.
        // Only relinks the nodes: running out of memory ends the program
        // (a count past MaxBuckets is taken as it, an array the heap refuses).
        SGCL_INLINE_HOT void rehash(size_type count) noexcept {
            auto needed = std::max(count, _buckets_for(_size));
            if (needed) {
                needed = std::bit_ceil(std::min(needed, MaxBuckets));
                if (needed != _bucket_count) {
                    _rehash(needed);
                }
            }
        }

        SGCL_INLINE_HOT void reserve(size_type count) noexcept {
            rehash(_buckets_for(count));
        }

        // observers

        SGCL_INLINE_HOT hasher hash_function() const noexcept(std::is_nothrow_copy_constructible_v<hasher>) {
            return _hash;
        }

        SGCL_INLINE_HOT key_equal key_eq() const noexcept(std::is_nothrow_copy_constructible_v<key_equal>) {
            return _equal;
        }

    protected:
        using mapped_type = typename Traits::mapped_type;   // void for a set

        // What insert(node_type&&) of a table of unique keys returns; the
        // containers of unique keys make it a member type of theirs, as std
        // does (a multi table's node insert returns the iterator)
        struct insert_return_type {
            iterator position;
            bool inserted;
            node_type node;
        };

        // A lookup by K: the hash and the equality are required noexcept for
        // the key type (the containers' static_asserts); a transparent
        // lookup by another type is as noexcept as their calls with it
        template<class K>
        SGCL_INLINE_HOT static constexpr bool _nothrow_lookup() noexcept {
            if constexpr(std::is_same_v<K, key_type>) {
                return true;
            } else {
                return nothrow_function_object<const hasher, const K&> && nothrow_function_object<const key_equal, const key_type&, const K&>;
            }
        }

        // An insertion constructs the element and nothing else may throw:
        // the growth only relinks, and running out of memory ends the
        // program
        template<class... A>
        SGCL_INLINE_HOT static constexpr bool _nothrow_emplace() noexcept {
            return std::is_nothrow_constructible_v<value_type, A...>;
        }

        // The element of _try_emplace: a set's from the key, a map's pair
        // piecewise from the key and the mapped value's arguments
        template<class K, class... A>
        SGCL_INLINE_HOT static constexpr bool _nothrow_keyed() noexcept {
            if constexpr(std::is_void_v<mapped_type>) {
                return std::is_nothrow_constructible_v<value_type, K>;
            } else {
                return std::is_nothrow_constructible_v<key_type, K> && std::is_nothrow_constructible_v<mapped_type, A...>;
            }
        }

        // The element node behind a link: every linked node but the
        // sentinel is one, and the sentinel is never reached this way.
        SGCL_INLINE_HOT static Node* _node(HashNodeBase* p) noexcept {
            return static_cast<Node*>(p);
        }

        SGCL_INLINE_HOT static const Node* _node(const HashNodeBase* p) noexcept {
            return static_cast<const Node*>(p);
        }

        SGCL_INLINE_HOT static const key_type& _key(const HashNodeBase* node) noexcept {
            return Traits::key(_node(node)->slot.value);
        }

        SGCL_INLINE_HOT static iterator _make_iterator(HashNodeBase* node) noexcept {
            return iterator(node);
        }

        SGCL_INLINE_HOT static HashNodeBase* _node_of(const_iterator it) noexcept {
            return it._node;
        }

        // The first node of the chain every element is on (the buckets
        // point before their first node, the sentinel before the first of
        // all: libstdc++'s layout)
        SGCL_INLINE_HOT HashNodeBase* _chain_first() const noexcept {
            auto sentinel = _before_begin.get();
            return sentinel ? sentinel->next.get() : nullptr;
        }

        // The first node of the iteration: of the insertion order in an
        // ordered table (the sentinel, which is the end, when there is
        // none), of the chain otherwise
        SGCL_INLINE_HOT HashNodeBase* _first() const noexcept {
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
        SGCL_INLINE_HOT HashNodeBase* _end_node() const noexcept {
            if constexpr(ordered) {
                return _before_begin.get();
            } else {
                return nullptr;
            }
        }

        // A lookup's null as the end iterator's node
        SGCL_INLINE_HOT HashNodeBase* _wrap(HashNodeBase* p) const noexcept {
            return p ? p : _end_node();
        }

        // The node after `node` in the iteration
        SGCL_INLINE_HOT static HashNodeBase* _next(HashNodeBase* node) noexcept {
            if constexpr(ordered) {
                return _order(node)->after.get();
            } else {
                return node->next.get();
            }
        }

        SGCL_INLINE_HOT static OrderedHashNodeBase* _order(HashNodeBase* node) noexcept {
            return static_cast<OrderedHashNodeBase*>(node);
        }

        // Moves a linked node to the back (the front) of the insertion
        // order: the element counts as inserted last (first) from now on.
        // One that is there already is left alone: a cache touching its
        // newest element again (most hits, in one with an eviction order)
        // pays a load, not the eight stores of a relink
        SGCL_INLINE_HOT void _to_back(HashNodeBase* node) noexcept requires ordered {
            if (_order(node)->after.get() == _before_begin.get()) {
                return;
            }
            _order_unlink(node);
            _order_link(node, _before_begin.get());
        }

        SGCL_INLINE_HOT void _to_front(HashNodeBase* node) noexcept requires ordered {
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
        SGCL_INLINE_HOT Node* _find(const K& key) const noexcept(_nothrow_lookup<K>()) {
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
        SGCL_INLINE_HOT std::pair<Node*, bool> _try_emplace(K&& key, A&&... a) noexcept(_nothrow_lookup<std::remove_cvref_t<K>>() && _nothrow_keyed<K&&, A&&...>()) {
            auto hash = _hash(key);
            if (auto prev = _find_before(hash, key)) {
                return {_node(prev->next.get()), false};
            }
            return {_emplace_absent(hash, std::forward<K>(key), std::forward<A>(a)...), true};
        }

        // The miss of _try_emplace, out of line: the hit (a lookup) is the
        // hot path of operator[] and of the weak containers' insertions
        template<class K, class... A>
        SGCL_NOINLINE Node* _emplace_absent(size_t hash, K&& key, A&&... a) noexcept(_nothrow_keyed<K&&, A&&...>()) {
            auto node = _make_keyed(std::forward<K>(key), std::forward<A>(a)...);
            _link_new(hash, node, nullptr);
            return node.get();
        }

        // The mapped value moved out of the element with the key and the
        // element erased, in one walk of the bucket (take); nothing when
        // the key is absent
        template<class K>
        optional<mapped_type> _take(const K& key) noexcept(_nothrow_lookup<K>() && std::is_nothrow_move_constructible_v<mapped_type>) requires (!std::is_void_v<mapped_type>) {
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
                auto bucket = hash_bucket(p->hash, _mask);
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

        // The predicate of erase_if called with an element: a set's key as
        // const, as its iterators give it (a key changed in place would no
        // longer hash to its bucket), a map's pair as it is
        template<class Pred>
        SGCL_INLINE_HOT static bool _taken(Pred& pred, value_type& value) {
            if constexpr(Traits::const_iterators) {
                return pred(std::as_const(value));
            } else {
                return pred(value);
            }
        }

        template<class Pred>
        // erase_if: one walk, the nodes the predicate takes unlinked and
        // destroyed on the way
        size_type _erase_if(Pred& pred) {
            size_type count = 0;
            auto stop = _end_node();
            for (auto p = _first(); p != stop;) {
                if (_taken(pred, _node(p)->slot.value)) {
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

        // The most buckets an array is made of: the largest power of two
        // of entries within the bytes of an address space. A count past it
        // is taken as it, an array the heap refuses, so that the program
        // ends as at any refused managed allocation (DESIGN 356): rounded
        // up as it is, its power of two would be undefined
        static constexpr size_type MaxBuckets = std::bit_floor(size_type(std::numeric_limits<difference_type>::max()) / sizeof(LinkPtr));

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
        SGCL_INLINE_HOT HashNodeBase* _bucket_first(size_type n) const noexcept {
            auto before = n < _bucket_count ? _buckets.get()[n].get() : nullptr;
            return before ? before->next.get() : nullptr;
        }

        // The iterator of whatever an insertion returned
        SGCL_INLINE_HOT static iterator _position(const std::pair<iterator, bool>& result) noexcept {
            return result.first;
        }

        SGCL_INLINE_HOT static iterator _position(const insert_return_type& result) noexcept {
            return result.position;
        }

        SGCL_INLINE_HOT static iterator _position(const iterator& result) noexcept {
            return result;
        }

        // A node with its element constructed. If the constructor throws,
        // the slot marks the node Destroyed and the holder just drops it.
        template<class... A>
        SGCL_INLINE_HOT NodePtr _make_node(A&&... a) noexcept(std::is_nothrow_constructible_v<value_type, A...>) {
            static_assert(sizeof(Array<sizeof(Node)>) <= PageDataSize, "Element is too large");
            auto node = unique_ptr<Node>(UniquePtr<Node>(_node(Maker<Node>::make_tracked().release())));
            node.get()->slot.construct(std::forward<A>(a)...);
            return NodePtr(std::move(node));
        }

        // A node whose element is made from a key and, in a map, the
        // arguments of its mapped value (_try_emplace): the pair piecewise,
        // a set's element from the key alone
        template<class K, class... A>
        SGCL_INLINE_HOT NodePtr _make_keyed(K&& key, A&&... a) noexcept(_nothrow_keyed<K&&, A&&...>()) {
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
        HashNodeBase* _find_before(size_t hash, const K& key) const noexcept(_nothrow_lookup<K>()) {
            if (!_bucket_count) {
                return nullptr;
            }
            auto bucket = hash_bucket(hash, _mask);
            auto prev = _buckets.get()[bucket].get();
            if (!prev) {
                return nullptr;
            }
            for (auto p = prev->next.get();;) {
                if (p->hash == hash && _equal(_key(p), key)) {
                    return prev;
                }
                auto next = p->next.get();   // one load per step: the link read once, advanced with
                if (!next || (hash_bucket(next->hash, _mask)) != bucket) {
                    return nullptr;
                }
                prev = p;
                p = next;
            }
        }

        // One past the run of nodes with the key of `first` (they are
        // adjacent in the chain).
        HashNodeBase* _run_end(const HashNodeBase* first) const noexcept {
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
        SGCL_INLINE_HOT std::pair<HashNodeBase*, HashNodeBase*> _equal_range(const K& key) const noexcept(_nothrow_lookup<K>()) {
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

        // A unique table: found or not. A multi one: the run in the chain.
        // (Not _equal_range's pair for a unique table: in an ordered one its
        // `last` is the next in insertion order, which the chain that
        // _run_length walks need not reach.)
        template<class K>
        SGCL_INLINE_HOT size_type _count(const K& key) const noexcept(_nothrow_lookup<K>()) {
            if constexpr(unique) {
                return _find(key) ? 1 : 0;
            } else {
                auto [first, last] = _equal_range(key);
                return _run_length(first, last);
            }
        }

        // Whether the key of what an insertion was given is read in place:
        // of a value_type by the traits, of a pair with the key type first
        // (the std::pair<Key, T> of a range) its first. Anything else is
        // converted to a value_type once, before the lookup.
        template<class V>
        static constexpr bool _keyed = std::is_same_v<std::remove_cvref_t<V>, value_type>
            || (!std::is_void_v<mapped_type> && requires(const std::remove_cvref_t<V>& v) { requires std::is_same_v<std::remove_cvref_t<decltype(v.first)>, key_type>; });

        template<class V>
        SGCL_INLINE_HOT static const key_type& _key_of(const V& value) noexcept {
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
        auto _insert(V&& value) noexcept(_nothrow_emplace<V&&>()) {
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
                    _link_new(hash, node, nullptr);
                    return std::pair<iterator, bool>(iterator(node.get()), true);
                } else {
                    auto node = _make_node(std::forward<V>(value));
                    _link_new(hash, node, prev);
                    return iterator(node.get());
                }
            }
        }

        // A node made before the lookup (emplace): with the key present in a
        // unique table the element dies at once and the node is garbage.
        // The hash and the equality are noexcept and a growth cannot fail
        // (running out of memory ends the program): nothing to undo.
        auto _insert_node(const NodePtr& node) noexcept {
            auto hash = _hash(_key(node.get()));
            auto prev = _find_before(hash, _key(node.get()));
            if constexpr(unique) {
                if (prev) {
                    node.get()->slot.destroy();
                    return std::pair<iterator, bool>(iterator(prev->next.get()), false);
                }
                _link_new(hash, node, nullptr);
                return std::pair<iterator, bool>(iterator(node.get()), true);
            } else {
                _link_new(hash, node, prev);
                return iterator(node.get());
            }
        }

        // Links a node (rooted by the caller): at the front of its bucket,
        // or before the first node of its key when `prev` precedes one
        // (multi tables keep equal keys adjacent). Grows first if needed.
        void _link_new(size_t hash, const NodePtr& node, HashNodeBase* prev) noexcept {
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
                _link_front(hash_bucket(hash, _mask), node);
            }
            if constexpr(ordered) {
                _order_link(node.get(), _before_begin.get());
            }
            ++_size;
        }

        // The insertion order of an ordered table: a node linked before
        // `at` (the sentinel: at the back), a node taken out
        SGCL_INLINE_HOT void _order_link(HashNodeBase* node, OrderedHashNodeBase* at) noexcept requires ordered {
            auto n = _order(node);
            n->before = at->before;
            n->after.reset(at);
            at->before->after.reset(n);
            at->before.reset(n);
        }

        SGCL_INLINE_HOT void _order_unlink(HashNodeBase* node) noexcept requires ordered {
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
        void _link_front(size_type bucket, const NodePtr& node) noexcept {
            auto buckets = _buckets.get();
            if (auto before = buckets[bucket].get()) {
                node->next = before->next;
                before->next = node;
            } else {
                node->next = _before_begin->next;
                _before_begin->next = node;
                if (node->next) {
                    buckets[hash_bucket(node->next->hash, _mask)] = node;
                }
                buckets[bucket] = _before_begin;
            }
        }

        // Unlinks `node` (rooted by the caller), leaving its element alone;
        // its own link is cleared.
        void _unlink(HashNodeBase* node) noexcept {
            auto bucket = hash_bucket(node->hash, _mask);
            auto prev = _buckets.get()[bucket].get();
            while (prev->next.get() != node) {
                prev = prev->next.get();
            }
            _unlink_after(prev, node);
        }

        // The same with the predecessor in hand (a lookup by key returns
        // it): no walk of the bucket's chain.
        void _unlink_after(HashNodeBase* prev, HashNodeBase* node) noexcept {
            auto bucket = hash_bucket(node->hash, _mask);
            auto buckets = _buckets.get();
            auto next = node->next.get();
            if (prev == buckets[bucket].get()) {
                if (!next || (hash_bucket(next->hash, _mask)) != bucket) {
                    if (next) {
                        buckets[hash_bucket(next->hash, _mask)] = buckets[bucket];
                    }
                    buckets[bucket] = nullptr;
                }
            } else if (next && (hash_bucket(next->hash, _mask)) != bucket) {
                buckets[hash_bucket(next->hash, _mask)].reset(prev);
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
        HashNodeBase* _erase(HashNodeBase* node) noexcept {
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
        SGCL_INLINE_HOT void _erase_after(HashNodeBase* prev, HashNodeBase* node) noexcept {
            Anchor keep(node);
            _unlink_after(prev, node);
            _node(node)->slot.destroy();
        }

        template<class K>
        // Every node with the key erased, or extracted into a node handle
        // (the first one, for extract)
        size_type _erase_key(const K& key) noexcept(_nothrow_lookup<K>()) {
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
                // The run's end found before anything is erased: the key
                // may be one of the run's own (erase(it->first)), read
                // no more once its element is destroyed
                auto last = _run_end(prev->next.get());
                size_type count = 0;
                HashNodeBase* node;
                while ((node = prev->next.get()) != last) {
                    _erase_after(prev, node);   // prev stays: the next one moves up behind it
                    ++count;
                }
                return count;
            }
        }

        template<class K>
        SGCL_INLINE_HOT node_type _extract_key(const K& key) noexcept(_nothrow_lookup<K>()) {
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
        // of two, at least double). The buckets past MaxBuckets are
        // MaxBuckets: a quotient past size_t (a count near it, a load
        // factor near zero) would not convert
        SGCL_INLINE_HOT size_type _buckets_for(size_type count) const noexcept {
            auto needed = std::ceil((double)count / (double)_max_load_factor);
            return needed < (double)MaxBuckets ? (size_type)needed : MaxBuckets;
        }

        // No buckets hold no element, whatever the factor: the first
        // insertion makes the array (0 times an infinite factor is NaN,
        // which made the largest size, and the insertion went on into
        // an array that was not there)
        SGCL_INLINE_HOT size_type _threshold(size_type bucket_count) const noexcept {
            if (!bucket_count) {
                return 0;
            }
            auto t = (double)bucket_count * (double)_max_load_factor;
            return t < (double)max_size() ? (size_type)t : max_size();
        }

        SGCL_INLINE_HOT void _grow() noexcept {
            auto needed = std::bit_ceil(_buckets_for(_size + 1));
            _rehash(std::max({needed, _bucket_count * 2, MinBucketCount}));
        }

        // A new bucket array of `count` (a power of two) entries; the nodes
        // are relinked in chain order, so equal keys stay adjacent and in
        // order. The rest of the old chain is rooted from the stack while
        // its head is moved. The sentinel, made on the first call, is a
        // bare HashNodeBase: it never carries an element.
        void _rehash(size_type count) noexcept {
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
                auto bucket = hash_bucket(p->hash, _mask);
                if (prev && prev_bucket == bucket) {
                    p->next = prev->next;
                    prev->next = p;
                    if (p->next && (hash_bucket(p->next->hash, _mask)) != bucket) {
                        b[hash_bucket(p->next->hash, _mask)] = p;
                    }
                } else if (b[bucket]) {
                    p->next = b[bucket]->next;
                    b[bucket]->next = p;
                } else {
                    p->next = _before_begin->next;
                    _before_begin->next = p;
                    if (p->next) {
                        b[hash_bucket(p->next->hash, _mask)] = p;
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
