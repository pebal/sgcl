//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../config.h"
#include "os.h"
#include "types.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <vector>

namespace sgcl::detail {
    // The managed heap is one virtual range reserved at first use and backed
    // lazily. It is handed out in 64 KB pages taken from 2 MB chunks aligned
    // to 2 MB: a chunk is committed when first used and decommitted when every
    // page in it is free. A side table maps each page to its header, so
    // page_of() never touches the page and the collector never writes into
    // data pages while marking (headers live outside; fork/COW keeps working).
    //
    // Locating anything is shifts on (p - base): page index p >> 16, chunk
    // index p >> 21, page within chunk (p >> 16) & 31.
    // What the barrier and the allocators read at every store and every
    // slot, on a line of their own: the heap's range and tables, written
    // once, and the epoch, written by the collector at every flip. As
    // separate statics the linker laid them next to a mutex taken once per
    // page and the page counters written once per cycle, whose writes took
    // the line from every mutator (Heap::globals).
    struct alignas(config::CacheLineSize) HeapGlobals {
        uintptr_t base = 0;
        size_t size = 0;
        std::atomic<Page*>* table = nullptr;
        std::atomic<Page*>* biased_table = nullptr;   // table - (base >> PageShift)
        std::atomic<uint8_t>* cards = nullptr;         // CardCount cards, one per 64 KB of address space (card_of)
        std::atomic<uint32_t> epoch = {1};             // the cycle number, advanced at the start of every cycle (page.h: flip_epoch)
        std::atomic<uint8_t> epoch_byte = {1};         // its low byte, for the cards
        // Reachable with the parity of the epoch, stored by the collector at
        // every flip: one byte load for the barrier (page.h: reachable_state)
        std::atomic<State> current_reachable = {State(State::Reachable | State::Parity)};
    };

    class Heap {
    public:
        static constexpr size_t PageSize = config::PageSize;
        static constexpr size_t ChunkSize = config::ChunkSize;
        static constexpr unsigned PageShift = std::countr_zero(PageSize);
        static constexpr unsigned ChunkShift = std::countr_zero(ChunkSize);
        static constexpr unsigned PagesPerChunk = ChunkSize / PageSize;
        static_assert(PagesPerChunk == 32, "a chunk's free mask is a 32-bit word");

        // Never destroyed: the GC thread may still be returning pages while
        // the process runs its static destructors, and the range is meant to
        // live as long as the process anyway.
        static Heap& instance() {
            static Heap* heap = new Heap;
            return *heap;
        }

        // Write-barrier path: the pointer is known to be managed. The table
        // pointer is pre-biased by base >> PageShift, so this is one global
        // load, a shift and one load from a hot table.
        static Page* page_of(const void* p) noexcept {
            return globals.biased_table[(uintptr_t)p >> PageShift].load(std::memory_order_relaxed);
        }

        // For pointers of unknown origin (stack scanning): null when the
        // address is outside the heap or the page holds no object. Acquire
        // pairs with the release in set_pages: the header is complete.
        static Page* page_of_checked(const void* p) noexcept {
            auto offset = (uintptr_t)p - globals.base;
            return offset < globals.size ? globals.table[offset >> PageShift].load(std::memory_order_acquire) : nullptr;
        }

        static bool contains(const void* p) noexcept {
            return (uintptr_t)p - globals.base < globals.size;
        }

        static uintptr_t base() noexcept {
            return globals.base;
        }

        static size_t size() noexcept {
            return globals.size;
        }

        // Card marking for the young cycles (page.h: mark_card): one byte
        // per 64 KB of address space, in a table covering the whole user
        // address space (2 GB of virtual memory, backed by the system for
        // the pages a program touches: the cards of its heap and a page
        // or two for its stacks), so that the barrier's stamp is one
        // shift, one indexed load and, on a change, one store: no range
        // check, no page table, no page header. A store into a location
        // outside the heap (a stack: the rules allow no other place for a
        // tracked_ptr) stamps a card nothing reads. The index is masked to
        // the table, so an address beyond 47 bits aliases a card of
        // another region: a spurious stamp at worst, never a missed one.
        // The byte is the low byte of the epoch; the collector reads a
        // page (or a range of pages) as dirty when a card of it was stamped
        // in the current epoch or the one before, modulo 256: a card 256 or
        // 257 cycles old reads as dirty too, which costs a trace and never
        // a miss.
        static constexpr size_t CardCount = size_t(1) << (47 - PageShift);

        static std::atomic<uint8_t>& card_of(const void* location) noexcept {
            return globals.cards[((uintptr_t)location >> PageShift) & (CardCount - 1)];
        }

