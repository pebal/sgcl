//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "concepts.h"

#include <algorithm>

namespace sgcl {
    // m_equatable<Derived>: two values of Derived compare equal when they
    // hold equal elements in the same order (what == is on every standard
    // container), and the declaration that they compare: c_equatable<R>
    // is "R carries m_equatable" (or, for a value that is not the
    // library's, "R has =="). The operator exists only for elements that
    // compare (c_equatable); found through Derived, as a friend of its
    // base. Nothing about order: that is m_comparable, and a container
    // without an order of its own (set) carries only this one.
    template<class Derived>
    class m_equatable {
    public:
        constexpr friend bool operator==(const Derived& a, const Derived& b) requires detail::EquatableElements<Derived> {
            return std::equal(a.begin(), a.end(), b.begin(), b.end());
        }

    protected:
        m_equatable() = default;
        ~m_equatable() = default;
    };
}
