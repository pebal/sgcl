//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "pointer_pool_base.h"

namespace sgcl::detail {
    template<unsigned Size, unsigned Offset>
    class PointerPool : public PointerPoolBase {
    public:
        constexpr PointerPool() noexcept
        : PointerPoolBase(_indexes, Size, Offset) {
        }

        PointerPool(void* data) noexcept
        : PointerPool() {
            fill(data);
        }

        PointerPool* next = nullptr;

    private:
        void* _indexes[Size];
    };
}
