//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the containers' boundary tests share (DESIGN 408): a value whose
// destructor overwrites it, so that an argument read after the element it
// refers to was destroyed (an erasure by the container's own key or value)
// compares unequal to what it was, rather than by chance as it was.
#pragma once

#include <compare>
#include <cstddef>
#include <functional>

namespace boundary {
    struct Poisoned {
        static constexpr int Dead = -1000000;

        int v = 0;

        Poisoned() = default;

        Poisoned(int x) noexcept
        : v(x) {
        }

        Poisoned(const Poisoned&) = default;
        Poisoned& operator=(const Poisoned&) = default;

        ~Poisoned() {
            *(volatile int*)&v = Dead;   // a store the compiler keeps: the element read after its end shows it
        }

        friend bool operator==(const Poisoned& a, const Poisoned& b) noexcept {
            return a.v == b.v;
        }

        friend auto operator<=>(const Poisoned& a, const Poisoned& b) noexcept {
            return a.v <=> b.v;
        }
    };
}

template<>
struct std::hash<boundary::Poisoned> {
    size_t operator()(const boundary::Poisoned& p) const noexcept {
        return std::hash<int>{}(p.v);
    }
};
