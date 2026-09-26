//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/weak_iterator.h"
#include "map.h"
#include "detail/concurrent_weak_table.h"

#include <cassert>
#include <tuple>
#include <utility>

namespace sgcl::concurrent {
    namespace detail { using namespace sgcl::detail; }
    // The weak_map shared by any number of threads: a map from objects to
    // values that does not keep its objects alive (weak_map.h has the
    // account: the key is the object itself, its identity, held by a
    // weak pointer; an entry whose object is gone is dead, never found,
    // passed over by the iteration, dropped by a sweep), over the
    // lock-free hash table of map
    // (map.h has the rules of the table). Metadata
    // attached to objects from several threads, a cache keyed by the
    // object that the workers share, a registry that forgets.
    // find, contains and count are wait-free and never write; insert,
    // emplace, try_emplace and erase are lock-free and linearizable at
    // the compare-exchange of the table; the iteration is weakly
    // consistent, an iterator holding its node and, on a live entry,
    // the object. The dead entries are swept by the inserting thread
    // every so many insertions, as many as the map has entries, one
    // sweep at a time, and on sweep(); size() is the table's count, the
    // dead entries not yet swept included (detail/concurrent_weak_table.h
    // has who sweeps and why it is safe). The values are the map's own,
    // destroyed with the node by the collector once nothing holds it: a
    // value holding a strong pointer to its own key keeps the key alive,
    // and the entry with it. No operator[] and no insert_or_assign, as
    // the table has none: a value is set once, at the insertion, and
    // changed through an atomic inside it. The map lives where a
    // tracked_ptr may: on a stack or inside a managed object; the one a
    // program shares goes into a managed object under a root_ptr.
    template<class Key, class T>
    class weak_map
    : public detail::ConcurrentWeakTable<Key, map<detail::WeakKey<Key>, T, detail::ConcurrentWeakHash<Key>, detail::ConcurrentWeakEqual<Key>>> {
        using Base = detail::ConcurrentWeakTable<Key, map<detail::WeakKey<Key>, T, detail::ConcurrentWeakHash<Key>, detail::ConcurrentWeakEqual<Key>>>;
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

        // The live entries, each once, in the order of the table's list;
        // weakly consistent
        iterator begin() noexcept {
            return iterator(_table.begin(), _table.end());
        }

        iterator end() noexcept {
            return iterator(_table.end(), _table.end());
        }

        // The entry of the object, or end(); a null pointer has none.
        // Wait-free; the iterator holds the node and the object.
        iterator find(const key_pointer& object) noexcept {
            return object ? iterator(_table.find(object), _table.end()) : end();
        }

        // A value for the object, T(a...), unless it has one: the entry
        // and whether it was added. The key is looked up first and
        // nothing is built when it is there; a concurrent insertion of
        // the same object wins or loses at the table's compare-exchange,
        // exactly one returns true. A null pointer is not an object.
        template<class... A>
        pair<iterator, bool> try_emplace(const key_pointer& object, A&&... a) {
            assert(object && "a weak_map has no entry for a null pointer");
            auto [it, inserted] = _table._insert_absent(object, [&] {   // one search: the node, with its weak cell, made only when the object has no entry
                return _table._make_node(std::piecewise_construct, std::forward_as_tuple(this->_key(object)), std::forward_as_tuple(std::forward<A>(a)...));
            });
            if (inserted) {
                this->_inserted_one();
            }
            return {iterator(it, _table.end()), inserted};
        }

        template<class... A>
        pair<iterator, bool> emplace(const key_pointer& object, A&&... a) {
            return try_emplace(object, std::forward<A>(a)...);
        }

        pair<iterator, bool> insert(const key_pointer& object, const T& value) {
            return try_emplace(object, value);
        }

        pair<iterator, bool> insert(const key_pointer& object, T&& value) {
            return try_emplace(object, std::move(value));
        }

        // The entry the iterator stands on, erased if it is still there:
        // the next live entry
        iterator erase(iterator pos) {
            return iterator(_table.erase(pos.inner()), _table.end());
        }

        using Base::erase;
    };
}
