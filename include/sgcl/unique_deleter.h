//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "priv/unique_deleter.h"

namespace sgcl {
    using unique_deleter = Priv::Unique_deleter;
}
