//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <atomic>
#include <thread>
#include <vector>

namespace sgcl::detail {
    using RawPointer = std::atomic<void*>;

    template<size_t>
    struct Array;
    struct ArrayBase;
    struct ArrayMetadata;
    struct ChildPointers;
    class  Collector;
    struct Counter;
    class  Heap;
    template<class>
    struct Conservative;
    struct FrameWord;
    template<class>
    struct MayContainTracked;
    template<class>
    class  Maker;
    class  MemoryCounters;
    struct Metadata;
    template<class>
    class  ObjectAllocator;
    class  ObjectAllocatorBase;
    template<class>
    class  ObjectPoolAllocator;
    class  ObjectPoolAllocatorBase;
    struct Page;
    class  PageAllocator;
    template<class>
    struct PageInfo;
    class  Pointer;
    template <class>
    class  RootContainerAllocator;
    class  Thread;
    class  Timer;
    class  Tracked;
    template<class>
    struct TypeInfo;
    struct UniqueDeleter;
    struct WeakCell;

    // The state of a slot. Reachable carries the parity of the epoch the
    // barrier read when it set it (Page::reachable_state): the state of the
    // current parity says "reachable in this cycle", the other parity says
    // nothing, as Used does. Nothing demotes a state: a cycle begins with a
    // flip of the epoch, one atomic store, and every state set before it is
    // out of date at once. Fresh marks an object handed to its first
    // tracked_ptr and not registered yet; with the current parity it says
    // "created after the flip", and the cycle leaves such an object alone
    // (page.h: set_state, collector.h: _register_page).
    enum State : uint8_t {
        Used = 0,
        Reachable = 1,
        UniqueLock = 2,
        Destroyed = 4,
        BadAlloc = 8,
        Reserved = 16,
        Unused = 32,
        Parity = 64,        // with Reachable: the epoch's parity; with UniqueLock: the parity read at the allocation
        Fresh = 128,        // with Reachable: made tracked in the epoch of its parity, not registered yet
        Earlier = 2,        // with Reachable | Fresh: allocated in the epoch before the one of its parity (page.h: set_state)
        Unreachable = Used,
        CreatedMask = 15,
        FreeMask = Reserved | Unused
    };

    void collector_init();
    Collector& collector_instance();
    void terminate_collector() noexcept;
    Thread& current_thread() noexcept;
    void waking_up_collector() noexcept;
    void force_short_sleep() noexcept;
    // full collection, waits for the cycle: the allocators' last resort
    void collect_before_bad_alloc();
}
