//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "unique_deleter.h"

#include <memory>

namespace sgcl {
    template<class T>
    using unique_ptr = std::unique_ptr<T, unique_deleter>;
}
