//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../config.h"
#include "types.h"

#include <cassert>
#include <cstring>

namespace sgcl::detail {
    struct StackPointerAllocator {
#ifdef SGCL_ARCH_X86_64
        static constexpr unsigned MaxStackSize = config::MaxStackSize;
        static constexpr unsigned PageSize = 4096;
#else
        static constexpr unsigned MaxStackSize = config::MaxStackSize / 2;
        static constexpr unsigned PageSize = 2048;
#endif
        static constexpr unsigned PageCount = MaxStackSize / PageSize;

        StackPointerAllocator() noexcept {}

        Pointer* alloc(void* p) noexcept {
#ifdef SGCL_ARCH_X86_64
            auto address = (uintptr_t)p;
#else
            auto address = ((uintptr_t)p / 2) & ~(uintptr_t)(sizeof(RawPointer) - 1);
#endif
            auto offset = address % MaxStackSize;
            assert(offset % sizeof(RawPointer) == 0);
            auto index = offset / PageSize;
            auto used = is_used[index].load(std::memory_order_relaxed);
            if (!used) {
                std::memset((char*)data + index * PageSize, 0, PageSize);
                is_used[index].store(true, std::memory_order_release);
            }
            return (Pointer*)((char*)data + offset);
        }

        std::atomic<bool> is_used[PageCount] = {};
        union {
            RawPointer data[MaxStackSize / sizeof(RawPointer)];
        };
    };
}
