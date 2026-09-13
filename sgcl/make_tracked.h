//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/maker.h"
#include "unique_ptr.h"

namespace sgcl {
    template<class T, class ...A>
    auto make_tracked(A&&... a) {
        static_assert(!std::is_array_v<T>, "Managed arrays are not a public type; use sgcl::vector");
        static_assert(!std::is_void_v<T>, "Cannot create an object of type void");
        static_assert(sizeof(detail::Array<sizeof(T)>) <= detail::PageDataSize, "Object is too large");
        auto ptr = detail::Maker<T>::make_tracked(std::forward<A>(a)...);
        return unique_ptr<T>(std::move(ptr));
    }
}
