//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// logging level [0-3]
#define SGCL_LOG_PRINT_LEVEL 0

//------------------------------------------------------------------------------
// Reduces memory usage on x86-64 platforms by using two highest bits of pointer
// Warning!
// Note: User heap must be allocated in the low half of virtual address space
//------------------------------------------------------------------------------
#if defined(__x86_64__) || defined(_M_X64)
#define SGCL_ARCH_X86_64
#endif

#include <chrono>
#include <cstddef>

namespace sgcl::config {
    [[maybe_unused]] static constexpr auto MaxSleepTime = std::chrono::seconds(30);
                     static constexpr size_t PageSize = 0x10000;
                     static constexpr size_t MaxStackSize = 0x400000;
    [[maybe_unused]] static constexpr size_t MaxOffsetForStackDetection = 0x1000;
    [[maybe_unused]] static constexpr size_t MaxTypesNumber = 4096;

    static_assert(PageSize <= 0x10000, "PageSize is too large");
    static_assert((PageSize & (PageSize - 1)) == 0, "PageSize is not a power of 2");
    static_assert((MaxStackSize & (MaxStackSize - 1)) == 0, "MaxStackSize is not a power of 2");
}
