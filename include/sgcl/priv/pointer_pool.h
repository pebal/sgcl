//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "pointer_pool_base.h"

namespace sgcl {
    namespace Priv {
        template<unsigned Size, unsigned Offset>
        struct Pointer_pool : Pointer_pool_base {
            constexpr Pointer_pool()
                : Pointer_pool_base(Size, Offset) {
                Pointer_pool_base::_indexes = _indexes.data();
            }
            Pointer_pool(void* data)
                : Pointer_pool() {
                Pointer_pool_base::_indexes = _indexes.data();
                fill(data);
            }

            Pointer_pool* next = nullptr;

        private:
            std::array<void*, Size> _indexes;
        };
    }
}
