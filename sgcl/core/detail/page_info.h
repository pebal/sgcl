//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_metadata.h"
#include "page.h"
#include "types.h"

namespace sgcl::detail {
    // The whole page holds objects: the header is outside (heap.h).
    static constexpr size_t PageDataSize = config::page_size;

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
        SGCL_INLINE_HOT static constexpr auto get_destroy_function() -> void(*)(void*) noexcept {
            if constexpr (!std::is_trivially_destructible_v<Type> && std::is_destructible_v<Type>) {
                return &_destroy;
            } else {
                return nullptr;
            }
        }

        // Headers of this type come from one slab (page.h: Page::release),
        // the Metadata's. Never destroyed: the collector thread and the
        // exiting threads still return headers while the process runs its
        // static destructors.
        SGCL_INLINE_HOT static HeaderSlab& header_slab() {
            return *private_metadata().header_slab;
        }

        // The type's record (metadata.h: TypeRecord): its constants, its
        // number for the allocators and the slots of its Metadata and its
        // pointer map. constinit, not a function-local static like
        // array_metadata below: a constant (no guard, no dynamic
        // initialization), so a global whose initializer creates the first
        // object of this type in any translation unit finds it whole (the
        // map once was an inline static vector, unordered with the globals
        // of other translation units: pull request #13).
        inline static constinit TypeRecord record = {.constants = Metadata::constants_of<Type>(),
                                                     .may_contain_tracked = MayContainTracked<Type>::value,
                                                     .conservative = Conservative<Type>::value};

        // The type's Metadata (metadata.h), made on first use from the
        // type's constants (data, no code per type) and never freed: the
        // pages of the type point at it. One function for every type
        // (Metadata::of_type), given the record.
        SGCL_INLINE_HOT static Metadata& private_metadata() {
            return Metadata::of_type(PageInfo<std::remove_extent_t<Type>>::record);
        }

        // The ArrayMetadata of buffers of this element type
        // (array_metadata.h)
        inline static auto& array_metadata() {
            static auto metadata = new ArrayMetadata((std::remove_extent_t<std::conditional_t<std::is_void_v<Type>, char, Type>>*)0);
            return *metadata;
        }

        // The type's pointer map, made on first use (ChildPointers::of_type,
        // one function for every type) and never freed, like the Metadata
        // that names it
        SGCL_INLINE_HOT static ChildPointers& child_pointers() {
            return ChildPointers::of_type(record);
        }

    private:
        SGCL_INLINE_HOT static void _destroy(void* p) noexcept {
            std::destroy_at((T*)p);
        }
    };
}
