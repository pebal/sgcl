//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "unique_deleter.h"

#include <memory>

namespace sgcl::detail {
    template<class T>
    using UniquePtr = std::unique_ptr<T, UniqueDeleter>;
}
