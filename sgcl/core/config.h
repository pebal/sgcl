//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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
    [[maybe_unused]] static constexpr auto   long_sleep_time = std::chrono::seconds(30);
    [[maybe_unused]] static constexpr auto   short_sleep_time = std::chrono::seconds(3);
                     static constexpr size_t page_size = 0x10000;
    // Structures written by different threads are kept on lines of this
    // size: 128 bytes covers Apple silicon; x86 uses 64.
                     static constexpr size_t cache_line_size = 128;
    // The line of the platform's L1 cache: a block of cells
    // (detail/cell_block.h) is one line, so that the pointers it holds
    // share one line and no more.
#if defined(__APPLE__) && defined(__aarch64__)
                     static constexpr size_t l1_line_size = 128;
#else
                     static constexpr size_t l1_line_size = 64;
#endif
    // The managed heap is one virtual range reserved at first use and backed
    // lazily; the reservation costs no physical memory. Pages come from 2 MB
    // chunks aligned to 2 MB (commit/decommit unit, huge-page friendly).
                     static constexpr size_t chunk_size = 0x200000;
    [[maybe_unused]] static constexpr size_t heap_reserve_factor = 4;                        // times physical memory
    [[maybe_unused]] static constexpr size_t heap_reserve_minimum = size_t(64) << 30;        // 64 GB
    [[maybe_unused]] static constexpr size_t heap_reserve_floor = size_t(1) << 30;           // give up below 1 GB
    // Entirely free chunks are returned to the system once they have been free
    // for a whole GC cycle, except this many kept committed for reuse
    // (recommitting costs a page fault per 4 KB). 32 chunks = 64 MB.
#ifndef SGCL_HEAP_FREE_CHUNK_RESERVE
#define SGCL_HEAP_FREE_CHUNK_RESERVE 32
#endif
    [[maybe_unused]] static constexpr size_t heap_free_chunk_reserve = SGCL_HEAP_FREE_CHUNK_RESERVE;
    // Commit limit: by default this share of the effective memory limit (the
    // cgroup limit, else physical memory), leaving the rest to everything
    // outside the managed heap. Above heap_pressure_percent of it the collector
    // cycles every pressure_sleep_time and returns every free chunk; at the
    // limit an allocation forces a collection and, failing that, throws.
    // Heap::set_memory_limit() overrides the default.
    [[maybe_unused]] static constexpr size_t heap_limit_percent = 90;
    [[maybe_unused]] static constexpr size_t heap_pressure_percent = 75;
    [[maybe_unused]] static constexpr auto   pressure_sleep_time = std::chrono::milliseconds(100);
    // Stack roots are found by scanning the used part of every thread's
    // stack. Words left behind by dead frames can keep an object alive until
    // they are overwritten; collector::force_collect() and the counting
    // functions first zero this much stack below the caller's frame, never
    // closer than stack_guard_margin to the end of the thread's stack.
    [[maybe_unused]] static constexpr size_t stack_clear_size = 0x10000;
    [[maybe_unused]] static constexpr size_t stack_guard_margin = 0x8000;
    [[maybe_unused]] static constexpr size_t max_types_number = 4096;
    // The sweep (destructors, freeing of slots) runs on the collector thread
    // while it keeps up. Once a cycle has at least sweep_page_threshold pages
    // of garbage to sweep, helper threads share the work: up to
    // sweep_threads_max of them (0 = half the hardware threads, at most 8),
    // parked between cycles, so they cost nothing while idle.
    [[maybe_unused]] static constexpr size_t sweep_page_threshold = 256;   // 16 MB of pages
#ifndef SGCL_SWEEP_THREADS_MAX
#define SGCL_SWEEP_THREADS_MAX 0
#endif
    [[maybe_unused]] static constexpr size_t sweep_threads_max = SGCL_SWEEP_THREADS_MAX;
    // Marking shares the helpers as well, once the previous cycle marked at
    // least mark_object_threshold objects (one helper per quarter of it, up to
    // sweep_threads_max): every thread traces from a stack of its own and
    // hands half of it to an idle one; the mark bit is set atomically.
#ifndef SGCL_MARK_OBJECT_THRESHOLD
#define SGCL_MARK_OBJECT_THRESHOLD (1024 * 1024)
#endif
    [[maybe_unused]] static constexpr size_t mark_object_threshold = SGCL_MARK_OBJECT_THRESHOLD;
    // The parallel marking traces the objects it pops from its stack through
    // a window of this many, each prefetched as it enters and traced as it
    // leaves: the depth-first order over a graph laid out by allocation
    // misses the cache at every object, and the window lets the misses
    // overlap (a random graph of 65536 roots marks in 5.3 ns per object
    // instead of 9.5; a tree laid out in the order it is traced gains
    // nothing). A power of two; 0 traces straight from the stack.
