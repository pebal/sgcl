//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_base.h"

namespace sgcl::detail {
    // What a buffer's header names about its elements (array_base.h): the
    // element type's pointer map, for the marking of the elements, its
    // type, for the statistics, and its size, for the stride. One per
    // element type (page_info.h: array_metadata).
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
