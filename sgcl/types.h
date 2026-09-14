//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

namespace sgcl {
    template<class>
    class tracked_ptr;
    template<class>
    class unique_ptr;
    template<class>
    class atomic;
    template<class>
    class atomic_ref;
    class collector;

    // The containers, the observers and the coroutines take the kind of
    // the word by which they hold their memory as their last parameter:
    // tracked_ptr by default, gc::tracked_ptr for one that lives anywhere (gc.h).
    template<class, size_t, template<class> class = tracked_ptr>
    struct array;
    template<class, template<class> class = tracked_ptr>
    class deque;
    template<class, template<class> class = tracked_ptr>
    class expiry_queue;
    template<class, template<class> class = tracked_ptr>
    class forward_list;
    template<class, template<class> class = tracked_ptr>
    class list;
    template<class, class, class, template<class> class>
    class map;
    template<class, class, class, template<class> class>
    class multimap;
    template<class, class, template<class> class>
    class multiset;
    template<class, class, class>
    class priority_queue;
    template<class, class>
    class queue;
    template<class, class, template<class> class>
    class set;
    template<class, class>
    class stack;
    template<class, class, class, class, template<class> class>
    class unordered_map;
    template<class, class, class, class, template<class> class>
    class unordered_multimap;
    template<class, class, class, template<class> class>
    class unordered_multiset;
    template<class, class, class, template<class> class>
    class unordered_set;
    template<class, template<class> class = tracked_ptr>
    class vector;
    template<class, template<class> class = tracked_ptr>
    class weak_ptr;
}

namespace gc {
    template<class>
    class tracked_ptr;
}
