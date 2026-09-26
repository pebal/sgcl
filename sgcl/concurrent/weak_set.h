//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/weak_iterator.h"
#include "set.h"
#include "detail/concurrent_weak_table.h"

#include <cassert>

namespace sgcl::concurrent {
    namespace detail { using namespace sgcl::detail; }
    // The weak_set shared by any number of threads: a set of objects that
    // does not keep them alive, the weak_map of nothing but
    // keys (weak_map.h has the rules), over the lock-free hash
    // table of set. Objects registered from several
    // threads without being owned there: the sessions, the listeners,
    // the instances of a class, a set that forgets. The iteration gives
    // out the live objects, held while the iterator stands on them.
    template<class Key>
    class weak_set
    : public detail::ConcurrentWeakTable<Key, set<detail::WeakKey<Key>, detail::ConcurrentWeakHash<Key>, detail::ConcurrentWeakEqual<Key>>> {
        using Base = detail::ConcurrentWeakTable<Key, set<detail::WeakKey<Key>, detail::ConcurrentWeakHash<Key>, detail::ConcurrentWeakEqual<Key>>>;
        using Table = typename Base::table_type;
        using Base::_table;

    public:
        using key_type = Key;
        using key_pointer = tracked_ptr<Key>;
        using size_type = size_t;

        // What an iterator gives out: the object, held
        using reference = key_pointer;
        using iterator = detail::WeakIterator<typename Table::iterator, Key, reference>;

        weak_set() = default;

        iterator begin() noexcept {
            return iterator(_table.begin(), _table.end());
        }

        iterator end() noexcept {
            return iterator(_table.end(), _table.end());
        }

        iterator find(const key_pointer& object) noexcept {
            return object ? iterator(_table.find(object), _table.end()) : end();
        }

        // The object added, unless it is in the set: its entry and
        // whether it was added; of two threads adding the same object
        // exactly one gets true. A null pointer is not an object.
        pair<iterator, bool> insert(const key_pointer& object) {
            assert(object && "a weak_set has no entry for a null pointer");
            auto [it, inserted] = _table._insert_absent(object, [&] {   // one search: the node, with its weak cell, made only when the object has no entry
                return _table._make_node(this->_key(object));
            });
            if (inserted) {
                this->_inserted_one();
            }
            return {iterator(it, _table.end()), inserted};
        }

        iterator erase(iterator pos) {
            return iterator(_table.erase(pos.inner()), _table.end());
        }

        using Base::erase;
    };
}
