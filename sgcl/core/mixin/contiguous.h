//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../req.h"

namespace sgcl::mixin {
    // contiguous<Derived>: a declaration without methods, that the
    // elements lie in one block: data() is the first, data() + size() the
    // end, and a slice of them holds the block. What iterator_category
    // says of the iterator, said of the container where a concept can
    // ask for it: req::contiguous<R> is "R carries contiguous", and
    // mixin::random_access and mixin::bidirectional with it.
    template<class Derived>
    class contiguous {
    protected:
        contiguous() = default;
        ~contiguous() = default;
    };
}
