//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "memory_counters.h"
#include "object_allocator_base.h"
#include "type_info.h"
#include <functional>

namespace sgcl::detail {
    template<class T>
    class ObjectAllocator : public ObjectAllocatorBase {
    public:
        using ValueType = typename TypeInfo<T>::Type;
        using IsPoolAllocator = std::false_type;

        ObjectAllocator(std::atomic<Page*>& pages)
        : ObjectAllocatorBase(pages) {
        }

        ValueType* alloc(size_t size) const {
            using WakingUp = std::unique_ptr<Counter, std::function<void(Counter*)>>;
            WakingUp waking_up;
            size += sizeof(ValueType) + sizeof(uintptr_t);
            MemoryCounters::update_alloc(1, size);
            auto alloc_counter = MemoryCounters::alloc_counter();
            auto live_counter = MemoryCounters::live_counter();
            if (alloc_counter * 4 > live_counter + Counter(64, config::PageSize * 4 * 64)) {
                waking_up = WakingUp(&live_counter, [](Counter*){ waking_up_collector(); });
            }
            auto mem = ::operator new(size, std::align_val_t(config::PageSize));
            auto data = (ValueType*)((uintptr_t)mem + sizeof(uintptr_t));
            auto hmem = ::operator new(TypeInfo<T>::HeaderSize);
            auto page = new(hmem) Page(nullptr, data);
            page->alloc_size = size;
            *((Page**)mem) = page;
            page->next = _pages.load(std::memory_order_relaxed);
            _pages.store(page, std::memory_order_release);
            return data;
        }

        static void free(Page* pages) noexcept {
            Page* page = pages;
            size_t count = 0;
            size_t size = 0;
            while(page) {
                auto data = (void*)(page->data - sizeof(uintptr_t));
                ::operator delete(data, std::align_val_t(config::PageSize));
                ++count;
                size += page->alloc_size;
                page->is_used = false;
                page = page->next_empty;
            }
            if (count) {
                MemoryCounters::update_free(count, size);
                auto free_counter = MemoryCounters::alloc_counter();
                auto live_counter = MemoryCounters::live_counter();
                free_counter *= 4;
                live_counter += Counter(4, config::PageSize * 4);
                if ((free_counter.count > live_counter.count)
                || (free_counter.size > live_counter.size)) {
                    waking_up_collector();
                }
            }
        }
    };
}
