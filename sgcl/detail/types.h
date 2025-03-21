//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <atomic>

namespace sgcl::detail {
    using RawPointer = std::atomic<void*>;

    template<size_t>
    struct Array;
    struct ArrayBase;
    struct ArrayMetadata;
    struct Block;
    class  BlockAllocator;
    struct ChildPointers;
    class  Collector;
    struct Counter;
    struct DataPage;
    template<class>
    class  ArrayIterator;
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
    template<class>
    struct PageInfo;
    class  Pointer;
    template<unsigned, unsigned>
    class  PointerPool;
    class  PointerPoolBase;
    template <class>
    class  RootContainerAllocator;
    struct StackPointerAllocator;
    class  Thread;
    class  Timer;
    class  Tracked;
    template<class>
    struct TypeInfo;
    struct UniqueDeleter;

    enum State : uint8_t {
        Used = 0,
        Reachable = 1,
        UniqueLock = 2,
        Destroyed = 4,
        BadAlloc = 8,
        Reserved = 16,
        Unused = 32,
        Unreachable = Used,
        ReachableMask = 3,
        CreatedMask = 15
    };

    void collector_init();
    Collector& collector_instance();
    void terminate_collector() noexcept;
    Thread& current_thread() noexcept;
    template<class T>
    void* clone_object(const void*);
    void waking_up_collector() noexcept;
}
