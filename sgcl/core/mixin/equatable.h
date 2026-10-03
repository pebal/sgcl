//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../req.h"

#include <algorithm>

namespace sgcl::mixin {
    // equatable<Derived>: two values of Derived compare equal when they
    // hold equal elements in the same order (what == is on every standard
    // container), so that Derived is req::equatable when its elements
    // are. The operator exists only for elements that compare
    // (req::equatable); found through Derived, as a friend of its base.
    // Nothing about order: that is mixin::comparable, which every
    // container carrying this one carries too (the hash containers carry
    // neither and have an == of their own). Not
    // noexcept even for elements whose == is, as std's containers: a
    // value whose defaulted == compares a container of itself (a tree)
    // would need its own exception specification to compute this one's.
    template<class Derived>
    class equatable {
    public:
        // The sizes first where a container knows its size and the walk
        // does not (a list): std::equal compares the distance itself for
        // random access, a walk of two lists would compare the elements
        // up to the end of the shorter
        constexpr friend bool operator==(const Derived& a, const Derived& b) requires detail::EquatableElements<Derived> {
            if constexpr (std::ranges::sized_range<const Derived> && !std::ranges::random_access_range<const Derived>) {
                if (std::ranges::size(a) != std::ranges::size(b)) {
                    return false;
                }
            }
            return std::equal(a.begin(), a.end(), b.begin(), b.end());
        }

    protected:
        equatable() = default;
        ~equatable() = default;
    };
}
