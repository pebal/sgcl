//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../req.h"

namespace sgcl::mixin {
    // random_access<Derived>: a declaration without methods, that an
    // element is reached by its position in constant time (it + n,
    // it[n]) and the searches on a sorted range (binary_search,
    // lower_bound) are O(log n). What iterator_category says of the
    // iterator, said of the container where a concept can ask for it:
    // req::random_access<R> is "R carries random_access", and
    // mixin::bidirectional with it.
    template<class Derived>
    class random_access {
    protected:
        random_access() = default;
        ~random_access() = default;
    };
}
