//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/skip_list.h"

#include <functional>

namespace sgcl::concurrent {
    namespace detail { using namespace sgcl::detail; }
    // A lock-free ordered map shared by any number of threads: a skip list
    // with the algorithm of Herlihy and Shavit (The Art of Multiprocessor
    // Programming, the LockFreeSkipList), the structure Java's
    // ConcurrentSkipListMap is. The bottom level is a sorted singly linked
    // list holding every element; each node also stands in a random
    // number of the levels above, a quarter of the nodes of one level in
    // the next, so that a search descends from the top level in
    // logarithmic time and finishes on the bottom list, which alone
    // decides what the map holds. What the collector does for the
    // algorithm is what it does for the queue and the stack: no node is
    // reused while a thread holds it, so there is no ABA and no
    // reclamation scheme, and a logical deletion is a marker node linked
    // after the deleted one, at each of its levels (Java's way of marking
    // without a bit stolen from the pointer, which the collector's pointer
    // maps could not follow); a marker is an allocation of one word, and
    // a marked node is unlinked by the next search that passes it.
    //
    // A node is one managed object: the bottom link and the element,
    // then its links of the upper levels, as many as its height, so that a
    // search steps through one object per node at any level. The
    // container holds the head node (a sentinel of the maximum height and
    // no element) and the number of levels in use; it lives where a
    // tracked_ptr may (on a stack or inside a managed object).
    //
    // insert, try_emplace, emplace and erase are lock-free and
    // linearizable: an insertion takes effect at the compare-exchange that
    // links the node into the bottom list, an erasure at the one that
    // marks it there; find, contains, lower_bound and upper_bound are
    // wait-free and never write. The iterators are weakly consistent, as
    // Java's: an iterator holds its node by a tracked pointer, so it is
    // valid whatever the other threads do, it skips the elements erased
    // since it passed them, and it may or may not see the ones inserted
    // meanwhile; the element it addresses stays alive for as long as the
    // iterator does, its key immutable, its mapped value what the
    // threads make of it. An element is destroyed by the collector with
    // its node, once nothing holds it: not at the erase, which other
    // threads may be reading it across. size() counts, in linear time,
    // as Java's does.
    template<class Key, class V, class Compare = std::less<Key>>
    class sorted_map
    : public detail::SkipList<detail::ConcurrentSortedMapTraits<Key, V, Compare>> {
        using Base = detail::SkipList<detail::ConcurrentSortedMapTraits<Key, V, Compare>>;

    public:
        using mapped_type = V;
        using typename Base::value_type;
        using typename Base::iterator;

        using Base::Base;
        using Base::insert;

        sorted_map() = default;

        // try_emplace searches once: the element is built from the key
        // and the arguments only when the key is absent, and linked
        // between the neighbours that search found
        template<class... A>
        pair<iterator, bool> try_emplace(const Key& key, A&&... a) {
            return this->_insert_absent(key, [&](unsigned h) {
                return this->_make_node(h, std::piecewise_construct, forward_as_tuple(key), forward_as_tuple(std::forward<A>(a)...));
            });
        }

        template<class... A>
        pair<iterator, bool> try_emplace(Key&& key, A&&... a) {
            return this->_insert_absent(key, [&](unsigned h) {
                return this->_make_node(h, std::piecewise_construct, forward_as_tuple(std::move(key)), forward_as_tuple(std::forward<A>(a)...));
            });
        }

        template<class P> requires std::is_constructible_v<value_type, P&&>
        pair<iterator, bool> insert(P&& value) {
            return this->emplace(std::forward<P>(value));
        }
    };
}
