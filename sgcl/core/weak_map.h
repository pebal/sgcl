//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "detail/weak_iterator.h"

namespace sgcl {
    namespace detail {
        // What an iterator of a weak map gives out: the object, held, and
        // its value (const through a const_iterator)
        template<class Key, class V>
        struct WeakMapReference {
            tracked_ptr<Key> key;
            V& value;

            template<class Entry>
            static WeakMapReference of(const tracked_ptr<Key>& object, Entry& entry) noexcept {
                return {object, entry.second};
            }
        };
    }

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
    class weak_map
    : public detail::WeakTable<Key, map<weak_ptr<Key>, T, detail::WeakHash<Key>, detail::WeakEqual<Key>>> {
        using Base = detail::WeakTable<Key, map<weak_ptr<Key>, T, detail::WeakHash<Key>, detail::WeakEqual<Key>>>;
        using Table = typename Base::table_type;
        using Base::_table;

    public:
        using key_type = Key;
        using key_pointer = tracked_ptr<Key>;
        using mapped_type = T;
        using size_type = size_t;

        // What an iterator gives out: the object, held, and its value
        using reference = detail::WeakMapReference<Key, T>;
        using iterator = detail::WeakIterator<typename Table::iterator, Key, reference>;
        using const_reference = detail::WeakMapReference<Key, const T>;
        using const_iterator = detail::WeakIterator<typename Table::const_iterator, Key, const_reference>;

        weak_map() = default;

        iterator begin() noexcept {
            return iterator(_table.begin(), _table.end());
        }

        iterator end() noexcept {
            return iterator(_table.end(), _table.end());
        }

        const_iterator begin() const noexcept {
            return const_iterator(_table.begin(), _table.end());
        }

        const_iterator end() const noexcept {
            return const_iterator(_table.end(), _table.end());
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        const_iterator cend() const noexcept {
            return end();
        }

        // The entry of the object, or end(); a null pointer has none
        iterator find(const key_pointer& object) {
            return object ? iterator(_table.find(object), _table.end()) : end();
        }

        const_iterator find(const key_pointer& object) const {
            return object ? const_iterator(_table.find(object), _table.end()) : end();
        }

        // The value of the object, made if the object has none; a null
        // pointer is not an object. One search: the entry is made in place
        // from the pointer when the search finds none
        T& operator[](const key_pointer& object) {
            assert(object && "a weak_map has no entry for a null pointer");
            auto [node, inserted] = _table._try_emplace(object);
            if (inserted) {
                this->_inserted_one();
            }
            return node->slot.value.second;
        }

        // A value for the object, unless it has one; whether one was added.
        // The value is built in place, and not at all when the object has
        // one
        template<class... A>
        pair<iterator, bool> emplace(const key_pointer& object, A&&... a) {
            assert(object && "a weak_map has no entry for a null pointer");
            auto [node, inserted] = _table._try_emplace(object, std::forward<A>(a)...);
            if (inserted) {
                this->_inserted_one();
            }
            return {iterator(_table._make_iterator(node), _table.end()), inserted};
        }

        pair<iterator, bool> insert(const key_pointer& object, const T& value) {
            return emplace(object, value);
        }

        pair<iterator, bool> insert(const key_pointer& object, T&& value) {
            return emplace(object, std::move(value));
        }

        // The value for the object, replaced if it has one
        template<class V>
        pair<iterator, bool> insert_or_assign(const key_pointer& object, V&& value) {
            auto [it, inserted] = emplace(object, std::forward<V>(value));
            if (!inserted) {
                it->value = std::forward<V>(value);
            }
            return {it, inserted};
        }

        // The next live entry up to the iterator's own bound
        iterator erase(iterator pos) {
            return iterator(_table.erase(pos.inner()), pos.bound());
        }

        using Base::erase;
    };

    template<class Key, class T>
    class weak_multimap
    : public detail::WeakTable<Key, multimap<weak_ptr<Key>, T, detail::WeakHash<Key>, detail::WeakEqual<Key>>> {
        using Base = detail::WeakTable<Key, multimap<weak_ptr<Key>, T, detail::WeakHash<Key>, detail::WeakEqual<Key>>>;
        using Table = typename Base::table_type;
        using Base::_table;

    public:
        using key_type = Key;
        using key_pointer = tracked_ptr<Key>;
        using mapped_type = T;
        using size_type = size_t;

        using reference = detail::WeakMapReference<Key, T>;
        using iterator = detail::WeakIterator<typename Table::iterator, Key, reference>;
        using const_reference = detail::WeakMapReference<Key, const T>;
        using const_iterator = detail::WeakIterator<typename Table::const_iterator, Key, const_reference>;

        weak_multimap() = default;

        iterator begin() noexcept {
            return iterator(_table.begin(), _table.end());
        }

        iterator end() noexcept {
            return iterator(_table.end(), _table.end());
        }

        const_iterator begin() const noexcept {
            return const_iterator(_table.begin(), _table.end());
        }

        const_iterator end() const noexcept {
            return const_iterator(_table.end(), _table.end());
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        const_iterator cend() const noexcept {
            return end();
        }

        // The first entry of the object, or end()
        iterator find(const key_pointer& object) {
            return object ? iterator(_table.find(object), _table.end()) : end();
        }

        const_iterator find(const key_pointer& object) const {
            return object ? const_iterator(_table.find(object), _table.end()) : end();
        }

        // The entries of the object: a range of its own, [first, last)
        pair<iterator, iterator> equal_range(const key_pointer& object) {
            if (!object) {
                return {end(), end()};
            }
            auto [first, last] = _table.equal_range(object);
            return {iterator(first, last), iterator(last, last)};
        }

        // One more value for the object, built in place
        template<class... A>
        iterator emplace(const key_pointer& object, A&&... a) {
            assert(object && "a weak_multimap has no entry for a null pointer");
            auto it = _table.emplace(std::piecewise_construct, forward_as_tuple(object), forward_as_tuple(std::forward<A>(a)...));
            this->_inserted_one();
            return iterator(it, _table.end());
        }

        iterator insert(const key_pointer& object, const T& value) {
            return emplace(object, value);
        }

        iterator insert(const key_pointer& object, T&& value) {
            return emplace(object, std::move(value));
        }

        // The next live entry up to the iterator's own bound: an erase
        // through an equal_range stops where the range does, a dead entry
        // there included
        iterator erase(iterator pos) {
            return iterator(_table.erase(pos.inner()), pos.bound());
        }

        using Base::erase;
    };
}
