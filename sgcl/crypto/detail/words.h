//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/slice.h"
#include "bytes.h"
#include "paths.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

// The product of two 64-bit words (the module's paths: paths.h)

namespace sgcl::crypto::detail {
    inline uint32_t rotl32(uint32_t v, int n) noexcept {
        return v << n | v >> (32 - n);
    }

    // The 128-bit product of two words, as (low, high): one mul and one
    // umulh on arm64, one mul on x86-64. No branch and no table: Poly1305
    // multiplies its secret key with it.
    struct Wide {
        uint64_t lo, hi;
    };

    inline Wide mul64(uint64_t a, uint64_t b) noexcept {
#if defined(__SIZEOF_INT128__)
        unsigned __int128 p = (unsigned __int128)a * b;
        return {uint64_t(p), uint64_t(p >> 64)};
#elif defined(_MSC_VER) && defined(_M_X64)
        uint64_t hi;
        uint64_t lo = _umul128(a, b, &hi);
        return {lo, hi};
#else
        uint64_t a0 = uint32_t(a), a1 = a >> 32, b0 = uint32_t(b), b1 = b >> 32;
        uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
        uint64_t mid = (p00 >> 32) + uint32_t(p01) + uint32_t(p10);
        return {(mid << 32) | uint32_t(p00), p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32)};
#endif
    }

    // a + b into a, with the carry of the low word into the high one
    inline void add_wide(Wide& a, Wide b) noexcept {
        uint64_t lo = a.lo + b.lo;
        a.hi += b.hi + (lo < b.lo);
        a.lo = lo;
    }

    // The low bits of a wide value from the bit `shift` on (shift < 64)
    inline uint64_t shift_right(Wide v, int shift) noexcept {
        return v.lo >> shift | v.hi << (64 - shift);
    }

    // Two buffers that overlap other than by starting at the same byte:
    // in-place work is allowed (out == in), a shifted overlap is not, as
    // Go's cipher packages panic on it
    inline bool inexact_overlap(const void* out, size_t out_size, const void* in, size_t in_size) noexcept {
        if (out_size == 0 || in_size == 0 || out == in) {
            return false;
        }
        auto o = reinterpret_cast<uintptr_t>(out);
        auto i = reinterpret_cast<uintptr_t>(in);
        return o < i + in_size && i < o + out_size;
    }
}
