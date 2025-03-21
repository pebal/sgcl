//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

namespace sgcl {
    template<class>
    class atomic;
    template<class>
    class atomic_ref;
    class collector;
    template<class>
    class list;
    template<class>
    class tracked_ptr;
    template<class T, unsigned N>
    class tracked_ptr<T[N]>;
    template<class>
    class unique_ptr;
    template<class T, unsigned N>
    class unique_ptr<T[N]>;
}