        static void stamp_card(const void* location, uint32_t epoch) noexcept {
            if constexpr(!config::Generational) {
                return;
            }
            auto& card = card_of(location);
            auto e = (uint8_t)epoch;
            if (card.load(std::memory_order_relaxed) != e) {
                card.store(e, std::memory_order_release);
            }
        }

        // The barrier's card stamp, with the epoch byte kept in the same
        // line of statics as the table's address
        static void mark_card(const void* location) noexcept {
            auto& card = card_of(location);
            auto e = globals.epoch_byte.load(std::memory_order_relaxed);
            if (card.load(std::memory_order_relaxed) != e) {
                card.store(e, std::memory_order_release);
            }
        }

        static void set_epoch(uint32_t epoch) noexcept {
            globals.epoch_byte.store((uint8_t)epoch, std::memory_order_relaxed);
        }

        static bool dirty_since_last(const void* data, size_t pages, uint32_t epoch) noexcept {
            if constexpr(!config::Generational) {
                return false;
            }
            for (size_t i = 0; i < pages; ++i) {
                auto card = card_of((const char*)data + i * PageSize).load(std::memory_order_acquire);
                if ((uint8_t)(epoch - card) <= 1) {
                    return true;
                }
            }
            return false;
        }

        // Publishes the header of one page, or of every page of a range.
        static void set_pages(const void* data, size_t pages, Page* page) noexcept {
            auto first = ((uintptr_t)data - globals.base) >> PageShift;
            // a release store per entry (a release fence and relaxed stores
            // would do, but the thread sanitizer does not follow fences)
            for (size_t i = 0; i < pages; ++i) {
                globals.table[first + i].store(page, std::memory_order_release);
            }
        }

        // Single pages for the pool allocators; fills up to `count` entries.
        unsigned alloc_pages(void** out, unsigned count) {
            std::lock_guard<std::mutex> lock(_mutex);
            unsigned n = 0;
            while (n < count) {
                auto p = _alloc_page_locked();
                if (!p) {
                    break;
                }
                out[n++] = p;
            }
            return n;
        }

        void free_pages(void* const* pages, unsigned count) noexcept {
            std::lock_guard<std::mutex> lock(_mutex);
            for (unsigned i = 0; i < count; ++i) {
                _free_page_locked(pages[i]);
            }
        }

        // Contiguous pages for objects larger than a page.
        void* alloc_range(size_t pages) {
            std::lock_guard<std::mutex> lock(_mutex);
            return _alloc_range_locked(pages);
        }

        void free_range(const void* data, size_t pages) noexcept {
            std::lock_guard<std::mutex> lock(_mutex);
            _free_range_locked(((uintptr_t)data - globals.base) >> PageShift, pages);
        }

        // Diagnostics: free page ranges outside the pool chunks.
        size_t free_range_count() noexcept {
            std::lock_guard<std::mutex> lock(_mutex);
            size_t n = 0;
            for (auto& bin : _bins) {
                n += bin.size();
            }
            return n;
        }

        size_t committed_bytes() const noexcept {
            return _committed_chunks.load(std::memory_order_relaxed) * ChunkSize;
        }

        // Hard cap on committed memory (0 = none); see config::HeapLimitPercent.
        size_t memory_limit() const noexcept {
            return _limit.load(std::memory_order_relaxed);
        }

        void set_memory_limit(size_t bytes) noexcept {
            _limit.store(bytes, std::memory_order_relaxed);
        }

        // Above config::HeapPressurePercent of the limit.
        bool under_pressure() const noexcept {
            auto limit = memory_limit();
            return limit && committed_bytes() * 100 >= limit * config::HeapPressurePercent;
        }

        size_t reserved_bytes() const noexcept {
            return globals.size;
        }

    private:
        struct Chunk {
            uint32_t free_mask;   // meaningful while in_pool: bit set = page free
            bool committed;
            bool in_pool;         // hands out single pages
            bool free_committed;  // entirely free, still committed
            uint32_t free_epoch;  // trim() epoch in which it became free
        };

        // A chunk taken for pages again: no longer idle for trim()
        void _mark_in_use(size_t c) noexcept {
            auto& chunk = _chunks[c];
            if (chunk.free_committed) {
                chunk.free_committed = false;
                --_free_committed;
            }
        }

        struct Range {            // free pages outside the pool chunks
            size_t first;
            size_t count;
        };

