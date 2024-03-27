//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include <atomic>

namespace sgcl {    
    namespace Priv {
        using Const_pointer = std::atomic<const void*>;
        using Pointer = std::atomic<void*>;

        template<size_t>
        struct Array;

        struct Array_base;
        struct Array_metadata;
        struct Block;
        struct Block_allocator;
        struct Child_pointers;
        struct Collector;
        struct Counter;
        struct DataPage;
        struct Heap_roots_allocator;

        template<class>
        struct Large_object_allocator;

        template<class>
        struct Maker;

        struct Metadata;
        struct Object_allocator;
        struct Page;

        template<class>
        struct Page_info;

        template<unsigned, unsigned>
        struct Pointer_pool;

        struct Pointer_pool_base;

        template<class T>
        struct Small_object_allocator;

        struct Small_object_allocator_base;

        struct Stack_roots_allocator;
        struct Thread;
        struct Timer;
        class Tracked;
        class Tracked_ptr;
        struct Unique_deleter;

        enum State : uint8_t {
            Used = 0,
            Reachable,
            ReachableAtomic = 4,
            UniqueLock,
            Destroyed,
            BadAlloc,
            Reserved,
            Unused
        };
    }
}
