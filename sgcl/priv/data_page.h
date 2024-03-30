//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "types.h"

namespace sgcl {
    namespace Priv {
        static constexpr size_t PageSize = 4096;
        static constexpr size_t PageDataSize = PageSize - sizeof(uintptr_t);

        struct Data_page {
            union{
                Block* block;
                Page* page;
            };
            union {
                Data_page* next;
                char data[PageSize - sizeof(Block*)];
            };
        };
    }
}
