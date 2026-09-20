//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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
        constexpr void fill(const auto& value) {
            std::ranges::fill(_begin(), _end(), value);
        }

        constexpr void reverse() noexcept requires req::bidirectional<Derived> {
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
