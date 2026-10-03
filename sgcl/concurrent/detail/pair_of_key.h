//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"

#include <type_traits>

namespace sgcl::concurrent::detail {
    // Whether P is a pair whose first is of the type Key: the argument of
    // a map's insert whose key the search can read before the element is
    // built (skip_list.h, split_list.h)
    template<class P, class Key>
    inline constexpr bool PairOfKey = false;

    template<class A, class B, class Key>
    inline constexpr bool PairOfKey<pair<A, B>, Key> = std::is_same_v<std::remove_cv_t<A>, Key>;
}
