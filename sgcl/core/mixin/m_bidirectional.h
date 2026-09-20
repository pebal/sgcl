//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "concepts.h"

namespace sgcl {
    // m_bidirectional<Derived>: a declaration without methods, that the
    // elements can be walked from the back too: rbegin() and rend(), and
    // the questions that walk backwards (last_index_of) are cheap. What
    // iterator_category says of the iterator, said of the container
    // where a concept can ask for it: c_bidirectional<R> is "R carries
    // m_bidirectional".
    template<class Derived>
    class m_bidirectional {
    protected:
        m_bidirectional() = default;
        ~m_bidirectional() = default;
    };
}
