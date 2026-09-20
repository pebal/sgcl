//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../req.h"

namespace sgcl::mixin {
    // bidirectional<Derived>: a declaration without methods, that the
    // elements can be walked from the back too: rbegin() and rend(), and
    // the questions that walk backwards (last_index_of) are cheap. What
    // iterator_category says of the iterator, said of the container
    // where a concept can ask for it: req::bidirectional<R> is "R carries
    // bidirectional".
    template<class Derived>
    class bidirectional {
    protected:
        bidirectional() = default;
        ~bidirectional() = default;
    };
}
