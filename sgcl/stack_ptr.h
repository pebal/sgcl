//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "pointer.h"

namespace sgcl {
    template <typename T>
    class StackPtr : public Pointer<T, PointerPolicy::Stack> {
    public:
        using Base = Pointer<T, PointerPolicy::Stack>;
        using Base::Base;

        StackPtr(const Base& base)
        : Base(base) {
        }
    };

    template <typename T>
    StackPtr(T*) -> StackPtr<T>;

    template <typename T>
    StackPtr(Pointer<T, PointerPolicy::Tracked>) -> StackPtr<T>;

    template <typename T>
    StackPtr(UniquePtr<T>&&) -> StackPtr<T>;

    template <typename T>
    StackPtr(const UnsafePtr<T>&&) -> StackPtr<T>;
}
