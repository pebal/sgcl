//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../req.h"

namespace sgcl::mixin {
    // immutable<Derived>: a declaration without methods, that the
    // container is a value that never changes: no method writes an
    // element in place, every change (set, push_back, insert, erase) is
    // a const method that returns a new container sharing the old one's
    // structure, and a copy is one word. A function that takes
    // req::immutable auto can keep what it was given, hand it to another
    // thread or compare it with a later version without a copy and
    // without a lock: im::vector, im::list, im::map, im::set carry it.
    // What a container that is merely not written (slice<const T>, a
    // sorted_set) does not say: their object may change behind them.
    template<class Derived>
    class immutable {
    protected:
        immutable() = default;
        ~immutable() = default;
    };
}
