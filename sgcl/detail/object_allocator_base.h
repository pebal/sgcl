//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"

namespace sgcl::detail {
    // What the two allocators of a thread's type share (object_allocator.h
    // for large objects, object_pool_allocator.h for the pool): the
    // thread's list of new pages, on which both publish theirs for the
    // collector (thread.h: Data::pages). The collector exists from the
    // first allocator on.
    class ObjectAllocatorBase {
    public:
        ObjectAllocatorBase(std::atomic<Page*>& pages)
        : _pages(pages) {
            collector_init();
        }

        virtual ~ObjectAllocatorBase() noexcept = default;

    protected:
        std::atomic<Page*>& _pages;
    };
}
