//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// logging level [0-3]
#define SGCL_LOG_PRINT_LEVEL 0

#include <chrono>
#include <cstddef>

namespace sgcl::config {
    static constexpr auto MaxSleepTime = std::chrono::seconds(30);
    static constexpr size_t PageSize = 0x10000;
    static constexpr ptrdiff_t MaxStackSize = 0x400000;
    static constexpr size_t MaxTypesNumber = 4096;

    static_assert(PageSize <= 0x10000, "PageSize is too large");
    static_assert((PageSize & (PageSize - 1)) == 0, "PageSize is not a power of 2");
    static_assert((MaxStackSize & (MaxStackSize - 1)) == 0, "MaxStackSize is not a power of 2");
}
