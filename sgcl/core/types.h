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

    template<class, size_t>
    struct array;
    template<class, class, class>
    class concurrent_map;
    template<class>
    class concurrent_queue;
    template<class, class>
    class concurrent_set;
    template<class, class, class, class>
    class concurrent_unordered_map;
    template<class, class, class>
    class concurrent_unordered_set;
    template<class>
    class concurrent_stack;
    template<class>
    class channel;
    template<class>
    class copy_on_write;
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