#ifndef SGCL_MARK_PREFETCH_WINDOW
#define SGCL_MARK_PREFETCH_WINDOW 8
#endif
    [[maybe_unused]] static constexpr unsigned mark_prefetch_window = SGCL_MARK_PREFETCH_WINDOW;
    // The longest wait of the exponential backoff of a failed
    // compare-exchange on a contended word (stack.h), in pause
    // instructions (detail/os.h: spin_pause): the wait doubles from one
    // pause per failure up to this. The cap is a time, some 40 µs, and the
    // count is that time over the cost of the platform's pause: isb on
    // arm64 takes 9 ns (Apple M2: 4096), pause on x86 40 ns on Skylake and
    // later (140 cycles; a few on older cores: 1024), and where there is no
    // pause instruction a turn of the loop takes a nanosecond or less
    // (65536). On 16 threads at one word the Treiber stack goes from 740
    // ns per operation to 23 with 4096 on arm64, to 40 with 1024; the tail
    // of one operation under that contention is the cap. 0 retries at once.
#ifndef SGCL_BACKOFF_MAX
#if defined(__aarch64__) || defined(_M_ARM64)
#define SGCL_BACKOFF_MAX 4096
#elif defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define SGCL_BACKOFF_MAX 1024
#else
#define SGCL_BACKOFF_MAX 65536
#endif
#endif
    [[maybe_unused]] static constexpr unsigned backoff_max = SGCL_BACKOFF_MAX;
    // The scheduler of the tasks (scheduler.h): the number of worker
    // threads, 0 for the hardware concurrency; and how long a worker
    // with nothing to run looks at the queue before it sleeps in the
    // kernel, in microseconds (a task made ready in that window costs
    // no wake).
#ifndef SGCL_WORKERS
#define SGCL_WORKERS 0
#endif
    [[maybe_unused]] static constexpr unsigned workers = SGCL_WORKERS;
#ifndef SGCL_WORKER_SPIN_US
#define SGCL_WORKER_SPIN_US 20
#endif
    [[maybe_unused]] static constexpr unsigned worker_spin_microseconds = SGCL_WORKER_SPIN_US;
    // The pool of threads for blocking calls (async/blocking.h:
    // spawn_blocking), apart from the workers: the most threads it grows
    // to, 0 for the larger of 64 and four times the hardware concurrency
    // (its threads sit in the kernel, so there are more of them than
    // cores); and how long an idle thread of it waits for a job before
    // it exits, in milliseconds.
#ifndef SGCL_BLOCKING_THREADS
#define SGCL_BLOCKING_THREADS 0
#endif
    [[maybe_unused]] static constexpr unsigned blocking_threads = SGCL_BLOCKING_THREADS;
#ifndef SGCL_BLOCKING_IDLE_MS
#define SGCL_BLOCKING_IDLE_MS 10000
#endif
    [[maybe_unused]] static constexpr unsigned blocking_idle_milliseconds = SGCL_BLOCKING_IDLE_MS;
    // The buffers of io: the block a buffered reader or writer holds in
    // front of its stream, and the block copy() moves data through. Each
    // is one managed array without a header, so a divisor of page_size
    // fills whole pages: 8 KB gives eight to a page, 32 KB two.
    static constexpr size_t io_buffer_size = 0x2000;
    static constexpr size_t io_copy_buffer_size = 0x8000;
    // Whether the helpers are used at all is decided by growth, not by
    // allocation: a collector that keeps up leaves the live memory flat
    // however much is allocated. When it grows by helpers_growth_threshold
    // bytes between two cycle starts the helpers switch on, and they stay
    // on while the mutators keep allocating at least half of what they
    // allocated per cycle when the growth was seen; below that they park.
#ifndef SGCL_HELPERS_GROWTH_THRESHOLD
#define SGCL_HELPERS_GROWTH_THRESHOLD (size_t(16) << 20)
#endif
    [[maybe_unused]] static constexpr size_t helpers_growth_threshold = SGCL_HELPERS_GROWTH_THRESHOLD;
    // The stacks are scanned by the collector thread while the pages they
    // have used add up to less than stack_scan_threshold bytes; above it the
    // helpers read the stacks too, in pieces of stack_scan_segment bytes,
    // each collecting the words that point into the heap for the collector
    // thread to mark (reading only, no shared state).
    [[maybe_unused]] static constexpr size_t stack_scan_threshold = size_t(4) << 20;
    [[maybe_unused]] static constexpr size_t stack_scan_segment = size_t(256) << 10;
    // generational collection with sticky mark bits: a young cycle keeps the
    // marks of the previous cycles, traces only the objects marked for the
    // first time (the young ones) and the marked objects on pages a pointer
    // was stored into since the previous cycle (page.h: mark_card), and
    // sweeps only the unmarked. Garbage among the marked objects waits for a
    // full cycle: after young_cycles_max young cycles, when the live memory
    // grew by full_cycle_growth_percent since the last full cycle, under memory
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
    [[maybe_unused]] static constexpr bool     generational = SGCL_GENERATIONAL;
    [[maybe_unused]] static constexpr unsigned young_cycles_max = 8;
    [[maybe_unused]] static constexpr size_t full_cycle_growth_percent = 100;

    static_assert(page_size <= 0x10000, "page_size is too large");
    static_assert((page_size & (page_size - 1)) == 0, "page_size is not a power of 2");
    static_assert(chunk_size == 32 * page_size, "a chunk is 32 pages (one 32-bit free mask)");
}
