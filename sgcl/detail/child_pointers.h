//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../config.h"
#include "types.h"

#include <array>
#include <atomic>
#include <vector>

namespace sgcl::detail {
    struct ChildPointers {
        using Map = std::array<std::atomic<uint8_t>, config::PageSize / sizeof(RawPointer) / 8>;
        using Vector = std::vector<ptrdiff_t>;

        constexpr ChildPointers(bool f) noexcept
        : final(f) {
        };

        Vector* offsets = {nullptr};
        Map map = {};
        std::atomic<bool> final;
    };
}
