//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/split_list.h"

#include <functional>

namespace sgcl {
    // A lock-free hash set shared by any number of threads: the
    // split-ordered list of concurrent_map
    // (concurrent_map.h has the account of the algorithm and the
    // rules) with the key as the element. find, contains and count are
    // wait-free; insert, emplace and erase lock-free and linearizable; the
    // iteration weakly consistent, an iterator holding its node. The
    // elements are const, as in std::unordered_set.
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class concurrent_set : public detail::SplitList<detail::ConcurrentSetTraits<Key, Hash, KeyEqual>> {
        using Base = detail::SplitList<detail::ConcurrentSetTraits<Key, Hash, KeyEqual>>;

    public:
        using typename Base::value_type;
        using typename Base::iterator;

        using Base::Base;
        using Base::insert;

        concurrent_set() = default;

        // insert(key) looks the key up first and builds nothing when it is
        // there
        pair<iterator, bool> insert(const Key& key) {
            return this->_insert_absent(key, [&] { return this->_make_node(key); });
        }

        pair<iterator, bool> insert(Key&& key) {
            return this->_insert_absent(key, [&] { return this->_make_node(std::move(key)); });
        }
    };
}
