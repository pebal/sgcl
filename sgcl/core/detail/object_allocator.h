//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../aliases.h"
#include "heap.h"
#include "memory_counters.h"
#include "object_allocator_base.h"
#include "type_info.h"

#include <new>

namespace sgcl::detail {
    // Objects larger than a page: each one gets its own range of pages and its
    // own header; every page of the range maps to that header in the heap
    // table, so interior pointers anywhere in the object resolve correctly.
    template<class T>
    class ObjectAllocator
    : public ObjectAllocatorBase {
    public:
        using ValueType = typename TypeInfo<T>::Type;
        using IsPoolAllocator = std::false_type;

        ObjectAllocator(std::atomic<Page*>& pages)
        : ObjectAllocatorBase(pages) {
        }

        // `init` runs on the range before its slot is published, as in
        // object_pool_allocator_base.h.
        template<class Init>
        ValueType* alloc(size_t size, Init&& init) const {
            if (os::forked_child.load(std::memory_order_relaxed)) [[unlikely]] {
                os::fail_after_fork("a managed allocation");   // before the heap's copied lock (os.h)
            }
            size += sizeof(ValueType);
            auto pages = (size + config::page_size - 1) / config::page_size;
            auto since = MemoryCounters::add_alloc(pages);   // page_allocator.h: the same rule
            bool wake = since * 4 > MemoryCounters::live_after_cycle() + 64;
            auto data = (ValueType*)Heap::instance().alloc_range(pages);
            if (!data) {
                // at the commit limit (or no contiguous range): a full
                // collection may free enough; otherwise give up cleanly
                collect_before_bad_alloc();
                data = (ValueType*)Heap::instance().alloc_range(pages);
                if (!data) {
                    throw bad_alloc();
                }
            }
            wake = wake || Heap::instance().under_pressure();
            auto hmem = TypeInfo<T>::header_slab().alloc();
            auto page = new(hmem) Page(data);
            page->page_count = pages;
            // the one slot comes out in state UniqueLock, like a pool slot
            init(data);
            page->states()[0].store(Page::unique_state(), std::memory_order_release);
            page->object_created.store(true, std::memory_order_release);
            Heap::set_pages(data, pages, page);
            // publish: the collector may exchange the list away at any time
            page->next = _pages.load(std::memory_order_relaxed);
            while (!_pages.compare_exchange_weak(page->next, page, std::memory_order_release, std::memory_order_relaxed)) {
            }
            if (wake) {
                waking_up_collector();
            }
            return data;
        }

        // GC thread. The headers stay valid until the collector unlinks and
        // deletes them; only the pages go back to the heap here.
        static void free(Page* pages) noexcept {
            size_t count = 0;
            for (auto page = pages; page; page = page->next_empty) {
                Heap::instance().free_range((void*)page->data, page->page_count);
                count += page->page_count;
                page->is_used = false;
            }
            if (count) {
                MemoryCounters::add_free(count);
                if (count * 4 > MemoryCounters::last_alloc() + 64) {
                    force_short_sleep();
                }
            }
        }
    };
}
