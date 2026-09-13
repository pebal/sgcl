//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <compare>
#include <concepts>

namespace sgcl::detail {
    // [expos.only.func] synth-three-way: <=> when the type has it, else a
    // weak ordering built from operator<, so that the containers compare
    // for every type the standard containers compare.
    struct SynthThreeWay {
        template<class T, class U>
        constexpr auto operator()(const T& t, const U& u) const
            requires requires {
                { t < u } -> std::convertible_to<bool>;
                { u < t } -> std::convertible_to<bool>;
            } {
            if constexpr(std::three_way_comparable_with<T, U>) {
                return t <=> u;
            } else {
                if (t < u) {
                    return std::weak_ordering::less;
                }
                if (u < t) {
                    return std::weak_ordering::greater;
                }
                return std::weak_ordering::equivalent;
            }
        }
    };

    inline constexpr SynthThreeWay synth_three_way;

    template<class T, class U = T>
    using synth_three_way_result = decltype(synth_three_way(std::declval<T&>(), std::declval<U&>()));
}
