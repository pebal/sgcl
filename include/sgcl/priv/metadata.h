//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "../configuration.h"
#include "array_base.h"
#include "child_pointers.h"

namespace sgcl {
    namespace Priv {
        template<class T>
        void* Clone(const void*);

        struct Metadata {
            template<class T>
            using Info = Type_info<T>;

            template<class T>
            Metadata(T*)
                : child_pointers(Info<T>::child_pointers)
                , destroy(!std::is_trivially_destructible_v<T> || std::is_base_of_v<Array_base, T> ? Info<T>::destroy : nullptr)
                , free(Info<T>::Object_allocator::free)
                , clone(Clone<T>)
                , object_size(Info<T>::ObjectSize)
                , object_count(Info<T>::ObjectCount)
                , is_array(std::is_base_of_v<Array_base, T>)
                , user_metadata(Info<T>::user_metadata())
                , type_info(typeid(T)) {
            }

            Child_pointers& child_pointers;
            void (*const destroy)(void*) noexcept;
            void (*const free)(Page*);
            void* (*const clone)(const void*);
            const size_t object_size;
            const unsigned object_count;
            bool is_array;
            metadata*& user_metadata;
            const std::type_info& type_info;
            Page* empty_page = {nullptr};
            Metadata* next = {nullptr};
        };
    }
}
