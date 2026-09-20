//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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
    struct CellBlock;

    // The state of a slot. Reachable carries the parity of the epoch the
    // barrier read when it set it (Page::reachable_state): the state of the
    // current parity says "reachable in this cycle", the other parity says
    // nothing, as Used does. Nothing demotes a state: a cycle begins with a
    // flip of the epoch, one atomic store, and every state set before it is
    // out of date at once. Fresh marks an object handed to its first
    // tracked_ptr and not registered yet; with the current parity it says
    // "created after the flip", and the cycle leaves such an object alone
    // (page.h: set_state, collector.h: _register_page). UniqueReleased is
    // the state of a block of cells (cell_block.h) its allocator has let
    // go of: a root still, freed by the collector once every word of the
    // block is free (collector.h: _release_cell_blocks), the way Destroyed
    // is freed without a condition.
    enum State : uint8_t {
        Used = 0,
        Reachable = 1,
        UniqueLock = 2,
        Destroyed = 4,
        UniqueReleased = UniqueLock | Destroyed,
        Releasing = UniqueLock | Reachable,   // with Parity, the allocation's: leaving its unique_ptr, the word and the card not stored yet; a root in either parity, like UniqueLock (page.h: set_state_releasing)
        BadAlloc = 8,
        Reserved = 16,
        Unused = 32,
        Parity = 64,        // with Reachable: the epoch's parity; with UniqueLock: the parity read at the allocation (page.h: unique_state)
        Fresh = 128,        // with Reachable: made tracked in the epoch of its parity, not registered yet
        Unreachable = Used,
        CreatedMask = 15,
        FreeMask = Reserved | Unused
    };

    // The tag of a Pointer built as a word alone, without the barrier
    // (tracked_ptr's store with barrier::off): the copy of an immutable
    // node, its source shaded once afterwards
    struct unshaded_t {};
    inline constexpr unshaded_t unshaded;

    // The tag of a null tracked_ptr made without the thread's
    // registration (slice without an owner): a null roots nothing, so
    // the stack it lies on need not be known to the collector yet
    struct unregistered_t {};
    inline constexpr unregistered_t unregistered;

    void collector_init();
    Collector& collector_instance();
    void terminate_collector() noexcept;
    Thread& current_thread() noexcept;
    void ensure_thread_registered() noexcept;

    // The head of a registered thread's Thread (thread.h), what the
    // barriers need of it: the word of its Data (in_barrier) that their
    // slow paths raise while they store what they derived from the epoch;
    // a dead word once the thread has exited (~Thread). One thread_local
    // for the pointer, the same one the pointers' paths read to see the
    // thread registered: a thread_local of its own for the word cost the
    // released path a second lookup, two nanoseconds on macOS.
    struct ThreadHead {
        std::atomic<uint32_t>* barrier_word = nullptr;
    };
    inline thread_local ThreadHead* current_thread_ptr = nullptr;

    // A barrier's slow path, from the read of the epoch to the last store
    // derived from it (page.h: set_state<Reachable>, the released path of
    // pointer.h, heap.h: mark_card). While the word is raised the
    // collector, having flipped the epoch, waits before it registers,
    // scans and marks (collector.h: _wait_barriers): a thread preempted
    // between its read of the epoch and its store would otherwise land a
    // state of the old parity, which no pass takes for a root, or a card
    // the dirty pass has been through, after the cycle's passes, and an
    // object reachable only through that store was swept (measured on the
    // stepper's gates: an object released from its unique_ptr across the
    // flip, a pointer moved behind the marker, a card stamped late). The
    // raise and the read of the epoch are a Dekker pair with the flip:
    // either the collector sees the word raised and waits for the stores,
    // or the read of the epoch sees the new one. Both are seq_cst, the
    // raise a store and the read a load, and no fence: on arm64 that is
    // stlr and ldar, and a load-acquire is not reordered before an earlier
    // store-release (a fence between them, dmb ish, cost a quarter of the
    // allocation benchmark, since every object released from its
    // unique_ptr comes through here). The epoch is read inside the region
    // with a seq_cst load (page.h, heap.h). A count: the released path
    // stores the state, the word and the card in one region, the card's
    // own inside it.
    struct BarrierRegion {
        std::atomic<uint32_t>* word;
        BarrierRegion() noexcept {
            auto head = current_thread_ptr;
            if (!head) [[unlikely]] {
                ensure_thread_registered();
                head = current_thread_ptr;
            }
            word = head->barrier_word;
            word->store(word->load(std::memory_order_relaxed) + 1, std::memory_order_seq_cst);
        }
        ~BarrierRegion() {
            word->store(word->load(std::memory_order_relaxed) - 1, std::memory_order_release);
        }
        BarrierRegion(const BarrierRegion&) = delete;
        BarrierRegion& operator=(const BarrierRegion&) = delete;
    };
    void waking_up_collector() noexcept;
    void force_short_sleep() noexcept;
    // full collection, waits for the cycle: the allocators' last resort
    void collect_before_bad_alloc();
}