        Heap() {
            auto physical = os::physical_memory();
            size_t size = std::max(physical * config::HeapReserveFactor, config::HeapReserveMinimum);
            os::Reservation r;
            // The range gets a guard chunk at both ends (never allocated,
            // never committed): the write barrier takes an address within
            // ChunkSize of its own frame for a stack address without
            // looking at the range (page.h: mark_card), which holds only
            // if no object can lie that close to anything outside the range.
            while (size >= config::HeapReserveFloor) {
                r = os::reserve(size + 3 * ChunkSize);   // slack to align the base to a chunk, two guards
                if (r.base) {
                    break;
                }
                size /= 2;
            }
            if (!r.base) {
                std::fprintf(stderr, "[sgcl] cannot reserve address space for the managed heap\n");
                std::terminate();
            }
            _reservation = r;
            globals.base = (((uintptr_t)r.base + ChunkSize - 1) & ~(uintptr_t)(ChunkSize - 1)) + ChunkSize;
            globals.size = size & ~(uintptr_t)(ChunkSize - 1);
            _needs_commit = r.needs_commit;
            _page_count = globals.size >> PageShift;
            _chunk_count = globals.size >> ChunkShift;
            _top_chunk = _chunk_count;
            // calloc: large allocations come from the OS as untouched zero
            // pages, so only the used part of these tables costs memory.
            globals.table = (std::atomic<Page*>*)std::calloc(_page_count, sizeof(std::atomic<Page*>));
            _chunks = (Chunk*)std::calloc(_chunk_count, sizeof(Chunk));
            _tags = (Tag*)std::calloc(_page_count, sizeof(Tag));
            if constexpr(config::Generational) {
                globals.cards = (std::atomic<uint8_t>*)os::map_lazy(CardCount);
            }
            globals.biased_table = globals.table - (globals.base >> PageShift);
            if (!globals.table || !_chunks || !_tags || (config::Generational && !globals.cards)) {
                std::fprintf(stderr, "[sgcl] cannot allocate the heap tables\n");
                std::terminate();
            }
            _has_free.assign((_chunk_count + 63) / 64, 0);
            os::advise_huge_pages((void*)globals.base, globals.size);
            if (auto limit = os::memory_limit()) {
                _limit.store(limit / 100 * config::HeapLimitPercent, std::memory_order_relaxed);
            }
        }

        ~Heap() = default;   // the range lives as long as the process

        uintptr_t _chunk_address(size_t c) const noexcept {
            return globals.base + (c << ChunkShift);
        }

        bool _commit_chunk(size_t c) noexcept {
            auto& chunk = _chunks[c];
            if (!chunk.committed) {
                auto limit = memory_limit();
                if (limit && committed_bytes() + ChunkSize > limit) {
                    return false;   // the caller collects and retries, then throws
                }
                if (_needs_commit && !os::commit((void*)_chunk_address(c), ChunkSize)) {
                    return false;
                }
                chunk.committed = true;
                _committed_chunks.fetch_add(1, std::memory_order_relaxed);
            }
            return true;
        }

        void _decommit_chunk(size_t c) noexcept {
            auto& chunk = _chunks[c];
            if (chunk.committed) {
                os::decommit((void*)_chunk_address(c), ChunkSize, _needs_commit);
                chunk.committed = false;
                _committed_chunks.fetch_sub(1, std::memory_order_relaxed);
            }
        }

        void _set_has_free(size_t c, bool value) noexcept {
            auto mask = uint64_t(1) << (c % 64);
            if (value) {
                _has_free[c / 64] |= mask;
            } else {
                _has_free[c / 64] &= ~mask;
            }
        }

        void* _take_page_from_chunk(size_t c) noexcept {
            auto& chunk = _chunks[c];
            unsigned bit = std::countr_zero(chunk.free_mask);
            chunk.free_mask &= ~(uint32_t(1) << bit);
            if (!chunk.free_mask) {
                _set_has_free(c, false);
            }
            return (void*)(_chunk_address(c) + ((size_t)bit << PageShift));
        }

        // One page for a pool: from a pool chunk with a free page, else a
        // free chunk kept for the pools, else a fresh chunk from the bottom
        // of the range (the arrays' ranges come from the top), committed
        // on the way if the system needs it.
        void* _alloc_page_locked() {
            // 1. a pool chunk with a free page, scanning from the last hit
            auto words = _has_free.size();
            for (size_t w = 0; w < words; ++w) {
                auto i = _hint + w;
                if (i >= words) {
                    i -= words;
                }
                if (auto word = _has_free[i]) {
                    _hint = i;
                    return _take_page_from_chunk(i * 64 + std::countr_zero(word));
                }
            }
            // 2. a chunk the pools gave back, else a fresh one from the
            // bottom of the range (the arrays take theirs from the top)
            size_t c;
            if (!_free_pool_chunks.empty()) {
                c = _free_pool_chunks.back();
                _free_pool_chunks.pop_back();
                _mark_in_use(c);
            } else {
                if (_next_chunk >= _top_chunk) {
                    return nullptr;
                }
                c = _next_chunk++;
            }
            if (!_commit_chunk(c)) {
                _free_pool_chunks.push_back(c);
                return nullptr;
            }
            auto& chunk = _chunks[c];
            chunk.in_pool = true;
            chunk.free_mask = ~uint32_t(0);
            _set_has_free(c, true);
            _hint = c / 64;
            return _take_page_from_chunk(c);
        }

