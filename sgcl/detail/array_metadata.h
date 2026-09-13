//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_base.h"

namespace sgcl::detail {
    struct ArrayMetadata {
        template<class T>
        ArrayMetadata(T*) noexcept
        : child_pointers(TypeInfo<T>::child_pointers())
        , type_info(typeid(T[]))
        , object_size(TypeInfo<T>::ObjectSize) {
        }

        ChildPointers& child_pointers;
        const std::type_info& type_info;
        const size_t object_size;
    };
}
