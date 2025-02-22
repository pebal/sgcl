//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"

#include <memory>

namespace sgcl::detail {
    struct ArrayBase {
        constexpr ArrayBase(size_t c) noexcept
        : count(c) {
        }

        template<class T>
        static void destroy(void* data, size_t count) noexcept {
            for (size_t i = count; i > 0; --i) {
                std::destroy_at((T*)data + i - 1);
            }
        }

        std::atomic<ArrayMetadata*> metadata = {nullptr};
        const size_t count;
    };
}
