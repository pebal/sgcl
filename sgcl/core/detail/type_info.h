//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cell_block.h"
#include "frame_word.h"
#include "page_info.h"
#include "tracked.h"
#include "weak_cell.h"

namespace sgcl::detail {
    template<class T>
    struct MayContainTracked {
        static constexpr size_t PointerSize = sizeof(RawPointer);
        using Type = std::remove_extent_t<T>;
        static constexpr auto value = std::is_same_v<Type, Pointer>
                                      || (!std::is_trivially_default_constructible_v<Type>
                                          && sizeof(Type) >= PointerSize
                                          && alignof(Type) >= alignof(RawPointer));
    };

    template<>
    struct MayContainTracked<void> {
        static constexpr auto value = false;
    };

    // A block of cells is pointers only, without a constructor (the trait
    // would say no): traced through its map, which is full and stays so
    template<>
    struct MayContainTracked<CellBlock> {
        static constexpr auto value = true;
    };

    // The word of a weak cell is a pointer the collector must not follow
    template<>
    struct MayContainTracked<WeakCell> {
        static constexpr auto value = false;
    };

    // A conservative type: every word may be a pointer in one object and
    // data in another, so a word found holding data does not remove its
    // offset from the type's pointer map (child_pointers.h). The frames of
    // coroutines (frame_word.h) only.
    template<class T>
    struct Conservative {
        static constexpr auto value = false;
    };

    template<>
    struct Conservative<FrameWord> {
        static constexpr auto value = true;
    };

    template<class T>
    struct TypeInfo : PageInfo<std::remove_cv_t<T>> {
        using Type = std::remove_cv_t<T>;
        using BaseType = std::remove_extent_t<Type>;
        using ValidType = std::conditional_t<std::is_void_v<BaseType>, char, BaseType>;
        static constexpr bool IsTracked = IsTrackedPointer<BaseType>;
        static constexpr bool IsArray = std::is_base_of_v<ArrayBase, BaseType>;
        static constexpr bool MayContainTracked = detail::MayContainTracked<T>::value;
    };
}
