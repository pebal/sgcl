//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The names of the standard types the Sgcl interface hands back:
// Optional<T> (Receive, TryDequeue, TryGet), None for the empty one.
#pragma once

#include "../../core/aliases.h"

namespace Sgcl {
    template<class T>
    using Optional = sgcl::optional<T>;

    inline constexpr sgcl::nullopt_t None = sgcl::nullopt;

    template<class A, class B>
    using Pair = sgcl::pair<A, B>;
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

