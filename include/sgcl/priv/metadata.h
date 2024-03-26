//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "../configuration.h"
#include "array_base.h"
#include "child_pointers.h"
#include "unique_ptr.h"

namespace sgcl {
    namespace Priv {
        template<class T>
        Unique_ptr<void> Clone(const void*);

        struct Metadata {
            template<class T>
            Metadata(T*)
                : child_pointers(Page_info<T>::child_pointers)
                , destroy(!std::is_trivially_destructible_v<T> || std::is_base_of_v<Array_base, T> ? Page_info<T>::destroy : nullptr)
                , free(Page_info<T>::Object_allocator::free)
                , clone(Clone<T>)
                , object_size(Page_info<T>::ObjectSize)
                , object_count(Page_info<T>::ObjectCount)
                , is_array(std::is_base_of_v<Array_base, T>)
                , user_metadata(Page_info<T>::user_metadata())
                , type_info(typeid(T)) {
            }

            Child_pointers& child_pointers;
            void (*const destroy)(void*) noexcept;
            void (*const free)(Page*);
            Unique_ptr<void> (*const clone)(const void*);
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
