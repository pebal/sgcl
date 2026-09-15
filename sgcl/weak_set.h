//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/weak_iterator.h"

namespace sgcl {
    // A set of objects that does not keep them alive: the weak_map of
    // nothing but keys (weak_map.h). Objects registered somewhere without
    // being owned there, a set that forgets. The iteration gives out the
    // live objects, held while the iterator stands on them.
    template<class Key, template<class> class Ptr = tracked_ptr>
    class weak_set : public detail::WeakTable<Key, unordered_set<weak_ptr<Key, Ptr>, detail::WeakHash<Key, Ptr>, detail::WeakEqual<Key, Ptr>, Ptr>, Ptr> {
        using Base = detail::WeakTable<Key, unordered_set<weak_ptr<Key, Ptr>, detail::WeakHash<Key, Ptr>, detail::WeakEqual<Key, Ptr>, Ptr>, Ptr>;
        using Table = typename Base::table_type;
        using Base::_table;

    public:
        using key_type = Key;
        using key_pointer = Ptr<Key>;
        using size_type = size_t;

        // What an iterator gives out: the object, held
        using reference = key_pointer;
        using iterator = detail::WeakIterator<typename Table::iterator, Key, Ptr, reference>;

        weak_set() = default;

        iterator begin() noexcept {
            return iterator(_table.begin(), _table.end());
        }

        iterator end() noexcept {
            return iterator(_table.end(), _table.end());
        }

        iterator find(const key_pointer& object) {
            return object ? iterator(_table.find(object), _table.end()) : end();
        }

        // The object added, unless it is in the set; whether it was added
        std::pair<iterator, bool> insert(const key_pointer& object) {
            assert(object && "a weak_set has no entry for a null pointer");
            auto it = _table.find(object);
            if (it != _table.end()) {
                return {iterator(it, _table.end()), false};
            }
            it = _table.emplace(weak_ptr<Key, Ptr>(object)).first;
            this->_inserted_one();
            return {iterator(it, _table.end()), true};
        }

        iterator erase(iterator pos) {
            return iterator(_table.erase(pos.inner()), _table.end());
        }

        using Base::erase;
    };
}
