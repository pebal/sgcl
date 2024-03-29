//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "page_info.h"

namespace sgcl {
    namespace Priv {
        template<class T>
        struct Type_info : Page_info<std::remove_cv_t<T>> {
            using type = std::remove_cv_t<T>;
        };
    }
}
