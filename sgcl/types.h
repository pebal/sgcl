//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "concepts.h"

#include <cstddef>

namespace sgcl {
    enum class PointerPolicy {
        Tracked,
        Stack
    };

    template<class T>
    using StackType = detail::StackType<T>::Type;

    template<class T, IsPointer Base>
    class ArrayPtr;
    template<TrackedPointer>
    class Atomic;
    template<TrackedPointer>
    class AtomicRef;
    class Collector;
    template<class, PointerPolicy>
    class Pointer;
    template<class T, PointerPolicy P, size_t N>
    class Pointer<T[N], P>;
    template<class>
    class UniquePtr;
    template<class T, size_t N>
    class UniquePtr<T[N]>;
    template<class>
    class UnsafePtr;
    template<class T, size_t N>
    class UnsafePtr<T[N]>;
}
