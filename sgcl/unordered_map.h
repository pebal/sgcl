//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "make_tracked.h"
#include "tracked_ptr.h"

#include <cmath>
#include <unordered_map>

namespace sgcl {
    namespace detail {
        template <typename K, typename Key, typename Hash, typename KeyEqual>
        concept HashableWith = requires(const Hash& h, const KeyEqual& eq, const Key& k, const K& x) {
            { h(x) } -> std::convertible_to<std::size_t>;
            { eq(k, x) } -> std::convertible_to<bool>;
        };
    }

    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class unordered_map {
        struct Node;
        using NodePtr = tracked_ptr<Node>;

        struct Node {
            std::pair<const Key, T> p;
            NodePtr next;
        };

        template <bool IsConst>
        class Iterator {
            using MapPtr = typename std::conditional_t<IsConst, const unordered_map*, unordered_map*>;
            using NodePtr = typename std::conditional_t<IsConst, tracked_ptr<const Node>, tracked_ptr<Node>>;

        public:
            using value_type = std::pair<const Key, T>;
            using reference = typename std::conditional_t<IsConst, const value_type&, value_type&>;
            using pointer = typename std::conditional_t<IsConst, const value_type*, value_type*>;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;

        public:
            Iterator(MapPtr map, size_t index, NodePtr node)
            : _map(map)
            , _bucket_index(index)
            , _node(node) {
                if (!_node && _bucket_index < _map->_buckets.size()) {
                    _to_next_valid();
                }
            }

            reference operator*() const noexcept {
                return _node->p;
            }

            pointer operator->() const noexcept {
                return std::addressof(_node->p);
            }

            Iterator& operator++() noexcept {
                if (_node) {
                    _node = _node->next;
                }
                _to_next_valid();
                return *this;
            }

            template<bool B>
            bool operator==(const Iterator<B>& other) const noexcept {
                return _node == other._node;
            }

            template<bool B>
            bool operator!=(const Iterator<B>& other) const noexcept {
                return !(*this == other);
            }

            operator Iterator<true>&() const noexcept {
                return *(Iterator<true>*)(this);
            }

        private:
            MapPtr _map;
            size_t _bucket_index;
            NodePtr _node;

            void _to_next_valid() {
                while(!_node && _bucket_index + 1 < _map->_buckets.size()) {
                    ++_bucket_index;
                    _node = _map->_buckets[_bucket_index];
                }
            }

            template<bool> friend class Iterator;
            template<class, class, class, class> friend class unordered_map;
        };

        static constexpr size_t MinBucketCount = 8;

    public:
        using key_type = Key;
        using mapped_type = T;
        using value_type = std::pair<const Key, T>;
        using reference = value_type;
        using const_reference = const value_type&;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using hasher = Hash;
        using key_equal = KeyEqual;
        using iterator = Iterator<false>;
        using const_iterator = Iterator<true>;
        using is_transparent = void;

        struct node_type {
            NodePtr _node;

            node_type() = default;
            explicit node_type(NodePtr node) : _node(std::move(node)) {}

            bool empty() const noexcept {
                return !_node;
            }

            void clear() noexcept {
                _node = nullptr;
            }

            const Key& key() const {
                return _node->p.first;
            }

            T& value() {
                return _node->p.second;
            }
        };

        struct insert_return_type {
            iterator position;
            bool inserted;
            node_type node;
        };

        unordered_map() noexcept = default;

        unordered_map(
            size_t bucket_count
          , const Hash& hash = Hash()
          , const KeyEqual& equal = KeyEqual()
        )
        : _buckets(bucket_count)
        , _hash(hash)
        , _equal(equal) {
        }

        template <typename InputIt>
        unordered_map(
            InputIt first
          , InputIt last
          , size_t bucket_count = MinBucketCount
          , const Hash& hash = Hash()
          , const KeyEqual& equal = KeyEqual()
        )
        : _buckets(bucket_count)
        , _hash(hash)
        , _equal(equal) {
            insert(first, last);
        }

        unordered_map(
            std::initializer_list<value_type> init
          , size_t bucket_count = MinBucketCount
          , const Hash& hash = Hash()
          , const KeyEqual& equal = KeyEqual()
        )
        : _buckets(bucket_count)
        , _hash(hash)
        , _equal(equal) {
            insert(init);
        }

        unordered_map(const unordered_map& other)
        : _buckets(other._buckets.size())
        , _hash(other._hash)
        , _equal(other._equal)
        , _max_load_factor(other._max_load_factor)
        , _size(0) {
            for (const auto& node: other) {
                insert(node);
            }
        }

        unordered_map(unordered_map&& other)
        : _buckets(std::move(other._buckets))
        , _hash(std::move(other._hash))
        , _equal(std::move(other._equal))
        , _max_load_factor(other._max_load_factor)
        , _size(other._size) {
            other._size = 0;
        }

        unordered_map& operator=(const unordered_map& other) {
            if (this != &other) {
                unordered_map temp(other);
                swap(temp);
            }
            return *this;
        }

        unordered_map& operator=(unordered_map&& other) {
            if (this != &other) {
                unordered_map temp(std::move(other));
                swap(temp);
            }
            return *this;
        }

        friend bool operator==(const unordered_map& lhs, const unordered_map& rhs) noexcept {
            if (lhs.size() != rhs.size()) {
                return false;
            }
            for (const auto& [key, val] : lhs) {
                auto found = rhs.find(key);
                if (found == rhs.end() || (*found).second != val) {
                    return false;
                }
            }
            return true;
        }

        friend bool operator!=(const unordered_map& lhs, const unordered_map& rhs) noexcept {
            return !(lhs == rhs);
        }

        T& operator[](const Key& key) noexcept {
            if (_buckets.size()) {
                size_t idx = _bucket_index(key);
                NodePtr current = _buckets[idx];
                while (current) {
                    if (_equal(current->p.first, key)) {
                        return current->p.second;
                    }
                    current = current->next;
                }
            }
            _ensure_capacity();
            auto idx = _bucket_index(key);
            auto& ref = _buckets[idx];
            ref = make_tracked<Node>(std::make_pair(key, T{}), _buckets[idx]);
            ++_size;
            return ref->p.second;
        }

        T& operator[](Key&& key) noexcept {
            if (_buckets.size()) {
                size_t idx = _bucket_index(key);
                NodePtr current = _buckets[idx];
                while (current) {
                    if (_equal(current->p.first, key)) {
                        return current->p.second;
                    }
                    current = current->next;
                }
            }
            _ensure_capacity();
            auto idx = _bucket_index(key);
            auto& ref = _buckets[idx];
            ref = make_tracked<Node>(std::make_pair(std::move(key), T{}), _buckets[idx]);
            ++_size;
            return ref->p.second;
        }

        T& at(const Key& key) {
            if (!_buckets.size()) {
                throw std::out_of_range("sgcl::unordered_map::at: map is empty");
            }
            size_t idx = _bucket_index(key);
            NodePtr current = _buckets[idx];
            while (current) {
                if (_equal(current->p.first, key)) {
                    return current->p.second;
                }
                current = current->next;
            }
            throw std::out_of_range("sgcl::unordered_map::at: key not found");
        }

        const T& at(const Key& key) const {
            if (!_buckets.size()) {
                throw std::out_of_range("sgcl::unordered_map::at: map is empty");
            }
            size_t idx = _bucket_index(key);
            NodePtr current = _buckets[idx];
            while (current) {
                if (_equal(current->p.first, key)) {
                    return current->p.second;
                }
                current = current->next;
            }
            throw std::out_of_range("sgcl::unordered_map::at: key not found");
        }

        std::pair<iterator, bool> insert(const value_type& val) {
            _ensure_capacity();
            size_t idx = _bucket_index(val.first);
            NodePtr current = _buckets[idx];
            while (current) {
                if (_equal(current->p.first, val.first)) {
                    return { iterator(this, idx, current), false };
                }
                current = current->next;
            }
            _buckets[idx] = make_tracked<Node>(val, _buckets[idx]);
            ++_size;
            return { iterator(this, idx, _buckets[idx]), true };
        }

        std::pair<iterator, bool> insert(value_type&& val) {
            _ensure_capacity();
            size_t idx = _bucket_index(val.first);
            NodePtr current = _buckets[idx];
            while (current) {
                if (_equal(current->p.first, val.first)) {
                    return { iterator(this, idx, current), false };
                }
                current = current->next;
            }
            _buckets[idx] = make_tracked<Node>(std::move(val), _buckets[idx]);
            ++_size;
            return { iterator(this, idx, _buckets[idx]), true };
        }

        template <typename P> requires std::constructible_from<value_type, P>
        std::pair<iterator, bool> insert(P&& p) {
            return insert(value_type(std::forward<P>(p)));
        }

        iterator insert(const_iterator, const value_type& val) {
            return insert(val).first;
        }

        iterator insert(const_iterator, value_type&& val) {
            return insert(std::move(val)).first;
        }

        template <typename P>
        std::pair<iterator, bool> insert(const_iterator, P&& p) {
            return insert(value_type(std::forward<P>(p)));
        }

        template <typename InputIt>
        void insert(InputIt first, InputIt last) {
            for (; first != last; ++first) {
                insert(*first);
            }
        }

        void insert(std::initializer_list<value_type> ilist) {
            for (const auto& val : ilist) {
                insert(val);
            }
        }

        insert_return_type insert(node_type&& nh) {
            if (!nh._node) {
                return { end(), false, std::move(nh) };
            }
            const Key& key = nh._node->p.first;
            size_t idx = _bucket_index(key);
            NodePtr current = _buckets[idx];
            while (current) {
                if (_equal(current->p.first, key)) {
                    return { iterator(this, idx, current), false, std::move(nh) };
                }
                current = current->next;
            }
            nh._node->next = _buckets[idx];
            _buckets[idx] = std::move(nh._node);
            ++_size;
            return { iterator(this, idx, _buckets[idx]), true, node_type{} };
        }

        iterator insert(const_iterator, node_type&& nh) {
            return insert(std::move(nh)).position;
        }

        template <class M>
        std::pair<iterator, bool> insert_or_assign(const Key& k, M&& obj) {
            if (_buckets.size()) {
                size_t idx = _bucket_index(k);
                NodePtr current = _buckets[idx];
                while (current) {
                    if (_equal(current->p.first, k)) {
                        current->p.second = std::forward<M>(obj);
                        return { iterator(this, idx, current), false };
                    }
                    current = current->next;
                }
            }
            _ensure_capacity();
            size_t idx = _bucket_index(k);
            _buckets[idx] = make_tracked<Node>(std::make_pair(k, std::forward<M>(obj)), _buckets[idx]);
            ++_size;
            return { iterator(this, idx, _buckets[idx]), true };
        }

        template <class M>
        std::pair<iterator, bool> insert_or_assign(Key&& k, M&& obj) {
            if (_buckets.size()) {
                size_t idx = _bucket_index(k);
                NodePtr current = _buckets[idx];
                while (current) {
                    if (_equal(current->p.first, k)) {
                        current->p.second = std::forward<M>(obj);
                        return { iterator(this, idx, current), false };
                    }
                    current = current->next;
                }
            }
            _ensure_capacity();
            size_t idx = _bucket_index(k);
            _buckets[idx] = make_tracked<Node>(std::make_pair(std::move(k), std::forward<M>(obj)), _buckets[idx]);
            ++_size;
            return { iterator(this, idx, _buckets[idx]), true };
        }

        template <class M>
        iterator insert_or_assign(const_iterator, const Key& k, M&& obj) {
            return insert_or_assign(k, std::forward<M>(obj)).first;
        }

        template <class M>
        iterator insert_or_assign(const_iterator, Key&& k, M&& obj) {
            return insert_or_assign(std::move(k), std::forward<M>(obj)).first;
        }

        template <class... Args>
        std::pair<iterator, bool> emplace(Args&&... args) {
            value_type val(std::forward<Args>(args)...);
            return insert(std::move(val));
        }

        template <class... Args>
        iterator emplace_hint(const_iterator, Args&&... args) {
            return emplace(std::forward<Args>(args)...).first;
        }

        template <class... Args>
        std::pair<iterator, bool> try_emplace(const Key& k, Args&&... args) {
            if (_buckets.size()) {
                size_t idx = _bucket_index(k);
                NodePtr current = _buckets[idx];
                while (current) {
                    if (_equal(current->p.first, k)) {
                        return { iterator(this, idx, current), false };
                    }
                    current = current->next;
                }
            }
            _ensure_capacity();
            size_t idx = _bucket_index(k);
            _buckets[idx] = make_tracked<Node>(std::make_pair(k, T(std::forward<Args>(args)...)), _buckets[idx]);
            ++_size;
            return { iterator(this, idx, _buckets[idx]), true };
        }

        template <class... Args>
        std::pair<iterator, bool> try_emplace(Key&& k, Args&&... args) {
            if (_buckets.size()) {
                size_t idx = _bucket_index(k);
                NodePtr current = _buckets[idx];
                while (current) {
                    if (_equal(current->p.first, k)) {
                        return { iterator(this, idx, current), false };
                    }
                    current = current->next;
                }
            }
            _ensure_capacity();
            size_t idx = _bucket_index(k);
            _buckets[idx] = make_tracked<Node>(std::make_pair(std::move(k), T(std::forward<Args>(args)...)), _buckets[idx]);
            ++_size;
            return { iterator(this, idx, _buckets[idx]), true };
        }

        template <class... Args>
        iterator try_emplace(const_iterator, const Key& k, Args&&... args) {
            return try_emplace(k, std::forward<Args>(args)...).first;
        }

        template <class... Args>
        iterator try_emplace(const_iterator, Key&& k, Args&&... args) {
            return try_emplace(std::move(k), std::forward<Args>(args)...).first;
        }

        iterator erase(iterator pos) {
            if (pos != end() && _buckets.size()) {
                size_t idx = pos._bucket_index;
                NodePtr current = _buckets[idx];
                NodePtr prev;
                while (current) {
                    if (current == pos._node) {
                        if (prev) {
                            prev->next = current->next;
                        } else {
                            _buckets[idx] = current->next;
                        }
                        --_size;
                        return iterator(this, idx, current->next);
                    }
                    prev = current;
                    current = current->next;
                }
            }
            return end();
        }

        iterator erase(const_iterator pos) {
            return erase(iterator(this, pos._bucket_index, pos._node));
        }

        iterator erase(const_iterator first, const_iterator last) {
            while(first != last) {
                first = erase(first);
            }
            return iterator(this, last._bucket_index, last._node);
        }

        size_t erase(const Key& key) {
            if (_buckets.size()) {
                size_t idx = _bucket_index(key);
                NodePtr current = _buckets[idx];
                NodePtr prev;
                while (current) {
                    if (_equal(current->p.first, key)) {
                        if (prev) {
                            prev->next = current->next;
                        } else {
                            _buckets[idx] = current->next;
                        }
                        --_size;
                        return 1;
                    }
                    prev = current;
                    current = current->next;
                }
            }
            return 0;
        }

        template<class K> requires detail::HashableWith<K, Key, Hash, KeyEqual>
        size_t erase(K&& x) {
            if (_buckets.size()) {
                size_t idx = _hash(x) % _buckets.size();
                NodePtr current = _buckets[idx];
                NodePtr prev;
                while (current) {
                    if (_equal(current->p.first, x)) {
                        if (prev) {
                            prev->next = current->next;
                        } else {
                            _buckets[idx] = current->next;
                        }
                        --_size;
                        return 1;
                    }
                    prev = current;
                    current = current->next;
                }
            }
            return 0;
        }

        iterator find(const Key& key) noexcept {
            if (_buckets.size()) {
                size_t idx = _bucket_index(key);
                NodePtr current = _buckets[idx];
                while (current) {
                    if (_equal(current->p.first, key)) {
                        return iterator(this, idx, current);
                    }
                    current = current->next;
                }
            }
            return end();
        }

        template<class K> requires detail::HashableWith<K, Key, Hash, KeyEqual>
        iterator find(const K& x) noexcept {
            if (_buckets.size()) {
                size_t idx = _hash(x) % _buckets.size();
                NodePtr current = _buckets[idx];
                while (current) {
                    if (_equal(current->p.first, x)) {
                        return iterator(this, idx, current);
                    }
                    current = current->next;
                }
            }
            return end();
        }

        const_iterator find(const Key& key) const noexcept {
            if (_buckets.size()) {
                size_t idx = _bucket_index(key);
                NodePtr current = _buckets[idx];
                while (current) {
                    if (_equal(current->p.first, key)) {
                        return const_iterator(this, idx, current);
                    }
                    current = current->next;
                }
            }
            return end();
        }

        template<class K> requires detail::HashableWith<K, Key, Hash, KeyEqual>
        const_iterator find(const K& x) const noexcept {
            if (_buckets.size()) {
                size_t idx = _hash(x) % _buckets.size();
                NodePtr current = _buckets[idx];
                while (current) {
                    if (_equal(current->p.first, x)) {
                        return const_iterator(this, idx, current);
                    }
                    current = current->next;
                }
            }
            return end();
        }

        size_t count(const Key& key) const noexcept {
            return find(key) != end();
        }

        template<class K> requires detail::HashableWith<K, Key, Hash, KeyEqual>
        size_t count(const K& x) const noexcept {
            return find(x) != end();
        }

        bool contains(const Key& key) const noexcept {
            if (_buckets.size()) {
                size_t idx = _bucket_index(key);
                NodePtr current = _buckets[idx];
                while (current) {
                    if (_equal(current->p.first, key)) {
                        return true;
                    }
                    current = current->next;
                }
            }
            return false;
        }

        template<class K> requires detail::HashableWith<K, Key, Hash, KeyEqual>
        bool contains(const K& x) const noexcept {
            return find(x) != end();
        }

        std::pair<iterator, iterator> equal_range(const Key& key) noexcept {
            iterator it = find(key);
            return it == end() ? std::make_pair(it, it)
                               : std::make_pair(it, std::next(it));
        }

        std::pair<const_iterator, const_iterator> equal_range(const Key& key) const noexcept {
            const_iterator it = find(key);
            return it == end() ? std::make_pair(it, it)
                               : std::make_pair(it, std::next(it));
        }

        template <typename K> requires detail::HashableWith<K, Key, Hash, KeyEqual>
        std::pair<iterator, iterator> equal_range(const K& x) noexcept {
            iterator it = find(x);
            return it == end() ? std::make_pair(it, it)
                               : std::make_pair(it, std::next(it));
        }

        template <typename K> requires detail::HashableWith<K, Key, Hash, KeyEqual>
        std::pair<const_iterator, const_iterator> equal_range(const K& x) const noexcept {
            const_iterator it = find(x);
            return it == end() ? std::make_pair(it, it)
                               : std::make_pair(it, std::next(it));
        }

        node_type extract(const Key& key) noexcept {
            if (_buckets.size()) {
                size_t idx = _bucket_index(key);
                NodePtr current = _buckets[idx];
                NodePtr prev;
                while (current) {
                    if (_equal(current->p.first, key)) {
                        if (prev) {
                            prev->next = current->next;
                        } else {
                            _buckets[idx] = current->next;
                        }
                        --_size;
                        current->next = nullptr;
                        return node_type(current);
                    }
                    prev = current;
                    current = current->next;
                }
            }
            return {};
        }

        node_type extract(const_iterator position) {
            if (position != end() && _buckets.size()) {
                size_t idx = position._bucket_index;
                NodePtr current = _buckets[idx];
                NodePtr prev;
                while (current) {
                    if (current == position._node) {
                        if (prev) {
                            prev->next = current->next;
                        } else {
                            _buckets[idx] = current->next;
                        }
                        --_size;
                        current->next = nullptr;
                        return node_type(current);
                    }
                    prev = current;
                    current = current->next;
                }
            }
            return {};
        }

        template<class H2, class P2>
        void merge(unordered_map<Key, T, H2, P2>& source) {
            for (auto it = source.begin(); it != source.end(); ) {
                const Key& k = it->first;
                if (!contains(k)) {
                    emplace(Key(k), std::move(it->second));
                    it = source.erase(it);
                } else {
                    ++it;
                }
            }
        }

        template<class H2, class P2>
        void merge(unordered_map<Key, T, H2, P2>&& source) {
            for (auto it = source.begin(); it != source.end(); ) {
                Key&& k = std::move(const_cast<Key&>(it->first));
                if (!contains(k)) {
                    emplace(std::move(k), std::move(it->second));
                    it = source.erase(it);
                } else {
                    ++it;
                }
            }
        }

        iterator begin() noexcept {
            for (size_t i = 0; i < _buckets.size(); ++i) {
                if (_buckets[i])
                    return iterator(this, i, _buckets[i]);
            }
            return end();
        }

        iterator end() noexcept {
            return iterator(this, _buckets.size(), nullptr);
        }

        const_iterator begin() const noexcept {
            for (size_t i = 0; i < _buckets.size(); ++i)
                if (_buckets[i]) {
                    return const_iterator(this, i, _buckets[i]);
                }
            return end();
        }

        const_iterator end() const noexcept {
            return const_iterator(this, _buckets.size(), nullptr);
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        const_iterator cend() const noexcept {
            return end();
        }

        size_t bucket_count() const noexcept {
            return _buckets.size();
        }

        size_type max_bucket_count() const noexcept {
            return _buckets.max_size();
        }

        size_type bucket_size(size_type n) const noexcept {
            size_type count = 0;
            if (n < _buckets.size()) {
                NodePtr current = _buckets[n];
                while (current) {
                    ++count;
                    current = current->next;
                }
            }
            return count;
        }

        size_type bucket(const Key& key) const {
            return _buckets.size() ? _bucket_index(key) : 0;
        }

        float load_factor() const noexcept {
            return _buckets.size() ? (float)size() / _buckets.size() : 1;
        }

        float max_load_factor() const noexcept {
            return _max_load_factor;
        }

        void max_load_factor(float factor) noexcept {
            _max_load_factor = factor;
        }

        size_type max_size() const noexcept {
            return std::numeric_limits<difference_type>::max();
        }

        void clear() noexcept {
            unordered_map().swap(*this);
        }

        size_t size() const noexcept {
            return _size;
        }

        bool empty() const noexcept {
            return size() == 0;
        }

        void rehash(size_t count) {
            if (count > std::ceil(size() / max_load_factor())) {
                _rehash(count);
            }
        }

        void reserve(size_type count) {
            rehash(std::ceil(count / max_load_factor()));
        }

        void shrink_to_fit() {
            if (_buckets.size()) {
                size_t optimal = static_cast<size_t>(std::ceil(size() / _max_load_factor));
                optimal = std::max(optimal, MinBucketCount);
                if (optimal < _buckets.size()) {
                    _rehash(optimal);
                }
            }
        }

        void swap(unordered_map& other) noexcept {
            using std::swap;
            swap(_buckets, other._buckets);
            swap(_hash, other._hash);
            swap(_equal, other._equal);
            swap(_max_load_factor, other._max_load_factor);
            swap(_size, other._size);
        }

        hasher hash_function() const noexcept {
            return _hash;
        }

        key_equal key_eq() const noexcept {
            return _equal;
        }

    private:
        sgcl::vector<NodePtr> _buckets;
        Hash _hash;
        KeyEqual _equal;
        float _max_load_factor = {0.75f};
        size_t _size = {0};

        size_t _bucket_index(const Key& key) const noexcept {
            return _hash(key) % _buckets.size();
        }

        void _rehash(size_t count) {
            decltype(_buckets) buckets(count);
            for (auto head : _buckets) {
                NodePtr current = head;
                while (current) {
                    NodePtr next = current->next;
                    size_t new_idx = _hash(current->p.first) % count;
                    current->next = buckets[new_idx];
                    buckets[new_idx] = current;
                    current = next;
                }
            }
            _buckets = std::move(buckets);
        }

        void _ensure_capacity() {
            if (_buckets.empty()) {
                _rehash(MinBucketCount);
                return;
            }
            size_t required_bucket_count = static_cast<size_t>(std::ceil(_size / _max_load_factor));
            if (required_bucket_count > _buckets.size()) {
                required_bucket_count = std::max(required_bucket_count, _buckets.size() * 2);
                _rehash(required_bucket_count);
            }
        }
    };

    template<class Key, class T, class Hash, class KeyEqual>
    class unordered_map<Key, unique_ptr<T>, Hash, KeyEqual> : public std::unordered_map<Key, unique_ptr<T>, Hash, KeyEqual> {
    public:
        using std::unordered_map<Key, unique_ptr<T>, Hash, KeyEqual>::unordered_map;
    };
}
