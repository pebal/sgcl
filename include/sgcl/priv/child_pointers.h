//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "../configuration.h"
#include "types.h"

#include <array>
#include <vector>

namespace sgcl {
    namespace Priv {
        struct Child_pointers {
            using Map = std::array<std::atomic<uint8_t>, MaxTypeNumber / sizeof(Pointer) / 8>;
            using Vector = std::vector<ptrdiff_t>;
            Vector* offsets = {nullptr};
            Map map = {};
            std::atomic<bool> final = {false};
        };
    }
}
