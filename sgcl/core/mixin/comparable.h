//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../req.h"
#include "detail/synth_three_way.h"

#include <algorithm>
#include <compare>

namespace sgcl::mixin {
    // comparable<Derived>: two values of Derived are ordered
    // lexicographically by their elements (what <=> is on the standard
    // sequences and the ordered associative containers), and the
    // declaration that they are: req::comparable<R> is "R carries
    // comparable" (or, for a value that is not the library's, "R has
    // <=> or <"). The operator exists only for elements that are ordered
    // (req::comparable): by their <=>, or by a weak ordering built from
    // their < (synth_three_way, as the standard does). Carried beside
    // mixin::equatable, which gives ==; a container whose iteration order is
    // not a value (set) does not carry this one.
    template<class Derived>
    class comparable {
    public:
        constexpr friend auto operator<=>(const Derived& a, const Derived& b) requires detail::ComparableElements<Derived> {
            return std::lexicographical_compare_three_way(a.begin(), a.end(), b.begin(), b.end(), detail::synth_three_way);
        }

    protected:
        comparable() = default;
        ~comparable() = default;
    };
}
