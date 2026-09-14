//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// logging level [0-3]
#ifndef SGCL_LOG_PRINT_LEVEL
#define SGCL_LOG_PRINT_LEVEL 0
#endif

#include <chrono>
#include <cstddef>

namespace sgcl::config {
    [[maybe_unused]] static constexpr auto   LongSleepTime = std::chrono::seconds(30);
    [[maybe_unused]] static constexpr auto   ShortSleepTime = std::chrono::seconds(3);
                     static constexpr size_t PageSize = 0x10000;
    // Structures written by different threads are kept on lines of this
    // size: 128 bytes covers Apple silicon; x86 uses 64.
                     static constexpr size_t CacheLineSize = 128;
    // The line of the platform's L1 cache: a block of cells
    // (detail/cell_block.h) is one line, so that the pointers it holds
    // share one line and no more.
#if defined(__APPLE__) && defined(__aarch64__)
                     static constexpr size_t L1LineSize = 128;
#else
                     static constexpr size_t L1LineSize = 64;
#endif
    // The managed heap is one virtual range reserved at first use and backed
    // lazily; the reservation costs no physical memory. Pages come from 2 MB
    // chunks aligned to 2 MB (commit/decommit unit, huge-page friendly).
                     static constexpr size_t ChunkSize = 0x200000;
    [[maybe_unused]] static constexpr size_t HeapReserveFactor = 4;                        // times physical memory
    [[maybe_unused]] static constexpr size_t HeapReserveMinimum = size_t(64) << 30;        // 64 GB
    [[maybe_unused]] static constexpr size_t HeapReserveFloor = size_t(1) << 30;           // give up below 1 GB
    // Entirely free chunks are returned to the system once they have been free
    // for a whole GC cycle, except this many kept committed for reuse
    // (recommitting costs a page fault per 4 KB). 32 chunks = 64 MB.
#ifndef SGCL_HEAP_FREE_CHUNK_RESERVE
#define SGCL_HEAP_FREE_CHUNK_RESERVE 32
#endif
    [[maybe_unused]] static constexpr size_t HeapFreeChunkReserve = SGCL_HEAP_FREE_CHUNK_RESERVE;
    // Commit limit: by default this share of the effective memory limit (the
    // cgroup limit, else physical memory), leaving the rest to everything
    // outside the managed heap. Above HeapPressurePercent of it the collector
    // cycles every PressureSleepTime and returns every free chunk; at the
    // limit an allocation forces a collection and, failing that, throws.
    // Heap::set_memory_limit() overrides the default.
    [[maybe_unused]] static constexpr size_t HeapLimitPercent = 90;
    [[maybe_unused]] static constexpr size_t HeapPressurePercent = 75;
    [[maybe_unused]] static constexpr auto   PressureSleepTime = std::chrono::milliseconds(100);
    // Stack roots are found by scanning the used part of every thread's
    // stack. Words left behind by dead frames can keep an object alive until
    // they are overwritten; collector::force_collect() and the counting
    // functions first zero this much stack below the caller's frame, never
    // closer than StackGuardMargin to the end of the thread's stack.
    [[maybe_unused]] static constexpr size_t StackClearSize = 0x10000;
    [[maybe_unused]] static constexpr size_t StackGuardMargin = 0x8000;
    [[maybe_unused]] static constexpr size_t MaxTypesNumber = 4096;
    // The sweep (destructors, freeing of slots) runs on the collector thread
    // while it keeps up. Once a cycle has at least SweepPageThreshold pages
    // of garbage to sweep, helper threads share the work: up to
    // SweepThreadsMax of them (0 = half the hardware threads, at most 8),
    // parked between cycles, so they cost nothing while idle.
    [[maybe_unused]] static constexpr size_t SweepPageThreshold = 256;   // 16 MB of pages
#ifndef SGCL_SWEEP_THREADS_MAX
#define SGCL_SWEEP_THREADS_MAX 0
#endif
    [[maybe_unused]] static constexpr size_t SweepThreadsMax = SGCL_SWEEP_THREADS_MAX;
    // Marking shares the helpers as well, once the previous cycle marked at
    // least MarkObjectThreshold objects (one helper per quarter of it, up to
    // SweepThreadsMax): every thread traces from a stack of its own and
    // hands half of it to an idle one; the mark bit is set atomically.
#ifndef SGCL_MARK_OBJECT_THRESHOLD
#define SGCL_MARK_OBJECT_THRESHOLD (1024 * 1024)
#endif
    [[maybe_unused]] static constexpr size_t MarkObjectThreshold = SGCL_MARK_OBJECT_THRESHOLD;
    // Whether the helpers are used at all is decided by growth, not by
    // allocation: a collector that keeps up leaves the live memory flat
    // however much is allocated. When it grows by HelpersGrowthThreshold
    // bytes between two cycle starts the helpers switch on, and they stay
    // on while the mutators keep allocating at least half of what they
    // allocated per cycle when the growth was seen; below that they park.
#ifndef SGCL_HELPERS_GROWTH_THRESHOLD
#define SGCL_HELPERS_GROWTH_THRESHOLD (size_t(16) << 20)
#endif
    [[maybe_unused]] static constexpr size_t HelpersGrowthThreshold = SGCL_HELPERS_GROWTH_THRESHOLD;
    // The stacks are scanned by the collector thread while the pages they
    // have used add up to less than StackScanThreshold bytes; above it the
    // helpers read the stacks too, in pieces of StackScanSegment bytes,
    // each collecting the words that point into the heap for the collector
    // thread to mark (reading only, no shared state).
    [[maybe_unused]] static constexpr size_t StackScanThreshold = size_t(4) << 20;
    [[maybe_unused]] static constexpr size_t StackScanSegment = size_t(256) << 10;
    // Generational collection with sticky mark bits: a young cycle keeps the
    // marks of the previous cycles, traces only the objects marked for the
    // first time (the young ones) and the marked objects on pages a pointer
    // was stored into since the previous cycle (page.h: mark_card), and
    // sweeps only the unmarked. Garbage among the marked objects waits for a
    // full cycle: after YoungCyclesMax young cycles, when the live memory
    // grew by FullCycleGrowthPercent since the last full cycle, under memory
    // pressure, and for every forced collection.
    // On by default: the young cycles cut the collector's CPU by a quarter
    // to two thirds and the memory by a third on a large live heap, at
    // 0.3 ns per store of a pointer into a heap object (the card: a shift
    // and a byte read; README, "Generations"). -DSGCL_GENERATIONAL=0
    // switches them off: every cycle is full and the barrier does no
    // carding, for a program that links more than it allocates and holds
    // little.
#ifndef SGCL_GENERATIONAL
#define SGCL_GENERATIONAL 1
#endif
    [[maybe_unused]] static constexpr bool     Generational = SGCL_GENERATIONAL;
    [[maybe_unused]] static constexpr unsigned YoungCyclesMax = 8;
    [[maybe_unused]] static constexpr size_t FullCycleGrowthPercent = 100;

    static_assert(PageSize <= 0x10000, "PageSize is too large");
    static_assert((PageSize & (PageSize - 1)) == 0, "PageSize is not a power of 2");
    static_assert(ChunkSize == 32 * PageSize, "a chunk is 32 pages (one 32-bit free mask)");
}
