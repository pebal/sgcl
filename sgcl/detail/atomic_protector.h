//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "maker.h"
#include "pointer.h"
#include "tracked.h"

namespace sgcl::detail {
    class AtomicProtector : Tracked
    {
    public:
        inline static void protect(void* p) {
            if (p) {
                Maker<AtomicProtector>::make_tracked(p);
            }
        }

    private:
        AtomicProtector(void* p) noexcept
        : ptr(p) {
        }

        const Pointer ptr;

        template<class T, class ...A> friend T* construct(void*, A&&...);
    };
}
