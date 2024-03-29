//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "../configuration.h"
#include "data_page.h"

#include <array>

namespace sgcl {
    namespace Priv {
        struct Stack_roots_allocator {
            static constexpr unsigned PageCount = MaxStackSize / PageSize;
            static constexpr size_t PointerCount = PageSize / sizeof(Pointer);
            using Page = std::array<Pointer, PointerCount>;

            ~Stack_roots_allocator() noexcept {
                for (auto& page : pages) {
                    delete page.load(std::memory_order_relaxed);
                }
            }

            Tracked_ptr* alloc(void* p) {
                auto index = ((uintptr_t)p / PageSize) % PageCount;
                auto page = pages[index].load(std::memory_order_relaxed);
                if (!page) {
                    page = new Page{};
                    pages[index].store(page, std::memory_order_release);
                }
                auto offset = ((uintptr_t)p % PageSize) / sizeof(Pointer);
                return (Tracked_ptr*)&(*page)[offset];
            }

            std::atomic<Page*> pages[PageCount] = {};
        };
    }
}
