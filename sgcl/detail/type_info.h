//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "page_info.h"

namespace sgcl::detail {
    template<class T>
    struct TypeInfo : PageInfo<std::remove_cv_t<T>> {
        using Type = std::remove_cv_t<T>;
    };
}
