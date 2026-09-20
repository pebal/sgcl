//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "concepts.h"

namespace sgcl {
    // m_random_access<Derived>: a declaration without methods, that an
    // element is reached by its position in constant time (it + n,
    // it[n]) and the searches on a sorted range (binary_search,
    // lower_bound) are O(log n). What iterator_category says of the
    // iterator, said of the container where a concept can ask for it:
    // c_random_access<R> is "R carries m_random_access", and
    // m_bidirectional with it.
    template<class Derived>
    class m_random_access {
    protected:
        m_random_access() = default;
        ~m_random_access() = default;
    };
}
