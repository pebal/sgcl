//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../config.h"
#include "types.h"

namespace sgcl::detail {
    struct DataPage {
        union{
            Block* block;
            Page* page;
        };
        union {
            DataPage* next;
            char data[config::PageSize - sizeof(Block*)];
        };
    };
}
