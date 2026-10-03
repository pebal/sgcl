//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <iterator>
#include <type_traits>
#include <utility>

namespace sgcl::immutable::detail {
    template<class It>
    consteval bool nothrow_step_back() {
        if constexpr (std::bidirectional_iterator<It>) {
            return noexcept(--std::declval<It&>());
        } else {
            return true;
        }
    }

    // A walk over [first, last) of It that cannot throw: the iterator
    // copied and moved, compared, advanced (and stepped back, when it
    // can be) and read. Half of the condition of a constructor from a
    // range; the other half is the element made of what it reads.
    template<class It>
    inline constexpr bool nothrow_iteration =
        std::is_nothrow_copy_constructible_v<It> && std::is_nothrow_move_constructible_v<It>
        && noexcept(std::declval<It&>() != std::declval<It&>())
        && noexcept(++std::declval<It&>())
        && noexcept(*std::declval<It&>())
        && nothrow_step_back<It>();
}
