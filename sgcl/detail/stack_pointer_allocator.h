//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../config.h"
#include "types.h"

#include <cassert>

namespace sgcl::detail {
    struct StackPointerAllocator {
        static constexpr unsigned PageCount = config::MaxStackSize / config::PageSize;

        StackPointerAllocator() noexcept {}

        Pointer* alloc(void* p) noexcept {
            auto index = ((uintptr_t)p / config::PageSize) % PageCount;
            auto used = is_used[index].load(std::memory_order_relaxed);
            if (!used) {
                std::memset((char*)data + index * config::PageSize, 0, config::PageSize);
                is_used[index].store(true, std::memory_order_release);
            }
            auto offset = ((uintptr_t)p % config::MaxStackSize);
            assert(offset % sizeof(RawPointer) == 0);
            return (Pointer*)((char*)data + offset);
        }

        std::atomic<bool> is_used[PageCount] = {};
        union {
            RawPointer data[config::MaxStackSize / sizeof(RawPointer)];
        };
    };
}
