//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "priv/maker.h"

namespace sgcl {   
    template<class T, class ...A>
    auto make_tracked(A&&... a) {
        if constexpr(!std::is_array_v<T>) {
            static_assert(sizeof(T) <= Priv::PageDataSize, "Object is too large");
        }
        return Priv::Maker<T>::make_tracked(std::forward<A>(a)...);
    }
}
