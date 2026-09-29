//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_base.h"
#include "page_info.h"

#include <cstddef>
#include <type_traits>

namespace sgcl::detail {
    // Trivially destructible on purpose: a page of buffers has no destroy
    // function, the collector frees a dead buffer without looking inside.
    template<size_t Size = 1>
    struct Array : ArrayBase {
        char data[Size];
    };

    // The elements must start at sizeof(ArrayBase): the base has no tail
    // padding a derived class could place `data` in.
    static_assert(sizeof(Array<16>) == sizeof(ArrayBase) + 16, "the elements must start at sizeof(ArrayBase)");
    static_assert(std::is_trivially_destructible_v<Array<>>, "a buffer must have no destructor");

    // A buffer past a page (maker.h: _make_array): the page layout of one
    // object, as for Array<PageDataSize>, and the allocator of Array<> itself,
    // which adds sizeof(Array<>) to the bytes asked for past it, the maker's
    // arithmetic. Under the name TypeInfo reads (Allocator): the allocator
    // of Array<PageDataSize>, inherited when the alias had another name,
    // added its 65 552 bytes instead, a page more to every range
    template<>
    struct PageInfo<Array<>>
    : public PageInfo<Array<PageDataSize>> {
        using Allocator = detail::ObjectAllocator<Array<>>;
    };
}
