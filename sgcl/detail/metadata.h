//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_base.h"
#include "child_pointers.h"
#include "header_slab.h"

namespace sgcl::detail {
    // What every page of one type shares, one per type (page_info.h:
    // private_metadata): the type's pointer map, its destroy and free
    // functions, the slab of its page headers, the size and count of its
    // objects, and the kinds the collector treats apart. The last three
    // fields are the collector's: the list of the type's pages emptied by
    // a cycle, on the way back to the allocators (collector.h:
    // _release_unused_pages).
    struct Metadata {
        template<class T>
        Metadata(T*) noexcept
        : child_pointers(TypeInfo<T>::child_pointers())
        , destroy(TypeInfo<T>::get_destroy_function())
        , free(TypeInfo<T>::Allocator::free)
        , header_slab(&TypeInfo<T>::header_slab())
        , object_size(TypeInfo<T>::ObjectSize)
        , object_count(TypeInfo<T>::ObjectCount)
        , is_array(TypeInfo<T>::IsArray)
        , pool_allocated(TypeInfo<T>::Allocator::IsPoolAllocator::value)
        , is_weak_cell(std::is_same_v<std::remove_cv_t<T>, WeakCell>)
        , is_cell_block(std::is_same_v<std::remove_cv_t<T>, CellBlock>)
        , type_info(typeid(T)) {
        }

        ChildPointers& child_pointers;
        void (*const destroy)(void*) noexcept;
        void (*const free)(Page*) noexcept;
        HeaderSlab* const header_slab;
        const size_t object_size;
        const unsigned object_count;
        const bool is_array;
        const bool pool_allocated;   // slots with a free bitmap, not page ranges
        const bool is_weak_cell;     // weak_cell.h: the pages the weak phase visits
        const bool is_cell_block;    // cell_block.h: the pages of the blocks of cells, traced without a map and released by state
        const std::type_info& type_info;
        Page* empty_page = {nullptr};
        Metadata* next = {nullptr};
        bool used = {false};
    };
}
