//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/weak_iterator.h"

namespace sgcl {
    // A map from objects to values that does not keep its objects alive:
    // the key is the object itself (its identity, not its contents), held
    // by a weak pointer, and an entry whose object is gone is dead: never
    // found, passed over by the iteration, dropped by a sweep. Metadata
    // attached to objects from outside, a cache keyed by the object, a
    // registry that forgets. Hashed and compared by the object's address,
    // which the weak pointer's cell holds while the object lives and the
    // collector clears before the address can be given out again
    // (detail/weak_table.h). The values are the map's, destroyed with
    // the entry; a value holding a strong pointer to its own key keeps
    // the key alive, and the entry with it. The sweeps run every so many
    // insertions, as many as the map has entries, and on sweep(); size()
    // counts the entries a sweep has not yet dropped. The map lives where
    // a tracked_ptr may: on a stack or in a managed object. weak_multimap
    // holds several values per object.
    template<class Key, class T>
    class weak_map : public detail::WeakTable<Key, unordered_map<weak_ptr<Key>, T, detail::WeakHash<Key>, detail::WeakEqual<Key>>> {
        using Base = detail::WeakTable<Key, unordered_map<weak_ptr<Key>, T, detail::WeakHash<Key>, detail::WeakEqual<Key>>>;
        using Table = typename Base::table_type;
        using Base::_table;

    public:
        using key_type = Key;
        using key_pointer = tracked_ptr<Key>;
        using mapped_type = T;
        using size_type = size_t;

        // What an iterator gives out: the object, held, and its value
        struct reference {
            key_pointer key;
            T& value;
            static reference of(const key_pointer& object, typename Table::value_type& entry) noexcept {
                return {object, entry.second};
            }
        };
        using iterator = detail::WeakIterator<typename Table::iterator, Key, reference>;

        weak_map() = default;

        iterator begin() noexcept {
            return iterator(_table.begin(), _table.end());
        }

        iterator end() noexcept {
            return iterator(_table.end(), _table.end());
        }

        // The entry of the object, or end(); a null pointer has none
        iterator find(const key_pointer& object) {
            return object ? iterator(_table.find(object), _table.end()) : end();
        }

        // The value of the object, made if the object has none; a null
        // pointer is not an object
        T& operator[](const key_pointer& object) {
            assert(object && "a weak_map has no entry for a null pointer");
            auto it = _table.find(object);
            if (it == _table.end()) {
                it = _table.emplace(weak_ptr<Key>(object), T()).first;
                this->_inserted_one();
            }
            return it->second;
        }

        // A value for the object, unless it has one; whether one was added
        template<class... A>
        std::pair<iterator, bool> emplace(const key_pointer& object, A&&... a) {
            assert(object && "a weak_map has no entry for a null pointer");
            auto it = _table.find(object);
            if (it != _table.end()) {
                return {iterator(it, _table.end()), false};
            }
            it = _table.emplace(weak_ptr<Key>(object), T(std::forward<A>(a)...)).first;
            this->_inserted_one();
            return {iterator(it, _table.end()), true};
        }

        std::pair<iterator, bool> insert(const key_pointer& object, const T& value) {
            return emplace(object, value);
        }

        std::pair<iterator, bool> insert(const key_pointer& object, T&& value) {
            return emplace(object, std::move(value));
        }

        // The value for the object, replaced if it has one
        template<class V>
        std::pair<iterator, bool> insert_or_assign(const key_pointer& object, V&& value) {
            auto [it, inserted] = emplace(object, std::forward<V>(value));
            if (!inserted) {
                it->value = std::forward<V>(value);
            }
            return {it, inserted};
        }

        iterator erase(iterator pos) {
            return iterator(_table.erase(pos.inner()), _table.end());
        }

        using Base::erase;
    };

    template<class Key, class T>
    class weak_multimap : public detail::WeakTable<Key, unordered_multimap<weak_ptr<Key>, T, detail::WeakHash<Key>, detail::WeakEqual<Key>>> {
        using Base = detail::WeakTable<Key, unordered_multimap<weak_ptr<Key>, T, detail::WeakHash<Key>, detail::WeakEqual<Key>>>;
        using Table = typename Base::table_type;
        using Base::_table;

    public:
        using key_type = Key;
        using key_pointer = tracked_ptr<Key>;
        using mapped_type = T;
        using size_type = size_t;

        struct reference {
            key_pointer key;
            T& value;
            static reference of(const key_pointer& object, typename Table::value_type& entry) noexcept {
                return {object, entry.second};
            }
        };
        using iterator = detail::WeakIterator<typename Table::iterator, Key, reference>;

        weak_multimap() = default;

        iterator begin() noexcept {
            return iterator(_table.begin(), _table.end());
        }

        iterator end() noexcept {
            return iterator(_table.end(), _table.end());
        }

        // The first entry of the object, or end()
        iterator find(const key_pointer& object) {
            return object ? iterator(_table.find(object), _table.end()) : end();
        }

        // The entries of the object: a range of its own, [first, last)
        std::pair<iterator, iterator> equal_range(const key_pointer& object) {
            if (!object) {
                return {end(), end()};
            }
            auto [first, last] = _table.equal_range(object);
            return {iterator(first, last), iterator(last, last)};
        }

        // One more value for the object
        template<class... A>
        iterator emplace(const key_pointer& object, A&&... a) {
            assert(object && "a weak_multimap has no entry for a null pointer");
            auto it = _table.emplace(weak_ptr<Key>(object), T(std::forward<A>(a)...));
            this->_inserted_one();
            return iterator(it, _table.end());
        }

        iterator insert(const key_pointer& object, const T& value) {
            return emplace(object, value);
        }

        iterator insert(const key_pointer& object, T&& value) {
            return emplace(object, std::move(value));
        }

        iterator erase(iterator pos) {
            return iterator(_table.erase(pos.inner()), _table.end());
        }

        using Base::erase;
    };
}
