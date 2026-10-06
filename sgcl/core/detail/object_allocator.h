//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../aliases.h"
#include "heap.h"
#include "memory_counters.h"
#include "object_allocator_base.h"
#include "type_info.h"

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

        SGCL_INLINE_HOT ObjectAllocator(std::atomic<Page*>& pages, StackRecord& stack_record)
        : ObjectAllocatorBase(pages, stack_record) {
        }

        // `init` runs on the range before its slot is published, as in
        // object_pool_allocator_base.h. When the heap refuses the range (the
        // commit limit, or no contiguous range), a full collection and one
        // more try; the program ends when that fails too (heap.h:
        // out_of_managed_memory): the other place memory can run out.
        template<class Init>
        ValueType* alloc(size_t size, Init&& init) const noexcept {
            if (os::forked_child.load(std::memory_order_relaxed)) [[unlikely]] {
                os::fail_after_fork("a managed allocation");   // before the heap's copied lock (os.h)
            }
            if constexpr(config::stack_sp != 0) {
                uint64_t seen = _stack_record.epoch.load(std::memory_order_relaxed);
                record_stack(_stack_record, seen);   // a range of pages: the slow path of every variant
            }
            size += sizeof(ValueType);
#if defined(SGCL_ASAN)
            // a redzone of at least 16 bytes past the object, poisoned below
            // with the rest of the range (os.h: SGCL_ASAN)
            auto pages = size <= size_t(-1) - 16 - (config::page_size - 1) ? (size + 16 + config::page_size - 1) / config::page_size : size_t(-1) / config::page_size;   // a size near the end of size_t: more pages than any heap, refused
#else
            auto pages = (size + config::page_size - 1) / config::page_size;
#endif
            auto data = (ValueType*)Heap::instance().alloc_range(pages);
            if (!data) [[unlikely]] {
                data = _alloc_after_collection(pages);
            }
            auto since = MemoryCounters::add_alloc(pages);   // page_allocator.h: the same rule
            bool wake = since * 4 > MemoryCounters::live_after_cycle() + 64 || Heap::instance().under_pressure();
            auto hmem = TypeInfo<T>::header_slab().alloc();
            auto page = new(hmem) Page(data);
            page->page_count = pages;
            // the one slot comes out in state UniqueLock, like a pool slot
#if defined(SGCL_ASAN)
            // the object's bytes addressable (a buffer's: its header and
            // elements), the range's last use left them poisoned; its shadow
            // given back rather than written (os.h: asan::release); the
            // rest of the range poisoned, a redzone of 16 bytes at least
            SGCL_ASAN_RELEASE(data, pages * config::page_size);
            init(data);
            SGCL_ASAN_POISON((char*)data + size, pages * config::page_size - size);
#else
            init(data);
#endif
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

    private:
        SGCL_COLD static ValueType* _alloc_after_collection(size_t pages) noexcept {
            collect_for_allocation();
            auto data = (ValueType*)Heap::instance().alloc_range(pages);
            if (!data) {
                out_of_managed_memory();
            }
            return data;
        }
    };
}
