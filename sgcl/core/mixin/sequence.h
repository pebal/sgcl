//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../req.h"

#include <algorithm>

namespace sgcl::mixin {
    // sequence<Derived>: the writes over a range whose elements can be
    // assigned — filling and reversing — and the declaration that they
    // can: req::sequence<R> is "R carries sequence", which is what the
    // sorts of mixin::ordered ask for. Nothing here asks anything of the
    // element. reverse needs a bidirectional range; the linked lists
    // have their own, on the nodes, which hides this one.
    template<class Derived>
    class sequence {
    public:
        // Where the elements are assignable from the value
        template<class V>
        constexpr void fill(const V& value) noexcept(std::is_nothrow_assignable_v<detail::ElementReference<Derived>, const V&>) requires std::is_assignable_v<detail::ElementReference<Derived>, const V&> {
            std::ranges::fill(_begin(), _end(), value);
        }

        constexpr void reverse() noexcept(std::is_nothrow_swappable_v<detail::ElementValue<Derived>>) requires req::bidirectional<Derived> {
            std::ranges::reverse(_begin(), _end());
        }

    protected:
        sequence() = default;
        ~sequence() = default;

    private:
        constexpr auto _begin() noexcept { return static_cast<Derived&>(*this).begin(); }
        constexpr auto _end() noexcept { return static_cast<Derived&>(*this).end(); }
    };
}
