//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "../configuration.h"
#include "array_metadata.h"
#include "data_page.h"
#include "page.h"
#include "types.h"

namespace sgcl {   
    namespace Priv {
        inline static metadata void_mdata;

        template<class T>
        struct Page_info {
            static constexpr size_t ObjectSize = sizeof(std::remove_extent_t<std::conditional_t<std::is_same_v<std::remove_cv_t<T>, void>, char, T>>);
            static constexpr size_t ObjectCount = std::max(size_t(1), PageDataSize / ObjectSize);
            static constexpr size_t StatesSize = (sizeof(std::atomic<State>) * ObjectCount + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
            static constexpr size_t FlagsCount = (ObjectCount + Page::FlagBitCount - 1) / Page::FlagBitCount;
            static constexpr size_t FlagsSize = sizeof(Page::Flags) * FlagsCount;
            static constexpr size_t HeaderSize = sizeof(Page) + StatesSize + FlagsSize;
            using Object_allocator = std::conditional_t<ObjectSize <= PageDataSize, Small_object_allocator<T>, Large_object_allocator<T>>;

            static void destroy(void* p) noexcept {
                std::destroy_at((T*)p);
            }

            inline static metadata* mdata_ptr = &void_mdata;

            inline static metadata*& user_metadata() {
                return mdata_ptr;
            }

            inline static void set_user_metadata(metadata* mdata) {
                mdata_ptr = mdata ? mdata : &void_mdata;
            }

            inline static auto& private_metadata() {
                static auto metadata = new Metadata((std::remove_extent_t<T>*)0);
                return *metadata;
            }

            inline static auto& array_metadata() {
                static auto metadata = new Array_metadata((std::remove_extent_t<std::conditional_t<std::is_same_v<std::remove_cv_t<T>, void>, char, T>>*)0);
                return *metadata;
            }

            inline static Child_pointers child_pointers;
        };
    }
}
