//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstdint>

namespace sgcl::hash::detail {
    // A 128-bit number as two words
    struct Wide {
        uint64_t low;
        uint64_t high;
    };

    // The whole product of two words: one instruction pair (mul, umulh) where
    // the compiler has a 128-bit integer; four products of halves where it
    // has not (MSVC), the same result
    inline Wide multiply_wide(uint64_t a, uint64_t b) noexcept {
#if defined(__SIZEOF_INT128__)
        const unsigned __int128 p = (unsigned __int128)a * b;
        return {uint64_t(p), uint64_t(p >> 64)};
#else
        const uint64_t a_low = a & 0xffffffffu, a_high = a >> 32;
        const uint64_t b_low = b & 0xffffffffu, b_high = b >> 32;
        const uint64_t low_low = a_low * b_low;
        const uint64_t high_low = a_high * b_low;
        const uint64_t low_high = a_low * b_high;
        const uint64_t high_high = a_high * b_high;
        const uint64_t middle = (low_low >> 32) + (high_low & 0xffffffffu) + low_high;   // no overflow: three numbers below 2^32 · 2^32
        return {(middle << 32) | (low_low & 0xffffffffu), high_high + (high_low >> 32) + (middle >> 32)};
#endif
    }

    // The product of two words folded into one, its halves XORed: every bit
    // of both inputs reaches most bits of the result
    inline uint64_t fold_product(uint64_t a, uint64_t b) noexcept {
        const Wide p = multiply_wide(a, b);
        return p.low ^ p.high;
    }

    inline uint64_t rotate_left(uint64_t v, int s) noexcept {
        return (v << s) | (v >> (64 - s));
    }

    inline uint32_t rotate_left(uint32_t v, int s) noexcept {
        return (v << s) | (v >> (32 - s));
    }
}
