//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../config.h"
#include "data_page.h"
#include <new>

namespace sgcl::detail {
    struct Block {
        static constexpr size_t PageCount = 15;

        Block() noexcept {
            DataPage* data = (DataPage*)(this + 1);
            for (size_t i = 0; i < PageCount; ++i) {
                data[i].block = this;
            }
        }

        static void* operator new(size_t) {
            constexpr size_t size = sizeof(void*) + sizeof(Block) + sizeof(DataPage) * (PageCount + 1);
            void* mem = malloc(size);
            if (!mem) {
                throw std::bad_alloc();
            }
            uintptr_t addres = (uintptr_t)mem + sizeof(void*) + sizeof(Block) + config::PageSize;
            addres = addres & ~(config::PageSize - 1);
            Block* block = (Block*)addres - 1;
            *((void**)block - 1) = mem;
            return block;
        }

        static void operator delete(void* p, size_t) noexcept {
            free(*((void**)p - 1));
        }

        Block* next = {nullptr};
        unsigned page_count = {0};
    };
}
