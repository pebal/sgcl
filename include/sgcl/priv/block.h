//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "data_page.h"

namespace sgcl {
    namespace Priv {
        struct Block {
            static constexpr size_t PageCount = 15;

            Block() noexcept {
                Data_page* data = (Data_page*)(this + 1);
                for (size_t i = 0; i < PageCount; ++i) {
                    data[i].block = this;
                }
            }

            static void* operator new(size_t) {
                auto size = sizeof(void*) + sizeof(Block) + sizeof(Data_page) * (PageCount + 1);
                void* mem = ::operator new(size);
                uintptr_t addres = (uintptr_t)mem + sizeof(void*) + sizeof(Block) + PageSize;
                addres = addres & ~(PageSize - 1);
                Block* block = (Block*)addres - 1;
                *((void**)block - 1) = mem;
                return block;
            }

            static void operator delete(void* p, size_t) noexcept {
                ::operator delete(*((void**)p - 1));
            }

            Block* next = {nullptr};
            unsigned page_count = {0};
        };
    }
}
