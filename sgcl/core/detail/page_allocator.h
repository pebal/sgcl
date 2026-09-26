//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../aliases.h"
#include "heap.h"
#include "memory_counters.h"

#include <new>

namespace sgcl::detail {
    // Per-thread cache of pages taken from the heap 8 at a time (one heap
    // mutex per 512 KB). Not a whole 2 MB chunk: a thread that takes all 32
    // pages drains the pool, so its next refill reaches for a fresh chunk
    // from the bump pointer and pays the page faults, while with a fraction
    // of a chunk the pages the collector frees come back to the pool in time
    // and the thread keeps cycling inside already-touched memory (measured:
    // +1 ns per 32-byte allocation for 32 pages; 4, 8 and 16 are equal).
    class PageAllocator {
    public:
        static constexpr unsigned CacheSize = 8;

        ~PageAllocator() noexcept {
            if (_count) {
                Heap::instance().free_pages(_cache, _count);
            }
        }

        // One page from the cache, the cache refilled from the heap when
        // empty. The collector is woken by the allocation that crosses the
        // wake rule: a cycle once the pages allocated since the last one
        // reach a quarter of what the last one left in use (at least 1 MB),
        // or when the heap is near its ceiling.
        void* alloc() {
            auto since = MemoryCounters::add_alloc(1);
            auto& heap = Heap::instance();
            bool wake = since * 4 > MemoryCounters::live_after_cycle() + 64 || heap.under_pressure();
            if (!_count) {
                _count = heap.alloc_pages(_cache, CacheSize);
                if (!_count) {
                    // at the commit limit (or out of address space): a full
                    // collection may free pages; otherwise give up cleanly
                    collect_before_bad_alloc();
                    _count = heap.alloc_pages(_cache, CacheSize);
                    if (!_count) {
                        throw bad_alloc();
                    }
                }
            }
            auto page = _cache[--_count];
            if (wake) {
                waking_up_collector();
            }
            return page;
        }

    private:
        void* _cache[CacheSize];
        unsigned _count = 0;
    };
}
