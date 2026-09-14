//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "memory_counters.h"
#include "merge_sort.h"
#include "object_allocator_base.h"

#include <cstring>
#include "page.h"
#include "states.h"
#include "page_allocator.h"

#include <bit>
#include <mutex>
#include <vector>

namespace sgcl::detail {
    // Objects of one type up to a page in size. Slots come from the free
    // bitmap in the page header: while a thread owns a page (page->owned) it
    // is the only writer of that bitmap, so allocation is two countr_zero and
    // a bit clear with no atomic operation. The collector records freed slots
    // in the states and rebuilds the bitmap only for pages nobody owns, right
    // before putting them into the per-type buffer.
    class ObjectPoolAllocatorBase : public ObjectAllocatorBase {
    public:
        // Rebuilds the free bitmap and the summary of a page from its states;
        // true when every slot is free. Collector only, on a page no mutator
        // owns (parallel over pages in _release_unused_pages).
        static bool rebuild_free_bitmap(Page* page) noexcept {
            assert(!page->owned.load(std::memory_order_acquire));
            auto states = page->states();
            auto free_bits = page->free_bits();
            auto count = page->flags_count();
            auto objects = page->object_count;
            auto summary = page->summary();
            std::memset(summary, 0, sizeof(uint64_t) * page->summary_count());
            unsigned free_count = 0;
            for (unsigned w = 0; w < count; ++w) {
                Page::Flag free = 0;
                auto base = w * Page::FlagBitCount;
                auto limit = std::min<unsigned>(Page::FlagBitCount, objects - base);
                // eight states per step (states.h); the bytes past the last
                // object lie in the rounded states area and are masked off
                for (unsigned b = 0; b < limit; b += 8) {
                    free |= Page::Flag(States8::free(States8::load(states + base + b))) << b;
                }
                if (limit < Page::FlagBitCount) {
                    free &= (Page::Flag(1) << limit) - 1;
                }
                free_bits[w] = free;
                if (free) {
                    summary[w / 64] |= uint64_t(1) << (w % 64);
                    free_count += std::popcount(free);
                }
            }
            page->unused_counter_gc = 0;
            page->all_free = free_count == objects;
            return page->all_free;
        }

        ObjectPoolAllocatorBase(PageAllocator& pa, std::atomic<Page*>& pages, std::atomic<Page*>& pb) noexcept
        : ObjectAllocatorBase(pages)
        , _page_allocator(pa)
        , _pages_buffer(pb) {
        }

        ~ObjectPoolAllocatorBase() noexcept override {
            if (_current_page) {
                // write the cached word back; the slots still free stay free
                // for the collector to pick up once the page has enough of them
                _free_bits[_cursor] = _free_word;
                _current_page->owned.store(false, std::memory_order_release);
            }
        }

        // Hot path: the current bitmap word and the address of its first slot
        // live in this object (the owning thread is the only writer of the
        // page's bitmap, so the page copy is refreshed only when the word is
        // exhausted or the page is given up).
        // The slot comes out in state UniqueLock, with the epoch's parity
        // (page.h: unique_state), and with the page's
        // object_created flag raised (set once per page, unless the
        // collector lowered it since): the maker need not look the page up.
        // `init` runs on the slot before the state is stored: the slot is
        // still free then, invisible to the collector (a free slot is
        // unregistered, and no scan, hazard or map reaches one), so the
        // maker writes a buffer's header with plain stores (an object's
        // slot needs nothing: maker.h, _init).
        // The store of the state is a release: the collector reads the
        // states relaxed and fences (acquire) before it reads the words of
        // the objects it registered, which orders the init before those
        // reads. What the constructor writes after this call is what the
        // collector may still see half done: zero or the final value.
        template<class Init>
        void* alloc(size_t, Init&& init) {
            auto free = _free_word;
            if (!free) {
                _refill();
                free = _free_word;
            }
            auto bit = std::countr_zero(free);
            _free_word = free & (free - 1);
            auto p = (void*)(_word_base + bit * _object_size);
            init(p);
            _word_states[bit].store(Page::unique_state(), std::memory_order_release);
            if (!_current_page->object_created.load(std::memory_order_relaxed)) {
                _current_page->object_created.store(true, std::memory_order_release);
            }
            return p;
        }

    private:
        // what alloc() touches first, on one line
        Page::Flag _free_word = 0;
        uintptr_t _word_base = 0;
        std::atomic<State>* _word_states = nullptr;   // the states of the current word's slots
        size_t _object_size = 0;
        Page* _current_page = {nullptr};
        Page::Flag* _free_bits = {nullptr};
        uint64_t* _summary = {nullptr};
        unsigned _summary_count = 0;
        unsigned _cursor = 0;
        PageAllocator& _page_allocator;
        std::atomic<Page*>& _pages_buffer;

        // Guards every per-type page buffer. Taken only when a thread has run
        // out of slots in its current page.
        inline static std::mutex _buffers_mutex;

        virtual Page* _create_page_parameters(void*) = 0;

