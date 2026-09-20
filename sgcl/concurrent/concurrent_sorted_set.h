//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/skip_list.h"

#include <functional>

namespace sgcl {
    // A lock-free ordered set shared by any number of threads: the skip
    // list of concurrent_sorted_map (concurrent_sorted_map.h has the account of the
    // algorithm and the rules) with the key as the element. find,
    // contains, lower_bound and upper_bound are wait-free; insert,
    // emplace and erase lock-free and linearizable; the iteration in key
    // order and weakly consistent, an iterator holding its node. The
    // elements are const, as in std::set.
    template<class Key, class Compare = std::less<Key>>
    class concurrent_sorted_set : public detail::SkipList<detail::ConcurrentSortedSetTraits<Key, Compare>> {
        using Base = detail::SkipList<detail::ConcurrentSortedSetTraits<Key, Compare>>;

    public:
        using typename Base::value_type;
        using typename Base::iterator;

        using Base::Base;
        using Base::insert;

        concurrent_sorted_set() = default;

        // insert(key) searches once and builds the node only when the key
        // is absent, as try_emplace does for the map
        pair<iterator, bool> insert(const Key& key) {
            return this->_insert_absent(key, [&](unsigned h) { return this->_make_node(h, key); });
        }

        pair<iterator, bool> insert(Key&& key) {
            return this->_insert_absent(key, [&](unsigned h) { return this->_make_node(h, std::move(key)); });
        }
    };
}
