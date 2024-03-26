//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "types.h"

namespace sgcl {
    namespace Priv {
        struct Object_allocator {
            virtual ~Object_allocator() noexcept = default;
            inline static std::atomic<Page*> pages = {nullptr};
        };
    }
}
