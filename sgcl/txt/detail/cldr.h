//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/os.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

// What every header of CLDR data is read with.
//
// A table of numbers is written as base-64 digits in literals rather than
// as an array of numbers: a value of up to 18 bits is three characters, of
// up to 36 six, where the number written out with its comma is four to
// eleven, and a literal compiles faster than an initializer of a hundred
// thousand elements. A value is read where it is needed, a few loads.
//
// A text is a reference of 32 bits into the header's chunks of UTF-8 —
// the chunk (4 bits), the offset in it (16) and the length (12) — and a
// row of a table names its texts by their index in the references.
namespace sgcl::txt::detail::cldr {
    // the value of a base-64 digit: A-Z a-z 0-9 + /
    inline constexpr uint8_t Digit64[128] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0,
        0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 0,
        0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0, 0, 0, 0, 0,
    };

    struct Packed {
        const char* const* chunks;
        uint32_t shift;    // 2^shift values a chunk
        uint32_t digits;   // digits of a value

        SGCL_INLINE_HOT constexpr uint32_t operator[](size_t i) const noexcept {
            const char* p = chunks[i >> shift] + (i & ((size_t(1) << shift) - 1)) * digits;
            uint32_t v = 0;
            for (uint32_t k = 0; k < digits; ++k) {
                v = (v << 6) | Digit64[uint8_t(p[k]) & 127];
            }
            return v;
        }
    };

    // The texts of a header: a reference is the chunk << 16 | the offset,
    // and a text runs to the next one's offset, or to the end of its chunk
    // where the next one is in another
    struct Text {
        const char8_t* const* chunks;
        const uint16_t* sizes;
        Packed refs;

        SGCL_INLINE_HOT std::string_view operator[](uint32_t index) const noexcept {
            uint32_t r = refs[index];
            uint32_t next = refs[index + 1];
            uint32_t chunk = r >> 16, offset = r & 0xFFFF;
            uint32_t end = (next >> 16) == chunk ? (next & 0xFFFF) : sizes[chunk];
            return std::string_view(reinterpret_cast<const char*>(chunks[chunk] + offset), end - offset);
        }
    };

    // Value k of record i of a sparse table: a record is a mask of its
    // values that are not 0 and then those values
    SGCL_INLINE_HOT constexpr uint32_t sparse(const Packed& values, const Packed& starts, uint32_t i, uint32_t k) noexcept {
        uint32_t at = starts[i];
        uint32_t mask = values[at];
        if (!(mask & (1u << k))) {
            return 0;
        }
        return values[at + 1 + uint32_t(__builtin_popcount(mask & ((1u << k) - 1)))];
    }

    // The first element of a sorted array not below key
    template<class T, class K>
    SGCL_INLINE_HOT constexpr size_t lower_bound(const T* a, size_t n, K key) noexcept {
        size_t lo = 0;
        while (n > 0) {
            size_t half = n / 2;
            if (a[lo + half] < key) {
                lo += half + 1;
                n -= half + 1;
            } else {
                n = half;
            }
        }
        return lo;
    }

    // The index of key in a sorted array, or n
    template<class T, size_t N, class K>
    SGCL_INLINE_HOT constexpr size_t find(const T (&a)[N], K key) noexcept {
        size_t i = lower_bound(a, N, key);
        return i < N && a[i] == key ? i : N;
    }
}
