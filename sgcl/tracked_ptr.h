//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "pointer.h"

namespace sgcl {
    template <class T>
    using TrackedPtr = Pointer<T, PointerPolicy::Tracked>;
}
