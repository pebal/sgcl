//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "data_page.h"
#include "object_allocator.h"
#include "type_info.h"

namespace sgcl {
    namespace Priv {
        template<class T>
        struct Large_object_allocator : Object_allocator {
            using Type = typename Type_info<T>::type;

            Type* alloc(size_t size) const {
                auto mem = ::operator new(size + sizeof(Type) + sizeof(uintptr_t), std::align_val_t(PageSize));
                auto data = (Type*)((uintptr_t)mem + sizeof(uintptr_t));
                auto hmem = ::operator new(Type_info<T>::HeaderSize);
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
