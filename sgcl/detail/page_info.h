//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_metadata.h"
#include "page.h"
#include "types.h"

namespace sgcl::detail {
    // The whole page holds objects: the header is outside (heap.h).
    static constexpr size_t PageDataSize = config::PageSize;

    template<class T>
    struct PageInfo {
        using Type = std::remove_cv_t<T>;
        static constexpr size_t ObjectSize = sizeof(std::remove_extent_t<std::conditional_t<std::is_void_v<Type>, char, Type>>);
        static constexpr size_t ObjectCount = std::max(size_t(1), PageDataSize / ObjectSize);
        static constexpr size_t StatesSize = (sizeof(std::atomic<State>) * ObjectCount + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
        static constexpr size_t FlagsCount = (ObjectCount + Page::FlagBitCount - 1) / Page::FlagBitCount;
        static constexpr size_t FlagsSize = sizeof(Page::Flags) * FlagsCount;
        static constexpr size_t SummaryCount = (FlagsCount + 63) / 64;
        static constexpr size_t FreeBitsSize = sizeof(Page::Flag) * FlagsCount;
        static constexpr size_t HeaderSize = sizeof(Page) + StatesSize + FlagsSize + FreeBitsSize + sizeof(uint64_t) * SummaryCount;
        using Allocator = std::conditional_t<ObjectSize <= PageDataSize, ObjectPoolAllocator<Type>, ObjectAllocator<Type>>;

        // The sweep's destroy function for the type, or null when the type
        // needs none (trivially destructible: nothing runs per object)
        static constexpr auto get_destroy_function() -> void(*)(void*) noexcept {
            if constexpr (!std::is_trivially_destructible_v<Type> && std::is_destructible_v<Type>) {
                return &_destroy;
            } else {
                return nullptr;
            }
        }

        // Headers of this type come from one slab (page.h: Page::release).
        // Never destroyed: the collector thread and the exiting threads still
        // return headers while the process runs its static destructors.
        inline static HeaderSlab& header_slab() {
            static auto slab = new HeaderSlab(HeaderSize);
            return *slab;
        }

        // The type's Metadata (metadata.h), made on first use and never
        // freed: the pages of the type point at it
        inline static auto& private_metadata() {
            static auto metadata = new Metadata((std::remove_extent_t<Type>*)0);
            return *metadata;
        }

        // The ArrayMetadata of buffers of this element type
        // (array_metadata.h), likewise
        inline static auto& array_metadata() {
            static auto metadata = new ArrayMetadata((std::remove_extent_t<std::conditional_t<std::is_void_v<Type>, char, Type>>*)0);
            return *metadata;
        }

        // A function-local static like its siblings above, not an inline
        // static data member: the map is a vector, so the member's dynamic
        // initialization would be unordered with respect to the globals of
        // other translation units, and a global whose initializer creates
        // the first object of this type could bind the metadata to a map
        // not yet constructed (reported in pull request #13).
        inline static ChildPointers& child_pointers() {
            static ChildPointers pointers {MayContainTracked<Type>::value, ObjectSize, typeid(Type), Conservative<Type>::value};
            return pointers;
        }

    private:
        static void _destroy(void* p) noexcept {
            std::destroy_at((T*)p);
        }
    };
}
