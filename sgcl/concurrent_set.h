//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/skip_list.h"

#include <functional>

namespace sgcl {
    // A lock-free ordered set shared by any number of threads: the skip
    // list of concurrent_map (concurrent_map.h has the account of the
    // algorithm and the rules) with the key as the element. find,
    // contains, lower_bound and upper_bound are wait-free; insert,
    // emplace and erase lock-free and linearizable; the iteration in key
    // order and weakly consistent, an iterator holding its node. The
    // elements are const, as in std::set.
    template<class Key, class Compare = std::less<Key>>
    class concurrent_set : public detail::SkipList<detail::ConcurrentSetTraits<Key, Compare>> {
        using Base = detail::SkipList<detail::ConcurrentSetTraits<Key, Compare>>;

    public:
        using typename Base::value_type;
        using typename Base::iterator;

        using Base::Base;
        using Base::insert;

        concurrent_set() = default;

        // insert(key) looks the key up first and builds nothing when it is
        // there, as try_emplace does for the map
        pair<iterator, bool> insert(const Key& key) {
            if (auto it = this->find(key); it != this->end()) {
                return {it, false};
            }
            unsigned h = this->_height();
            return this->_insert(this->_make_node(h, key), h);
        }

        pair<iterator, bool> insert(Key&& key) {
            if (auto it = this->find(key); it != this->end()) {
                return {it, false};
            }
            unsigned h = this->_height();
            return this->_insert(this->_make_node(h, std::move(key)), h);
        }
    };
}
