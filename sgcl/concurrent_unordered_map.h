//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/split_list.h"

#include <functional>

namespace sgcl {
    // A lock-free hash map shared by any number of threads: the
    // split-ordered list of Shalev and Shavit (2006), the structure that
    // makes a resizable lock-free hash table possible, and the one whose
    // resize is the reason for a collector. Every element sits in one
    // sorted singly linked list (Harris and Michael's, with marker nodes
    // for the deletions as in concurrent_map), ordered by the bit
    // reversal of its hash; an array of buckets points into that list at
    // dummy nodes, one per bucket, made on the bucket's first use and
    // inserted after the parent bucket's dummy (the bucket's number with
    // its highest bit cleared). In split order the elements of bucket i
    // of an array of n follow its dummy and precede the dummy of the
    // bucket that i splits into at 2n, so doubling the array moves no
    // node: the new array gets the old slots copied and the rest made on
    // use, the list stays what it was, and the old array is garbage once
    // nothing walks it. In C++ without a collector that old array, and
    // every node a walk may be standing on, is exactly what a reclamation
    // scheme has to guard; here it is nothing.
    //
    // find, contains and count are wait-free and never write; insert,
    // emplace, try_emplace and erase are lock-free and linearizable (at
    // the compare-exchange that links the node, and at the one that
    // marks it). The count of the elements is striped over cache lines,
    // as Java's LongAdder; size() is its sum, a snapshot of no particular
    // moment under concurrent modification, and the array doubles once
    // the elements outnumber the buckets. The iterators are weakly
    // consistent, as Java's: an iterator holds its node, is valid
    // whatever the other threads do, skips the elements erased since it
    // passed them and may or may not see the ones inserted meanwhile; the
    // order is the list's, the bit reversal of the hashes. An element is
    // destroyed by the collector with its node, once nothing holds it:
    // not at the erase, which other threads may be reading it across.
    // The container holds its bucket array, its head node and its
    // counters by tracked_ptrs, so it lives where one may (on a stack or
    // inside a managed object).
    // No operator[], at, insert_or_assign, node handles or local iteration
    // of a bucket.
    template<class Key, class V, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class concurrent_unordered_map : public detail::SplitList<detail::ConcurrentUnorderedMapTraits<Key, V, Hash, KeyEqual>> {
        using Base = detail::SplitList<detail::ConcurrentUnorderedMapTraits<Key, V, Hash, KeyEqual>>;

    public:
        using mapped_type = V;
        using typename Base::value_type;
        using typename Base::iterator;

        using Base::Base;
        using Base::insert;

        concurrent_unordered_map() = default;

        // try_emplace looks the key up first and builds nothing when it is
        // there, then builds the element from the key and the arguments
        template<class... A>
        pair<iterator, bool> try_emplace(const Key& key, A&&... a) {
            if (auto it = this->find(key); it != this->end()) {
                return {it, false};
            }
            return this->_insert(this->_make_node(std::piecewise_construct, forward_as_tuple(key), forward_as_tuple(std::forward<A>(a)...)), this->_hash(key));
        }

        template<class... A>
        pair<iterator, bool> try_emplace(Key&& key, A&&... a) {
            if (auto it = this->find(key); it != this->end()) {
                return {it, false};
            }
            size_t hash = this->_hash(key);
            return this->_insert(this->_make_node(std::piecewise_construct, forward_as_tuple(std::move(key)), forward_as_tuple(std::forward<A>(a)...)), hash);
        }

        template<class P> requires std::is_constructible_v<value_type, P&&>
        pair<iterator, bool> insert(P&& value) {
            return this->emplace(std::forward<P>(value));
        }
    };
}
