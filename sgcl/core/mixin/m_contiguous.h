//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "concepts.h"

namespace sgcl {
    // m_contiguous<Derived>: a declaration without methods, that the
    // elements lie in one block: data() is the first, data() + size() the
    // end, and a slice of them holds the block. What iterator_category
    // says of the iterator, said of the container where a concept can
    // ask for it: c_contiguous<R> is "R carries m_contiguous", and
    // m_random_access and m_bidirectional with it.
    template<class Derived>
    class m_contiguous {
    protected:
        m_contiguous() = default;
        ~m_contiguous() = default;
    };
}
