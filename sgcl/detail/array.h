//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_base.h"
#include "page_info.h"

namespace sgcl::detail {
    template<size_t Size = 1>
    struct Array : ArrayBase {
        constexpr Array(size_t c) noexcept
        : ArrayBase(c) {
        }

        ~Array() noexcept {
            auto metadata = this->metadata.load(std::memory_order_acquire);
            if (metadata && metadata->destroy) {
                metadata->destroy(data, count);
            }
        }

        char data[Size];
    };

    template<>
    struct PageInfo<Array<>> : public PageInfo<Array<PageDataSize>> {
        using ObjectAllocator = ObjectAllocator<Array<>>;

        static void destroy(void* p) noexcept {
            std::destroy_at((Array<>*)p);
        }
    };
}