        // Loads a word with a free slot: the next one of the current page,
        // or the first of the next page.
        void _refill() {
            if (_current_page) {
                // the cached word is exhausted: reflect it in the page
                _free_bits[_cursor] = 0;
                _summary[_cursor / 64] &= ~(uint64_t(1) << (_cursor % 64));
                if (_next_cursor()) {
                    _load_word();
                    return;
                }
                _current_page->owned.store(false, std::memory_order_release);
            }
            auto page = _next_page();
            _current_page = page;
            _free_bits = page->free_bits();
            _summary = page->summary();
            _summary_count = page->summary_count();
            _object_size = page->metadata->object_size;
            _cursor = 0;
            _next_cursor();
            _load_word();
        }

        // Moves the cursor to the next Flags word with a free slot (the
        // summary has a bit per word, kept exact by both writers).
        bool _next_cursor() noexcept {
            for (unsigned w = 0; w < _summary_count; ++w) {
                if (_summary[w]) {
                    _cursor = w * 64 + std::countr_zero(_summary[w]);
                    return true;
                }
            }
            return false;
        }

        void _load_word() noexcept {
            _free_word = _free_bits[_cursor];
            _word_base = _current_page->data + (uintptr_t)_cursor * Page::FlagBitCount * _object_size;
            _word_states = _current_page->states() + (size_t)_cursor * Page::FlagBitCount;
        }

        Page* _next_page() {
            if (os::forked_child.load(std::memory_order_relaxed)) [[unlikely]] {
                os::fail_after_fork("a managed allocation");   // before the copied locks (os.h)
            }
            Page* page;
            {
                // Slow path (once per page): a mutex instead of a lock-free
                // pop, which read page->next_empty of a page another thread
                // may already have popped and filled with objects (ABA).
                std::lock_guard<std::mutex> lock(_buffers_mutex);
                page = _pages_buffer.load(std::memory_order_relaxed);
                if (page) {
                    _pages_buffer.store(page->next_empty, std::memory_order_relaxed);
                }
            }
            if (page) {
                // owned before on_empty_list: the collector reads them in the
                // opposite order, so it never sees a buffered page as loose
                page->owned.store(true, std::memory_order_release);
                page->on_empty_list.store(false, std::memory_order_release);
                return page;
            }
            auto data = _page_allocator.alloc();
            page = _create_page_parameters(data);   // all slots free
            page->owned.store(true, std::memory_order_relaxed);
            Heap::set_pages(data, 1, page);
            // publish: the collector may exchange the list away at any time
            page->next = _pages.load(std::memory_order_relaxed);
            while (!_pages.compare_exchange_weak(page->next, page, std::memory_order_release, std::memory_order_relaxed)) {
            }
            return page;
        }

    protected:
        // GC thread. Rebuilds the free bitmap of every page on the list from
        // its states; entirely free pages go to `empty_pages`, the others
        // stay on `pages` for the buffer. Owned pages are skipped.
        // Moves the entirely free pages of `pages` to `empty_pages`; with
        // `rebuild` the bitmaps are rebuilt here, otherwise the collector
        // already did it (all_free).
        static void _remove_empty(Page*& pages, Page*& empty_pages, bool rebuild) noexcept {
            auto page = pages;
            Page* prev = nullptr;
            while(page) {
                auto next = page->next_empty;
                bool empty = rebuild ? rebuild_free_bitmap(page) : page->all_free;
                if (empty) {
                    page->next_empty = empty_pages;
                    empty_pages = page;
                    if (!prev) {
                        pages = next;
                    } else {
                        prev->next_empty = next;
                    }
                } else {
                    prev = page;
                }
                page = next;
            }
        }

        // GC thread: returns the pages of entirely empty headers to the heap.
        // The headers stay valid until the collector unlinks and deletes them.
        // GC thread. The page is zeroed here, before it goes back to the
        // heap: the next type it is issued to constructs its objects on a
        // page of zeros, as on a page fresh from the heap (maker.h: _init).
        static void _free(Page* pages) noexcept {
            std::vector<void*> data;
            for (auto page = pages; page; page = page->next_empty) {
                std::memset((void*)page->data, 0, config::PageSize);
                data.push_back((void*)page->data);
                page->is_used = false;
            }
            if (data.size()) {
                Heap::instance().free_pages(data.data(), (unsigned)data.size());
                MemoryCounters::add_free(data.size());
                if (data.size() * 4 > MemoryCounters::last_alloc() + 64) {
                    force_short_sleep();
                }
            }
        }

        // GC thread: `pages` are the collector's (bitmaps rebuilt already,
        // possibly on several threads), the buffer's own pages are rebuilt
        // here under the lock, since the collector freed more of their slots
        // while they waited.
        static void _free(Page* pages, std::atomic<Page*>& pages_buffer) noexcept {
            Page* empty_pages = nullptr;
            _remove_empty(pages, empty_pages, false);
            {
                std::lock_guard<std::mutex> lock(_buffers_mutex);
                auto waiting = pages_buffer.load(std::memory_order_relaxed);
                _remove_empty(waiting, empty_pages, true);
                if (pages) {
                    auto last = pages;
                    while(last->next_empty) {
                        last = last->next_empty;
                    }
                    last->next_empty = waiting;
                } else {
                    pages = waiting;
                }
                pages = merge_sort<&Page::next_empty>(pages);
                pages_buffer.store(pages, std::memory_order_relaxed);
            }
            if (empty_pages) {
                _free(empty_pages);
            }
        }
    };
}
