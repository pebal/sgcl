//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "object_pool_allocator_base.h"

namespace sgcl::detail {
    // The pool of T: its value type and free function. Never constructed:
    // a thread makes the pool's allocator as an ObjectPoolAllocatorBase
    // from T's Metadata (thread.h: _make_pool_allocator).
    template<class T>
    class ObjectPoolAllocator
    : public ObjectPoolAllocatorBase {
    public:
        using ValueType = typename TypeInfo<T>::Type;
        using IsPoolAllocator = std::true_type;

        // GC thread: one function for every pool (Metadata::free)
        static constexpr void (*free)(Page*) noexcept = &ObjectPoolAllocatorBase::free_pool_pages;
    };
}
