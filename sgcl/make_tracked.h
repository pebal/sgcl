//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "priv/maker.h"

namespace sgcl {
    template<class T, class ...A, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    auto make_tracked(A&&... a) {
        static_assert(sizeof(T) <= Priv::PageDataSize, "Object is too large");
        return Priv::Maker<T>::make_tracked(std::forward<A>(a)...);
    }

    template<class T, std::enable_if_t<std::is_array_v<T>, int> = 0>
    auto make_tracked(size_t size) {
        return Priv::Maker<T>::make_tracked(size);
    }

    template<class T, class ...A, std::enable_if_t<std::is_array_v<T>, int> = 0>
    auto make_tracked(size_t size, A&&... a) {
        return Priv::Maker<T>::make_tracked(size, std::forward<A>(a)...);
    }

    template<class T, std::enable_if_t<std::is_array_v<T>, int> = 0>
    auto make_tracked(std::initializer_list<std::remove_extent_t<T>> init) {
        return Priv::Maker<T>::make_tracked(init);
    }
}
