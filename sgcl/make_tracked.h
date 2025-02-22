//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/maker.h"
#include "unique_ptr.h"

namespace sgcl {
    template<class T, class ...A, std::enable_if_t<!std::is_array_v<T> && !std::is_void_v<T>, int> = 0>
    auto make_tracked(A&&... a) {
        static_assert(sizeof(detail::Array<sizeof(T)>) <= detail::PageDataSize, "Object is too large");
        auto ptr = detail::Maker<T>::make_tracked(std::forward<A>(a)...);
        return UniquePtr<T>(std::move(ptr));
    }

    template<class T, std::enable_if_t<std::is_array_v<T>, int> = 0>
    auto make_tracked(size_t size) {
        static_assert(sizeof(detail::Array<sizeof(std::remove_extent_t<T>)>) <= detail::PageDataSize, "Object is too large");
        auto ptr = detail::Maker<T>::make_tracked(size);
        return UniquePtr<T>(std::move(ptr));
    }

    template<class T, class ...A, std::enable_if_t<std::is_array_v<T>, int> = 0>
    auto make_tracked(size_t size, A&&... a) {
        static_assert(sizeof(detail::Array<sizeof(std::remove_extent_t<T>)>) <= detail::PageDataSize, "Object is too large");
        auto ptr = detail::Maker<T>::make_tracked(size, std::forward<A>(a)...);
        return UniquePtr<T>(std::move(ptr));
    }

    template<class T, std::enable_if_t<std::is_array_v<T> && std::is_trivial_v<std::remove_extent_t<T>>, int> = 0>
    auto make_tracked(std::initializer_list<std::remove_extent_t<T>> init) {
        static_assert(sizeof(detail::Array<sizeof(std::remove_extent_t<T>)>) <= detail::PageDataSize, "Object is too large");
        auto ptr = detail::Maker<T>::make_tracked(init);
        return UniquePtr<T>(std::move(ptr));
    }
}
