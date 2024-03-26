//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "data_page.h"
#include "object_allocator.h"
#include "page.h"

namespace sgcl {
    namespace Priv {
        template<class T>
        struct Large_object_allocator : Object_allocator {
            T* alloc(size_t size) const {
                auto mem = ::operator new(size + sizeof(T) + sizeof(uintptr_t), std::align_val_t(PageSize));
                auto data = (T*)((uintptr_t)mem + sizeof(uintptr_t));
                auto hmem = ::operator new(Page_info<T>::HeaderSize);
                auto page = new(hmem) Page(nullptr, data);
                *((Page**)mem) = page;
                page->next = pages.load(std::memory_order_relaxed);
                while(!pages.compare_exchange_weak(page->next, page, std::memory_order_release, std::memory_order_relaxed));
                return data;
            }

            static void free(Page* pages) noexcept {
                Page* page = pages;
                while(page) {
                    auto data = (void*)(page->data - sizeof(uintptr_t));
                    ::operator delete(data, std::align_val_t(PageSize));
                    page->is_used = false;
                    page = page->next_empty;
                }
            }
        };
    }
}
