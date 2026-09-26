//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstddef>

namespace sgcl {
    template<class>
    class tracked_ptr;
    template<class>
    class unique_ptr;
    template<class>
    class root_ptr;
    template<class>
    class atomic;
    template<class>
    class atomic_ref;
    class collector;

    // The write barrier of a store into a tracked_ptr, off: the tag of
    // the store that leaves it out, `p.store(q, barrier::off)`, for the
    // copy of a node that never changes, whose source is shaded once
    // afterwards (the immutable containers). A tag, so that the store
    // without the barrier is an overload of its own, chosen at compile
    // time; every other store has the barrier and needs no tag.
    struct barrier {
        struct off_t {};
        static constexpr off_t off = {};
    };

    template<class, size_t, class>
    class array;
    namespace concurrent {
        template<class, class, class>
        class sorted_map;
        template<class>
        class queue;
        template<class, class>
        class sorted_set;
        template<class, class, class, class>
        class map;
        template<class, class, class>
        class set;
        template<class>
        class stack;
        template<class>
        class copy_on_write;
    }
    namespace async {
        template<class>
        class channel;
    }
    template<class>
    class deque;
    template<class>
    class expiry_queue;
    template<class>
    class forward_list;
    template<class>
    class list;
    template<class, class, class>
    class sorted_map;
    template<class, class, class>
    class sorted_multimap;
    template<class, class>
    class sorted_multiset;
    template<class, class, class>
    class priority_queue;
    template<class, class>
    class queue;
    template<class, class>
    class sorted_set;
    template<class, class>
    class stack;
    template<class, class, class, class>
    class map;
    template<class, class, class, class>
    class multimap;
    template<class, class, class>
    class multiset;
    template<class, class, class>
    class set;
    template<class>
    class vector;
    template<class>
    class weak_ptr;
}
