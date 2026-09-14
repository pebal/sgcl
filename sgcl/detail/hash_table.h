//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../tracked_ptr.h"
#include "../unique_ptr.h"
#include "maker.h"
#include "anchor.h"
#include "managed.h"
#include "slot.h"

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

    // One raw node pointer, trivially copyable: the container roots every
    // linked node, and a raw pointer in a frame is seen by the conservative
    // stack scan. An iterator to an erased element is invalid as in std.
    template<class Node, class V>
    class HashIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
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
            _node = _node->next.get();
            return *this;
        }

        HashIterator operator++(int) noexcept {
            HashIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        operator HashIterator<Node, const value_type>() const noexcept requires (!std::is_const_v<V>) {
            return HashIterator<Node, const value_type>(_node);
        }

    private:
        HashNodeBase* _node = nullptr;

        explicit HashIterator(HashNodeBase* node) noexcept
        : _node(node) {
        }

        friend bool operator==(const HashIterator& lhs, const HashIterator& rhs) noexcept {
            return lhs._node == rhs._node;
        }

        template<class, class> friend class HashIterator;
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

        // The element dies as it was constructed, as the stored type
        void _release() {
            if (auto node = _node.get()) {
                reinterpret_cast<HashNode<managed_value_t<value_type>>*>(node)->slot.destroy();
                _node = nullptr;
            }
        }

        template<class> friend class HashTable;
    };

    template<class Hash, class Equal>
    concept TransparentLookup = requires {
        typename Hash::is_transparent;
        typename Equal::is_transparent;
    };

    // Root: the kind of the word by which the container holds its bucket
    // array and its sentinel, tracked_ptr or gc::tracked_ptr (types.h); the links
    // between the nodes are tracked_ptrs whatever it is.
    template<class Key, class T, class Hash, class Equal, bool Unique, template<class> class Root>
    struct HashMapTraits {
        using key_type = Key;
        using mapped_type = T;
        using value_type = std::pair<const Key, T>;
        // What a node stores (rb_tree.h: MapTraits)
        using stored_type = managed_value_t<value_type>;
        using hasher = Hash;
        using key_equal = Equal;
        template<class U> using root = Root<U>;
        static constexpr bool unique = Unique;
        static constexpr bool const_iterators = false;

        static const Key& key(const value_type& v) noexcept {
            return v.first;
        }
    };

    template<class Key, class Hash, class Equal, bool Unique, template<class> class Root>
    struct HashSetTraits {
        using key_type = Key;
        using mapped_type = void;
        using value_type = Key;
        using stored_type = managed_value_t<value_type>;
        using hasher = Hash;
        using key_equal = Equal;
        template<class U> using root = Root<U>;
        static constexpr bool unique = Unique;
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
    template<class Traits>
    class HashTable {
        using Node = HashNode<typename Traits::value_type>;         // the view of a node: its value as the interface sees it
        using StoredNode = HashNode<typename Traits::stored_type>;  // the node as allocated, constructed and destroyed
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
        using const_iterator = HashIterator<Node, const value_type>;
        using iterator = std::conditional_t<Traits::const_iterators, const_iterator, HashIterator<Node, value_type>>;
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

        HashTable& operator=(HashTable&& other) {
            if (this != &other) {
                clear();
                _buckets = other._buckets;
                _before_begin = other._before_begin;
                _bucket_count = other._bucket_count;
                _mask = other._mask;
                _size = other._size;
                _next_resize = other._next_resize;
                _max_load_factor = other._max_load_factor;
                _hash = std::move(other._hash);
                _equal = std::move(other._equal);
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
            return iterator();
        }

        const_iterator end() const noexcept {
            return const_iterator();
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
                _stored(_node(node))->slot.destroy();
                next = node->next;
                node->next = nullptr;
                p = next;
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
            return iterator(_erase(pos._node));
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
        // key this table (if unique) already holds stays in the source.
        template<class T> requires std::is_same_v<typename HashTable<T>::value_type, value_type>
        void merge(HashTable<T>& source) {
            if ((void*)&source == (void*)this) {
                return;
            }
            NodePtr p(_node(source._first()));
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

        template<class T> requires std::is_same_v<typename HashTable<T>::value_type, value_type>
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
            return iterator(_find(key));
        }

        const_iterator find(const key_type& key) const {
            return const_iterator(_find(key));
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        iterator find(const K& key) {
            return iterator(_find(key));
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        const_iterator find(const K& key) const {
            return const_iterator(_find(key));
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
            return {iterator(first), iterator(last)};
        }

        std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
            auto [first, last] = _equal_range(key);
            return {const_iterator(first), const_iterator(last)};
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        std::pair<iterator, iterator> equal_range(const K& key) {
            auto [first, last] = _equal_range(key);
            return {iterator(first), iterator(last)};
        }

        template<class K> requires TransparentLookup<hasher, key_equal>
        std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
            auto [first, last] = _equal_range(key);
            return {const_iterator(first), const_iterator(last)};
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
        // The element node behind a link: every linked node but the
        // sentinel is one, and the sentinel is never reached this way.
        // The node as stored, for its construction and destruction (a
        // handle destroys its value as the interface type, the same word)
        static StoredNode* _stored(Node* n) noexcept {
            return reinterpret_cast<StoredNode*>(n);
        }

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

        HashNodeBase* _first() const noexcept {
            auto sentinel = _before_begin.get();
            return sentinel ? sentinel->next.get() : nullptr;
        }

        template<class K>
        Node* _find(const K& key) const {
            if (!_bucket_count) {
                return nullptr;
            }
            auto prev = _find_before(_hash(key), key);
            return prev ? _node(prev->next.get()) : nullptr;
        }

        // Emplaces with the key looked up first: the element is constructed
        // only when it is inserted (try_emplace, operator[]).
        template<class K, class... A>
        std::pair<Node*, bool> _try_emplace(K&& key, A&&... a) {
            auto hash = _hash(key);
            if (auto prev = _find_before(hash, key)) {
                return {_node(prev->next.get()), false};
            }
            auto node = _make_node(std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple(std::forward<A>(a)...));
            _link_fresh(hash, node, nullptr);
            return {node.get(), true};
        }

        // The elements of `other`, in its order, with its bucket count: the
        // chain is copied node by node, so the layout is reproduced without
        // a lookup.
        void _copy_nodes(const HashTable& other) {
            if (!other._bucket_count) {
                return;
            }
            _rehash(other._bucket_count);
            auto buckets = _buckets.get();
            LinkPtr tail = _before_begin;
            for (auto p = other._first(); p; p = p->next.get()) {
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

        bool _equal_to(const HashTable& other) const {
            if (_size != other._size) {
                return false;
            }
            if constexpr(unique) {
                for (auto p = _first(); p; p = p->next.get()) {
                    auto q = other._find(_key(p));
                    if (!q || !(_node(p)->slot.value == q->slot.value)) {
                        return false;
                    }
                }
            } else {
                for (auto p = _first(); p;) {
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
        size_type _erase_if(Pred& pred) {
            size_type count = 0;
            for (auto p = _first(); p;) {
                if (pred(_node(p)->slot.value)) {
                    p = _erase(p);
                    ++count;
                } else {
                    p = p->next.get();
                }
            }
            return count;
        }

    private:
        static constexpr size_type MinBucketCount = 8;

        typename Traits::template root<LinkPtr> _buckets;        // managed array: each entry the node before its bucket's first
        typename Traits::template root<HashNodeBase> _before_begin;   // sentinel: its next is the first node
        size_type _bucket_count = 0;
        size_type _mask = 0;
        size_type _size = 0;
        size_type _next_resize = 0;        // the table grows when the size reaches it
        float _max_load_factor = 1.0f;
        [[no_unique_address]] hasher _hash;
        [[no_unique_address]] key_equal _equal;

        void _reset() noexcept {
            _buckets = nullptr;
            _before_begin = nullptr;
            _bucket_count = 0;
            _mask = 0;
            _size = 0;
            _next_resize = 0;
        }

        HashNodeBase* _bucket_first(size_type n) const noexcept {
            auto before = n < _bucket_count ? _buckets.get()[n].get() : nullptr;
            return before ? before->next.get() : nullptr;
        }

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
            auto node = unique_ptr<Node>(UniquePtr<Node>(_node(Maker<StoredNode>::make_tracked().release())));
            _stored(node.get())->slot.construct(std::forward<A>(a)...);
            return NodePtr(std::move(node));
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
                return {first, first->next.get()};
            } else {
                return {first, _run_end(first)};
            }
        }

        template<class K>
        size_type _count(const K& key) const {
            auto [first, last] = _equal_range(key);
            return _run_length(first, last);
        }

        template<class V>
        auto _insert(V&& value) {
            auto hash = _hash(Traits::key(value));
            auto prev = _find_before(hash, Traits::key(value));
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
                _stored(node.get())->slot.destroy();
                throw;
            }
            if constexpr(unique) {
                if (prev) {
                    _stored(node.get())->slot.destroy();
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
                _stored(node.get())->slot.destroy();
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
            ++_size;
        }

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
            --_size;
        }

        // Returns the node that followed. The node is rooted here while it
        // is unlinked and its element destroyed.
        HashNodeBase* _erase(HashNodeBase* node) {
            if (!node) {
                return nullptr;
            }
            Anchor keep(node);
            auto next = node->next.get();
            _unlink(node);
            _node(node)->slot.destroy();
            return next;
        }

        void _erase_after(HashNodeBase* prev, HashNodeBase* node) {
            Anchor keep(node);
            _unlink_after(prev, node);
            _node(node)->slot.destroy();
        }

        template<class K>
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
                _before_begin = unique_ptr<HashNodeBase>(Maker<HashNodeBase>::make_tracked());
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
