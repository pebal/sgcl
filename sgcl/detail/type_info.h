//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "page_info.h"

namespace sgcl::detail {
    template<class T>
    struct TypeInfo : PageInfo<std::remove_cv_t<T>> {
        using Type = std::remove_cv_t<T>;
    };

    template<class T>
    struct MayContainTracked {
        using Type = std::remove_extent_t<T>;
        static constexpr auto value = !std::is_trivial_v<Type> && sizeof(Type) >= sizeof(RawPointer);
    };

    template<>
    struct MayContainTracked<void> {
        static constexpr auto value = false;
    };

    template <class T>
    inline constexpr bool may_contain_tracked = MayContainTracked<T>::value;
}
