//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "types.h"

namespace sgcl {
    namespace Priv {
        static constexpr size_t PageSize = 4096;
        static constexpr size_t PageDataSize = PageSize - sizeof(uintptr_t);

        struct DataPage {
            union{
                Block* block;
                Page* page;
            };
            union {
                DataPage* next;
                char data[PageSize - sizeof(Block*)];
            };
        };
    }
}
