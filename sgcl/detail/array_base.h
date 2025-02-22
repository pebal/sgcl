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

        template<typename T>
        static constexpr auto get_destroy_function() -> void(*)(void*, size_t) noexcept {
            if constexpr (!std::is_trivially_destructible_v<T> && std::is_destructible_v<T>) {
                return &_destroy<T>;
            } else {
                return nullptr;
            }
        }

        std::atomic<ArrayMetadata*> metadata = {nullptr};
        const size_t count;

    private:
        template<class T>
        static void _destroy(void* data, size_t count) noexcept {
            for (size_t i = count; i > 0; --i) {
                std::destroy_at((T*)data + i - 1);
            }
        }
    };
}
