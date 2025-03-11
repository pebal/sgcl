//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/type_info.h"

namespace sgcl {
    template<class T>
    static constexpr bool IsTracked = detail::TypeInfo<T>::IsTracked;

    template<typename T>
    concept IsPointer = requires(T t) {
        { t.clone() };
        { t.get() };
        { t.object_size() };
    };

    template <typename T>
    concept TrackedPointer = IsTracked<T>;
}