        // A pool page back to its chunk; a chunk with every page free leaves
        // the pool and waits, committed, for trim()
        void _free_page_locked(void* p) noexcept {
            auto index = ((uintptr_t)p - globals.base) >> PageShift;
            globals.table[index].store(nullptr, std::memory_order_relaxed);
            auto c = index / PagesPerChunk;
            auto& chunk = _chunks[c];
            chunk.free_mask |= uint32_t(1) << (index % PagesPerChunk);
            if (chunk.free_mask == ~uint32_t(0)) {
                // every page free: leave the pool, keep the chunk for the
                // pools (committed until trim() finds it idle for a cycle)
                chunk.in_pool = false;
                chunk.free_mask = 0;
                _set_has_free(c, false);
                if (chunk.committed && !chunk.free_committed) {
                    chunk.free_committed = true;
                    chunk.free_epoch = _epoch;
                    ++_free_committed;
                }
                _free_pool_chunks.push_back(c);
            } else {
                _set_has_free(c, true);
            }
        }

        // Splits a chunk-aligned run of 32 pages out of the free ranges.
        void* _alloc_range_locked(size_t pages) {
            size_t first;
            bool found = false;
            // the smallest bin that can hold the request, first fit inside
            // it; the ranges of every higher bin fit, the first one is taken
            auto bin = _bin_of(pages);
            for (auto bits = _bin_mask >> bin << bin; bits && !found; bits &= bits - 1) {
                auto b = std::countr_zero(bits);
                for (auto r : _bins[b]) {
                    if (r.count >= pages) {
                        _remove(r);
                        first = r.first;
                        if (r.count > pages) {
                            _insert_range(first + pages, r.count - pages);
                        }
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                // fresh chunks from the top of the range, down towards the
                // pools: the two never interleave, so a freed array range
                // coalesces with its neighbours instead of breaking on a
                // pool chunk
                auto chunks = (pages + PagesPerChunk - 1) / PagesPerChunk;
                if (_top_chunk < _next_chunk + chunks) {
                    return nullptr;
                }
                _top_chunk -= chunks;
                first = _top_chunk * PagesPerChunk;
                auto leftover = chunks * PagesPerChunk - pages;
                if (leftover) {
                    _insert_range(first + pages, leftover);
                }
            }
            for (auto c = first / PagesPerChunk; c <= (first + pages - 1) / PagesPerChunk; ++c) {
                _mark_in_use(c);
                if (!_commit_chunk(c)) {
                    _free_range_locked(first, pages);
                    return nullptr;
                }
            }
            return (void*)(globals.base + (first << PageShift));
        }

        // A range of an array back to the bins, merged with its free
        // neighbours; the whole chunks inside the merged range wait for
        // trim()
        void _free_range_locked(size_t first, size_t count) noexcept {
            for (size_t i = 0; i < count; ++i) {
                globals.table[first + i].store(nullptr, std::memory_order_relaxed);
            }
            auto merged = _insert_range(first, count);
            // Chunks now entirely free stay committed; trim() returns them to
            // the system once they have been free for a whole GC cycle, so
            // steady-state churn never pays for decommit and page faults.
            auto begin = (merged.first + PagesPerChunk - 1) / PagesPerChunk;
            auto end = (merged.first + merged.count) / PagesPerChunk;
            for (auto c = begin; c < end; ++c) {
                auto& chunk = _chunks[c];
                if (chunk.committed && !chunk.free_committed) {
                    chunk.free_committed = true;
                    chunk.free_epoch = _epoch;
                    ++_free_committed;
                }
            }
        }

    public:
        // GC thread, once per cycle: decommits chunks that were already free
        // at the previous call, keeping config::HeapFreeChunkReserve of them.
        void trim() noexcept {
            std::lock_guard<std::mutex> lock(_mutex);
            auto previous = _epoch++;
            // under memory pressure every free chunk goes back at once
            bool pressure = under_pressure();
            size_t reserve = pressure ? 0 : config::HeapFreeChunkReserve;
            if (_free_committed <= reserve) {   // the usual case: nothing to return
                return;
            }
            for (auto c : _free_pool_chunks) {
                auto& chunk = _chunks[c];
                if (_free_committed > reserve && chunk.free_committed && (pressure || chunk.free_epoch < previous)) {
                    chunk.free_committed = false;
                    --_free_committed;
                    _decommit_chunk(c);
                }
            }
            for (auto bits = _bin_mask >> _bin_of(PagesPerChunk) << _bin_of(PagesPerChunk); bits; bits &= bits - 1) {
                for (auto& r : _bins[std::countr_zero(bits)]) {
                    auto begin = (r.first + PagesPerChunk - 1) / PagesPerChunk;
                    auto end = (r.first + r.count) / PagesPerChunk;
                    for (auto c = begin; c < end && _free_committed > reserve; ++c) {
                        auto& chunk = _chunks[c];
                        if (chunk.free_committed && (pressure || chunk.free_epoch < previous)) {
                            chunk.free_committed = false;
                            --_free_committed;
                            _decommit_chunk(c);
                        }
                    }
                }
            }
        }

    private:

        static unsigned _bin_of(size_t count) noexcept {
            return std::bit_width(count) - 1;
        }

        void _tag(const Range& r, uint32_t pos) noexcept {
            _tags[r.first] = {(uint32_t)r.count, pos};
            _tags[r.first + r.count - 1] = {(uint32_t)r.count, pos};
        }

        void _push(const Range& r) noexcept {
            auto& bin = _bins[_bin_of(r.count)];
            bin.push_back(r);
            _bin_mask |= uint32_t(1) << _bin_of(r.count);
            _tag(r, (uint32_t)(bin.size() - 1));
        }

        void _remove(const Range& r) noexcept {
            auto b = _bin_of(r.count);
            auto& bin = _bins[b];
            auto pos = _tags[r.first].pos;
            assert(pos < bin.size() && bin[pos].first == r.first);
            _tags[r.first] = {0, 0};
            _tags[r.first + r.count - 1] = {0, 0};
            if (pos + 1 != bin.size()) {
                bin[pos] = bin.back();
                _tag(bin[pos], pos);
            }
            bin.pop_back();
            if (bin.empty()) {
                _bin_mask &= ~(uint32_t(1) << b);
            }
        }

        // Adds a free range, coalesced with a free neighbour on either
        // side; returns the range the new one became part of.
        Range _insert_range(size_t first, size_t count) noexcept {
            Range merged{first, count};
            if (first > 0 && _tags[first - 1].count) {
                Range prev{first - _tags[first - 1].count, _tags[first - 1].count};
                _remove(prev);
                merged.first = prev.first;
                merged.count += prev.count;
            }
            auto next = first + count;
            if (next < _page_count && _tags[next].count) {
                Range after{next, _tags[next].count};
                _remove(after);
                merged.count += after.count;
            }
            _push(merged);
            return merged;
        }

    public:
        inline static HeapGlobals globals;
    private:

        os::Reservation _reservation;
        bool _needs_commit = false;
        size_t _page_count = 0;
        size_t _chunk_count = 0;
        size_t _next_chunk = 0;      // the pools' chunks grow up from here
        size_t _top_chunk = 0;       // the arrays' chunks grow down from here
        std::vector<size_t> _free_pool_chunks;   // entirely free, kept for the pools
        size_t _hint = 0;
        size_t _free_committed = 0;
        uint32_t _epoch = 1;
        Chunk* _chunks = nullptr;
        std::vector<uint64_t> _has_free;
        // Free page ranges outside the pool chunks, binned by size: bin b
        // holds the ranges of 2^b .. 2^(b+1)-1 pages, _bin_mask has a bit
        // per non-empty bin. Every free range tags its first and last page
        // (count and position in its bin), so that a neighbour of a range
        // being freed is found and taken out of its bin in O(1): the bins
        // are unordered, a removal swaps the last range in.
        struct Tag {
            uint32_t count;   // 0: not the boundary of a free range
            uint32_t pos;
        };
        static constexpr unsigned BinCount = 32;
        std::vector<Range> _bins[BinCount];
        uint32_t _bin_mask = 0;
        Tag* _tags = nullptr;
        // read by every page allocation (under_pressure), away from the mutex
        alignas(config::CacheLineSize) std::atomic<size_t> _committed_chunks = {0};
        std::atomic<size_t> _limit = {0};
        alignas(config::CacheLineSize) std::mutex _mutex;
    };
}
