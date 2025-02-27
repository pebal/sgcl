//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_metadata.h"
#include "page.h"
#include "types.h"

namespace sgcl::detail {
    static constexpr size_t PageDataSize = config::PageSize - sizeof(uintptr_t);

    template<class T>
    struct PageInfo {
        using Type = std::remove_cv_t<T>;
        static constexpr size_t ObjectSize = sizeof(std::remove_extent_t<std::conditional_t<std::is_void_v<Type>, char, Type>>);
        static constexpr size_t ObjectCount = std::max(size_t(1), PageDataSize / ObjectSize);
        static constexpr size_t StatesSize = (sizeof(std::atomic<State>) * ObjectCount + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
        static constexpr size_t FlagsCount = (ObjectCount + Page::FlagBitCount - 1) / Page::FlagBitCount;
        static constexpr size_t FlagsSize = sizeof(Page::Flags) * FlagsCount;
        static constexpr size_t HeaderSize = sizeof(Page) + StatesSize + FlagsSize;
        using ObjectAllocator = std::conditional_t<ObjectSize <= PageDataSize, ObjectPoolAllocator<Type>, ObjectAllocator<Type>>;

        static constexpr auto get_destroy_function() -> void(*)(void*) noexcept {
            if constexpr (!std::is_trivially_destructible_v<Type> && std::is_destructible_v<Type>) {
                return &_destroy;
            } else {
                return nullptr;
            }
        }

        inline static void* user_metadata = nullptr;

        inline static auto& private_metadata() {
            static auto metadata = new Metadata((std::remove_extent_t<Type>*)0);
            return *metadata;
        }

        inline static auto& array_metadata() {
            static auto metadata = new ArrayMetadata((std::remove_extent_t<std::conditional_t<std::is_void_v<Type>, char, Type>>*)0);
            return *metadata;
        }

        inline static ChildPointers child_pointers {std::is_base_of_v<ArrayBase, Type> || !MayContainTracked<Type>::value};

    private:
        static void _destroy(void* p) noexcept {
            std::destroy_at((T*)p);
        }
    };
}
