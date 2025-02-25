//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "memory_counters.h"
#include "object_allocator_base.h"
#include "type_info.h"

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
            size += sizeof(ValueType) + sizeof(uintptr_t);
            auto mem = ::operator new(size, std::align_val_t(config::PageSize));
            auto data = (ValueType*)((uintptr_t)mem + sizeof(uintptr_t));
            auto hmem = ::operator new(TypeInfo<T>::HeaderSize);
            auto page = new(hmem) Page(nullptr, data);
            page->alloc_size = size;
            *((Page**)mem) = page;
            page->next = _pages.load(std::memory_order_relaxed);
            _pages.store(page, std::memory_order_release);
            MemoryCounters::update_alloc(size);
            return data;
        }

        static void free(Page* pages) noexcept {
            Page* page = pages;
            while(page) {
                auto data = (void*)(page->data - sizeof(uintptr_t));
                ::operator delete(data, std::align_val_t(config::PageSize));
                MemoryCounters::update_free(page->alloc_size);
                page->is_used = false;
                page = page->next_empty;
            }
        }
    };
}
