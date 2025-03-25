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
        using Map = std::vector<std::atomic<uint8_t>>;
        using Vector = std::vector<ptrdiff_t>;

        constexpr ChildPointers(bool f, size_t object_size) noexcept
        : final(f)
        , map(f ? 0 : (object_size + sizeof(RawPointer) * 8 - 1) / (sizeof(RawPointer) * 8)) {
        };

        std::unique_ptr<Vector> offsets;
        std::atomic<bool> final;
        Map map;
    };
}
