//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "../configuration.h"
#include "array_base.h"
#include "child_pointers.h"
#include "tracked.h"

namespace sgcl {
    namespace Priv {
        template<class T>
        void* Clone(const void*);

        struct Array_metadata {
            template<class T>
            Array_metadata(T*)
                : child_pointers(Type_info<T>::child_pointers)
                , destroy(!std::is_trivially_destructible_v<T> ? Array_base::destroy<T> : nullptr)
                , clone(Clone<T[]>)
                , type_info(typeid(T[]))
                , object_size(Type_info<T>::ObjectSize)
                , user_metadata(Type_info<T[]>::user_metadata())
                , tracked_ptrs_only(std::is_base_of_v<Tracked, T>) {
            }

            Child_pointers& child_pointers;
            void (*const destroy)(void*, size_t) noexcept;
            void* (*const clone)(const void*);
            const std::type_info& type_info;
            const size_t object_size;
            metadata*& user_metadata;
            const bool tracked_ptrs_only;
        };
    }
}
