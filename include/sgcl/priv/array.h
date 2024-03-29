//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "array_base.h"
#include "page_info.h"

namespace sgcl {
    namespace Priv {
        template<size_t Size = 1>
        struct Array : Array_base {
            constexpr Array(size_t c) noexcept
            : Array_base(c) {
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
        struct Page_info<Array<>> : public Page_info<Array<PageDataSize>> {
            using Object_allocator = Large_object_allocator<Array<>>;

            static void destroy(void* p) noexcept {
                std::destroy_at((Array<>*)p);
            }
        };
    }
}
