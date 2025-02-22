//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/tracked.h"

#include <type_traits>

namespace sgcl {
    template<typename T>
    concept IsPointer = requires(T t) {
        { t.clone() };
        { t.get() };
        { t.object_size() };
    };

    template <typename T>
    concept TrackedPointer = std::is_base_of_v<detail::Tracked, T>;
}
