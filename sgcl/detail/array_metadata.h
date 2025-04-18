//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_base.h"

namespace sgcl::detail {
    struct ArrayMetadata {
        template<class T>
        ArrayMetadata(T*) noexcept
        : child_pointers(TypeInfo<T>::child_pointers)
        , destroy(ArrayBase::get_destroy_function<T>())
        , type_info(typeid(T[]))
        , object_size(TypeInfo<T>::ObjectSize)
        , user_metadata(TypeInfo<T[]>::user_metadata)
        , tracked_ptrs_only(TypeInfo<T>::IsTracked) {
        }

        ChildPointers& child_pointers;
        void (*const destroy)(void*, size_t) noexcept;
        const std::type_info& type_info;
        const size_t object_size;
        void*& user_metadata;
        const bool tracked_ptrs_only;
    };
}
