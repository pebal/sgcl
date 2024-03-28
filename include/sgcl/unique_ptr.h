//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "priv/unique_ptr.h"

namespace sgcl {
    template<class T>
    using unique_ptr = typename Priv::Unique_ptr<T>;
}
