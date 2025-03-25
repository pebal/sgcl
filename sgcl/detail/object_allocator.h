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
#include <thread>

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
            size_t size = 0;
            std::vector<void*> mems_to_delete;
            while(page) {
                auto data = (void*)(page->data - sizeof(uintptr_t));
                mems_to_delete.push_back(data);
                size += page->alloc_size;
                page->is_used = false;
                page = page->next_empty;
            }
            if (mems_to_delete.size()) {
                MemoryCounters::update_free(mems_to_delete.size(), size);
                auto alloc_counter = MemoryCounters::last_alloc_counter();
                auto free_counter = Counter(mems_to_delete.size(), size);
                if (free_counter * 4 > alloc_counter + Counter(64, config::PageSize * 4 * 64)) {
                    force_short_sleep();
                }
                destroyer_threads().emplace_back(
                    std::thread([mems = std::move(mems_to_delete)] {
                        for (auto mem: mems) {
                            ::operator delete(mem, std::align_val_t(config::PageSize));
                        }
                    })
                );
            }
        }
    };
}
