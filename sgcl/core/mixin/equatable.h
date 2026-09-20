//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../req.h"

#include <algorithm>

namespace sgcl::mixin {
    // equatable<Derived>: two values of Derived compare equal when they
    // hold equal elements in the same order (what == is on every standard
    // container), and the declaration that they compare: req::equatable<R>
    // is "R carries equatable" (or, for a value that is not the
    // library's, "R has =="). The operator exists only for elements that
    // compare (req::equatable); found through Derived, as a friend of its
    // base. Nothing about order: that is mixin::comparable, and a container
    // without an order of its own (set) carries only this one.
    template<class Derived>
    class equatable {
    public:
        constexpr friend bool operator==(const Derived& a, const Derived& b) requires detail::EquatableElements<Derived> {
            return std::equal(a.begin(), a.end(), b.begin(), b.end());
        }

    protected:
        equatable() = default;
        ~equatable() = default;
    };
}
