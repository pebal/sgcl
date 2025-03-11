//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_base.h"
#include "child_pointers.h"

namespace sgcl::detail {
    template<class T>
    void* clone_object(const void*);

    struct Metadata {
        template<class T>
        Metadata(T*) noexcept
        : child_pointers(TypeInfo<T>::child_pointers)
        , destroy(TypeInfo<T>::get_destroy_function())
        , free(TypeInfo<T>::Allocator::free)
        , clone(clone_object<T>)
        , object_size(TypeInfo<T>::ObjectSize)
        , object_count(TypeInfo<T>::ObjectCount)
        , is_array(TypeInfo<T>::IsArray)
        , user_metadata(TypeInfo<T>::user_metadata)
        , type_info(typeid(T)) {
        }

        ChildPointers& child_pointers;
        void (*const destroy)(void*) noexcept;
        void (*const free)(Page*) noexcept;
        void* (*const clone)(const void*);
        const size_t object_size;
        const unsigned object_count;
        const bool is_array;
        void*& user_metadata;
        const std::type_info& type_info;
        Page* empty_page = {nullptr};
        Metadata* next = {nullptr};
        bool used = {false};
    };
}
