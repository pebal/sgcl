//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"

namespace sgcl::detail {
    class ObjectAllocatorBase {
    public:
        ObjectAllocatorBase() {
            collector_init();
        }

        virtual ~ObjectAllocatorBase() noexcept = default;
        inline static std::atomic<Page*> pages = {nullptr};
    };
}
