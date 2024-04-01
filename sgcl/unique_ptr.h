//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "priv/unique_ptr.h"

namespace sgcl {
    template<class T>
    using unique_ptr = typename Priv::Unique_ptr<T>;

    template<class T, class U, std::enable_if_t<!std::is_array_v<T> || std::is_same_v<std::remove_cv_t<U>, void>, int> = 0>
    inline unique_ptr<T> static_pointer_cast(unique_ptr<U>&& r) noexcept {
        return unique_ptr<T>(static_cast<typename unique_ptr<T>::element_type*>(r.release()), Priv::Tracked());
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    inline unique_ptr<T> const_pointer_cast(unique_ptr<U>&& r) noexcept {
        return unique_ptr<T>(const_cast<typename unique_ptr<T>::element_type*>(r.release()), Priv::Tracked());
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    inline unique_ptr<T> dynamic_pointer_cast(unique_ptr<U>&& r) noexcept {
        return unique_ptr<T>(dynamic_cast<typename unique_ptr<T>::element_type*>(r.release()), Priv::Tracked());
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    inline unique_ptr<T> reinterpret_pointer_cast(unique_ptr<U>&& r) noexcept {
        return unique_ptr<T>(reinterpret_cast<typename unique_ptr<T>::element_type*>(r.release()), Priv::Tracked());
    }
}
