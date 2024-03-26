//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "types.h"

#include <memory>

namespace sgcl {
    namespace Priv {
        struct Array_base {
            Array_base(size_t c) noexcept
                : count(c) {
            }

            template<class T>
            static void destroy(void* data, size_t count) noexcept {
                for (size_t i = count; i > 0; --i) {
                    std::destroy_at((T*)data + i - 1);
                }
            }

            std::atomic<Array_metadata*> metadata = {nullptr};
            const size_t count;
        };
    }
}
