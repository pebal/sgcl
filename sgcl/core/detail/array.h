//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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

    template<>
    struct PageInfo<Array<>> : public PageInfo<Array<PageDataSize>> {
        using ObjectAllocator = detail::ObjectAllocator<Array<>>;
    };
}
