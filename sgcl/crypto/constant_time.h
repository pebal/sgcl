//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/slice.h"
#include "secure_zero.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// Comparisons whose time does not depend on the bytes compared, Go's
// crypto/subtle: what a tag, a MAC or a password hash is checked with, so
// that an attacker who measures how long a check takes does not learn how
// many leading bytes of a forgery were right.
namespace sgcl::crypto {
    namespace detail {
        // A value the optimizer may not reason about: an empty asm that
        // says it may have changed the register. It keeps the compiler
        // from turning an accumulated difference back into a branch (an
        // early exit at the first byte that differs) or a mask into a
        // select it predicts
        template<class T>
        SGCL_INLINE_HOT T value_barrier(T v) noexcept {
#if defined(__GNUC__) || defined(__clang__)
            __asm__ volatile("" : "+r"(v));
#else
            volatile T w = v;
            v = w;
#endif
            return v;
        }

        // Whether a and b hold the same n bytes, in a time that depends on
        // n only: every byte is read, eight at a time, the differences ORed
        // together and the result made from the sum with no branch on it
        inline bool equal_bytes(const unsigned char* a, const unsigned char* b, size_t n) noexcept {
            uint64_t d = 0;
            size_t i = 0;
            for (; i + 8 <= n; i += 8) {
                uint64_t x, y;
                std::memcpy(&x, a + i, 8);
                std::memcpy(&y, b + i, 8);
                d = value_barrier(d | (x ^ y));
            }
            for (; i < n; ++i) {
                d = value_barrier(d | uint64_t(a[i] ^ b[i]));
            }
            // 1 when d is zero, 0 otherwise, with no comparison of d
            return (((d | (0 - d)) >> 63) ^ 1) != 0;
        }
    }

    namespace constant_time {
        // Whether a and b hold the same bytes. Every byte of both is read
        // whatever they hold. The lengths are not secret: two slices of
        // different sizes are unequal at once, as Go's ConstantTimeCompare
        // has it (a tag is compared with one of its own length, which the
        // protocol fixes). [[nodiscard]]: a comparison whose result is
        // dropped is a check that was never made.
        [[nodiscard]] SGCL_INLINE_HOT bool equal(const slice<const byte>& a, const slice<const byte>& b) noexcept {
            if (a.size() != b.size()) {
                return false;
            }
            return detail::equal_bytes(reinterpret_cast<const unsigned char*>(a.data()), reinterpret_cast<const unsigned char*>(b.data()), a.size());
        }
    }
}
