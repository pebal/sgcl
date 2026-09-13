//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../config.h"

#include <atomic>
#include <cstddef>

namespace sgcl::detail {
    // Pages taken from and returned to the heap, in pages. The mutators add
    // with one relaxed fetch_add per page or range and get back the pages
    // allocated since the cycle began, which is what the wake rule wants
    // (page_allocator.h); no fences, nothing is published through these
    // numbers. The counters written by many threads have lines of their own;
    // the cycle snapshots the collector writes once per cycle share a
    // read-mostly line.
    class MemoryCounters {
    public:
        inline static size_t add_alloc(size_t pages) noexcept {
            auto total = _alloc.fetch_add(pages, std::memory_order_relaxed) + pages;
            return total - _alloc_at_cycle.load(std::memory_order_relaxed);
        }
        inline static void add_free(size_t pages) noexcept {
            _free.fetch_add(pages, std::memory_order_relaxed);
        }
        inline static size_t alloc_since_cycle() noexcept {
            return _alloc.load(std::memory_order_relaxed) - _alloc_at_cycle.load(std::memory_order_relaxed);
        }
        inline static size_t free_since_cycle() noexcept {
            return _free.load(std::memory_order_relaxed) - _free_at_cycle.load(std::memory_order_relaxed);
        }
        // pages allocated during the previous cycle (and the sleep after it)
        inline static size_t last_alloc() noexcept {
            return _last_alloc.load(std::memory_order_relaxed);
        }
        inline static size_t live_pages() noexcept {
            return _alloc.load(std::memory_order_relaxed) - _free.load(std::memory_order_relaxed);
        }
        inline static size_t live_bytes() noexcept {
            return live_pages() * config::PageSize;
        }
        // Pages in use when the last cycle ended, with its garbage swept:
        // the base of the wake rule. The pages in use right now include the
        // garbage waiting for the next sweep, and a rule measured against
        // them lets a backlog grow itself.
        inline static size_t live_after_cycle() noexcept {
            return _live_after_cycle.load(std::memory_order_relaxed);
        }
        inline static void end_cycle() noexcept {
            _live_after_cycle.store(live_pages(), std::memory_order_relaxed);
        }
        inline static void begin_cycle() noexcept {
            auto alloc = _alloc.load(std::memory_order_relaxed);
            _last_alloc.store(alloc - _alloc_at_cycle.load(std::memory_order_relaxed), std::memory_order_relaxed);
            _alloc_at_cycle.store(alloc, std::memory_order_relaxed);
            _free_at_cycle.store(_free.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
    private:
        alignas(config::CacheLineSize) inline static std::atomic<size_t> _alloc = {0};
        alignas(config::CacheLineSize) inline static std::atomic<size_t> _free = {0};
        alignas(config::CacheLineSize) inline static std::atomic<size_t> _alloc_at_cycle = {0};
        inline static std::atomic<size_t> _free_at_cycle = {0};
        inline static std::atomic<size_t> _last_alloc = {0};
        inline static std::atomic<size_t> _live_after_cycle = {0};
    };
}
