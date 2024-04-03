//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include <cstddef>

namespace sgcl {
    template<class>
    class atomic;

    struct collector;
    struct metadata;
    struct metadata_base;

    template<class>
    class tracked_ptr;

    template<class T, size_t N>
    class tracked_ptr<T[N]>;

    template<class>
    class root_ptr;

    template<class T, size_t N>
    class root_ptr<T[N]>;

    template<class>
    struct unsafe_ptr;

    template<class T, size_t N>
    struct unsafe_ptr<T[N]>;
}
