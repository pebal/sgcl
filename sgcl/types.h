//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

namespace sgcl {
    template<class, size_t>
    struct array;
    template<class>
    class atomic;
    template<class>
    class atomic_ref;
    class collector;
    template<class>
    class deque;
    template<class>
    class expiry_queue;
    template<class>
    class forward_list;
    template<class>
    class list;
    template<class, class, class>
    class map;
    template<class, class, class>
    class multimap;
    template<class, class>
    class multiset;
    template<class, class, class>
    class priority_queue;
    template<class, class>
    class queue;
    template<class, class>
    class set;
    template<class, class>
    class stack;
    template<class>
    class tracked_ptr;
    template<class>
    class unique_ptr;
    template<class, class, class, class>
    class unordered_map;
    template<class, class, class, class>
    class unordered_multimap;
    template<class, class, class>
    class unordered_multiset;
    template<class, class, class>
    class unordered_set;
    template<class>
    class vector;
    template<class>
    class weak_ptr;
}
