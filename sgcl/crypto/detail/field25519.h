//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "../secure_zero.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// The field of integers modulo p = 2^255 - 19, which X25519 (RFC 7748) and
// Ed25519 (RFC 8032) compute in. An element is five limbs of 51 bits in
// 64-bit words, v0 + v1·2^51 + v2·2^102 + v3·2^153 + v4·2^204; a product of
// two limbs fits in 128 bits (unsigned __int128, one mul and one umulh on
// arm64), and 2^255 = 19 mod p folds what passes the top limb back into the
// bottom one, times 19.
//
// Every operation here takes the same time whatever the values: no branch
// and no memory index depends on a limb. The elements of a secret — a
// scalar's ladder, the point a secret scalar gives — pass through the
// same functions as public ones. What is allowed to take a variable time
// is said where it is done (point decompression of a public key, the
// verification of a signature, both in edwards25519.h).
//
// Bounds. A "loose" element has limbs below 2^51 + 2^18; every function
// here returns a loose element and takes loose ones. A product of two
// loose limbs is below 2^104, a sum of five of them, one side times 19,
// below 2^112: well inside 128 bits. Subtraction adds 2p (limbs 2^52 - 38
// and 2^52 - 2), which is above any loose limb, so that no limb goes
// below zero. A canonical form (the value below p, limbs below 2^51) is
// made only when the element is written out as bytes or compared.
namespace sgcl::crypto::detail {
    using u128 = unsigned __int128;

    // A value the optimizer may not reason about, so that a mask made from
    // a secret bit stays a mask and is not turned back into a branch. The
    // 64-bit sibling of constant_time's barrier.
    inline uint64_t value_barrier(uint64_t v) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        __asm__("" : "+r"(v));
#endif
        return v;
    }

    // All ones when bit is 1, all zeros when it is 0 (bit is 0 or 1)
    inline uint64_t mask_of(uint64_t bit) noexcept {
        return value_barrier(0 - bit);
    }

    // All ones when a == b, in constant time
    inline uint64_t mask_equal(uint64_t a, uint64_t b) noexcept {
        uint64_t x = a ^ b;
        // x | -x has its top bit set exactly when x is not 0
        return mask_of(((x | (0 - x)) >> 63) ^ 1);
    }

    struct Fe {
        uint64_t v[5];
    };

    inline constexpr uint64_t fe_mask51 = (uint64_t(1) << 51) - 1;

    inline constexpr Fe fe_zero() noexcept {
        return Fe{{0, 0, 0, 0, 0}};
    }

    inline constexpr Fe fe_one() noexcept {
        return Fe{{1, 0, 0, 0, 0}};
    }

    // Each limb's carry taken into the next at once (the top one times 19
    // into the bottom): the limbs of any sum of loose elements come back
    // below 2^51 + 19·2^13
    inline void fe_carry(Fe& h) noexcept {
        uint64_t c0 = h.v[0] >> 51;
        uint64_t c1 = h.v[1] >> 51;
        uint64_t c2 = h.v[2] >> 51;
        uint64_t c3 = h.v[3] >> 51;
        uint64_t c4 = h.v[4] >> 51;
        h.v[0] = (h.v[0] & fe_mask51) + c4 * 19;
        h.v[1] = (h.v[1] & fe_mask51) + c0;
        h.v[2] = (h.v[2] & fe_mask51) + c1;
        h.v[3] = (h.v[3] & fe_mask51) + c2;
        h.v[4] = (h.v[4] & fe_mask51) + c3;
    }

    inline Fe fe_add(const Fe& a, const Fe& b) noexcept {
        Fe h;
        for (int i = 0; i < 5; ++i) {
            h.v[i] = a.v[i] + b.v[i];
        }
        fe_carry(h);
        return h;
    }

    // a - b as a + 2p - b, so that no limb goes below zero
    inline Fe fe_sub(const Fe& a, const Fe& b) noexcept {
        Fe h;
        h.v[0] = a.v[0] + 0xFFFFFFFFFFFDAull - b.v[0];
        h.v[1] = a.v[1] + 0xFFFFFFFFFFFFEull - b.v[1];
        h.v[2] = a.v[2] + 0xFFFFFFFFFFFFEull - b.v[2];
        h.v[3] = a.v[3] + 0xFFFFFFFFFFFFEull - b.v[3];
        h.v[4] = a.v[4] + 0xFFFFFFFFFFFFEull - b.v[4];
        fe_carry(h);
        return h;
    }

    inline Fe fe_neg(const Fe& a) noexcept {
        return fe_sub(fe_zero(), a);
    }

    // Five 128-bit column sums brought back to loose limbs: each column's
    // low 51 bits stay, its high part goes to the next column (the top
    // one's times 19 to the bottom), and a last carry pass evens them out
    inline Fe fe_reduce_wide(u128 r0, u128 r1, u128 r2, u128 r3, u128 r4) noexcept {
        Fe h;
        uint64_t h0 = uint64_t(r0 >> 51);
        uint64_t h1 = uint64_t(r1 >> 51);
        uint64_t h2 = uint64_t(r2 >> 51);
        uint64_t h3 = uint64_t(r3 >> 51);
        uint64_t h4 = uint64_t(r4 >> 51);
        h.v[0] = (uint64_t(r0) & fe_mask51) + h4 * 19;
        h.v[1] = (uint64_t(r1) & fe_mask51) + h0;
        h.v[2] = (uint64_t(r2) & fe_mask51) + h1;
        h.v[3] = (uint64_t(r3) & fe_mask51) + h2;
        h.v[4] = (uint64_t(r4) & fe_mask51) + h3;
        fe_carry(h);
        return h;
    }

    inline Fe fe_mul(const Fe& a, const Fe& b) noexcept {
        uint64_t a0 = a.v[0], a1 = a.v[1], a2 = a.v[2], a3 = a.v[3], a4 = a.v[4];
        uint64_t b0 = b.v[0], b1 = b.v[1], b2 = b.v[2], b3 = b.v[3], b4 = b.v[4];
        uint64_t b1_19 = b1 * 19, b2_19 = b2 * 19, b3_19 = b3 * 19, b4_19 = b4 * 19;
        u128 r0 = u128(a0) * b0 + u128(a1) * b4_19 + u128(a2) * b3_19 + u128(a3) * b2_19 + u128(a4) * b1_19;
        u128 r1 = u128(a0) * b1 + u128(a1) * b0 + u128(a2) * b4_19 + u128(a3) * b3_19 + u128(a4) * b2_19;
        u128 r2 = u128(a0) * b2 + u128(a1) * b1 + u128(a2) * b0 + u128(a3) * b4_19 + u128(a4) * b3_19;
        u128 r3 = u128(a0) * b3 + u128(a1) * b2 + u128(a2) * b1 + u128(a3) * b0 + u128(a4) * b4_19;
        u128 r4 = u128(a0) * b4 + u128(a1) * b3 + u128(a2) * b2 + u128(a3) * b1 + u128(a4) * b0;
        return fe_reduce_wide(r0, r1, r2, r3, r4);
    }

    // a², the cross products counted once and doubled
    inline Fe fe_sq(const Fe& a) noexcept {
        uint64_t a0 = a.v[0], a1 = a.v[1], a2 = a.v[2], a3 = a.v[3], a4 = a.v[4];
        uint64_t d0 = a0 * 2, d1 = a1 * 2;
        uint64_t a3_19 = a3 * 19, a4_19 = a4 * 19;
        u128 r0 = u128(a0) * a0 + u128(d1) * a4_19 + u128(a2 * 2) * a3_19;
        u128 r1 = u128(d0) * a1 + u128(a2 * 2) * a4_19 + u128(a3) * a3_19;
        u128 r2 = u128(d0) * a2 + u128(a1) * a1 + u128(a3 * 2) * a4_19;
        u128 r3 = u128(d0) * a3 + u128(d1) * a2 + u128(a4) * a4_19;
        u128 r4 = u128(d0) * a4 + u128(d1) * a3 + u128(a2) * a2;
        return fe_reduce_wide(r0, r1, r2, r3, r4);
    }

    // a squared n times
    inline Fe fe_sq_n(Fe a, int n) noexcept {
        for (int i = 0; i < n; ++i) {
            a = fe_sq(a);
        }
        return a;
    }

    // a times a small constant (below 2^32): the ladder's a24
    inline Fe fe_mul_small(const Fe& a, uint32_t k) noexcept {
        return fe_reduce_wide(u128(a.v[0]) * k, u128(a.v[1]) * k, u128(a.v[2]) * k, u128(a.v[3]) * k,
                              u128(a.v[4]) * k);
    }

    // 32 bytes little-endian; bit 255 is ignored, as RFC 7748 §5 and RFC
    // 8032 §5.1.3 both read a coordinate. A value from p to 2^255 - 1 is
    // taken as it is and reduced by the arithmetic (it is the caller's
    // business to reject it where a canonical encoding is required)
    inline Fe fe_from_bytes(const unsigned char* s) noexcept {
        uint64_t w0 = load_le64(s), w1 = load_le64(s + 8), w2 = load_le64(s + 16), w3 = load_le64(s + 24);
        Fe h;
        h.v[0] = w0 & fe_mask51;
        h.v[1] = (w0 >> 51 | w1 << 13) & fe_mask51;
        h.v[2] = (w1 >> 38 | w2 << 26) & fe_mask51;
        h.v[3] = (w2 >> 25 | w3 << 39) & fe_mask51;
        h.v[4] = (w3 >> 12) & fe_mask51;
        return h;
    }

    // The canonical form: the value below p, each limb below 2^51
    inline Fe fe_canonical(const Fe& a) noexcept {
        Fe h = a;
        // two passes of carries in order: the value is then below
        // 2^255 + 19 with limbs 1..4 below 2^51
        for (int pass = 0; pass < 2; ++pass) {
            h.v[1] += h.v[0] >> 51;
            h.v[0] &= fe_mask51;
            h.v[2] += h.v[1] >> 51;
            h.v[1] &= fe_mask51;
            h.v[3] += h.v[2] >> 51;
            h.v[2] &= fe_mask51;
            h.v[4] += h.v[3] >> 51;
            h.v[3] &= fe_mask51;
            h.v[0] += (h.v[4] >> 51) * 19;
            h.v[4] &= fe_mask51;
        }
        // q = 1 exactly when the value is p or more: then value + 19
        // reaches 2^255
        uint64_t q = (h.v[0] + 19) >> 51;
        q = (h.v[1] + q) >> 51;
        q = (h.v[2] + q) >> 51;
        q = (h.v[3] + q) >> 51;
        q = (h.v[4] + q) >> 51;
        // value + 19q - 2^255·q: the 2^255 is the bit dropped at the top
        h.v[0] += 19 * q;
        h.v[1] += h.v[0] >> 51;
        h.v[0] &= fe_mask51;
        h.v[2] += h.v[1] >> 51;
        h.v[1] &= fe_mask51;
        h.v[3] += h.v[2] >> 51;
        h.v[2] &= fe_mask51;
        h.v[4] += h.v[3] >> 51;
        h.v[3] &= fe_mask51;
        h.v[4] &= fe_mask51;
        return h;
    }

    // 32 bytes little-endian, canonical (the value below p, bit 255 zero)
    inline void fe_to_bytes(unsigned char* s, const Fe& a) noexcept {
        Fe h = fe_canonical(a);
        store_le64(s, h.v[0] | h.v[1] << 51);
        store_le64(s + 8, h.v[1] >> 13 | h.v[2] << 38);
        store_le64(s + 16, h.v[2] >> 26 | h.v[3] << 25);
        store_le64(s + 24, h.v[3] >> 39 | h.v[4] << 12);
    }

    // r = a where mask is all ones, r kept where it is zero
    inline void fe_cmov(Fe& r, const Fe& a, uint64_t mask) noexcept {
        for (int i = 0; i < 5; ++i) {
            r.v[i] ^= mask & (r.v[i] ^ a.v[i]);
        }
    }

    // a and b exchanged where mask is all ones
    inline void fe_cswap(Fe& a, Fe& b, uint64_t mask) noexcept {
        for (int i = 0; i < 5; ++i) {
            uint64_t t = mask & (a.v[i] ^ b.v[i]);
            a.v[i] ^= t;
            b.v[i] ^= t;
        }
    }

    // 1 when a is 0 mod p, else 0; in constant time
    inline uint64_t fe_is_zero(const Fe& a) noexcept {
        Fe h = fe_canonical(a);
        uint64_t x = h.v[0] | h.v[1] | h.v[2] | h.v[3] | h.v[4];
        return ((x | (0 - x)) >> 63) ^ 1;
    }

    // 1 when a and b are the same element, else 0
    inline uint64_t fe_equal(const Fe& a, const Fe& b) noexcept {
        return fe_is_zero(fe_sub(a, b));
    }

    // The low bit of the canonical value: RFC 8032's sign of x ("negative"
    // is odd)
    inline uint64_t fe_is_negative(const Fe& a) noexcept {
        return fe_canonical(a).v[0] & 1;
    }

    // a^(2^250 - 1) and a^11, the common start of the two exponentiations
    // below; the chain of squarings is fixed, so the time is too
    inline void fe_pow2_250_1(const Fe& a, Fe& out_2_250_1, Fe& out_11) noexcept {
        Fe a2 = fe_sq(a);                        // a^2
        Fe a8 = fe_sq_n(a2, 2);                  // a^8
        Fe a9 = fe_mul(a8, a);                   // a^9
        Fe a11 = fe_mul(a9, a2);                 // a^11
        Fe a22 = fe_sq(a11);                     // a^22
        Fe e5 = fe_mul(a22, a9);                 // a^(2^5 - 1)
        Fe e10 = fe_mul(fe_sq_n(e5, 5), e5);     // a^(2^10 - 1)
        Fe e20 = fe_mul(fe_sq_n(e10, 10), e10);  // a^(2^20 - 1)
        Fe e40 = fe_mul(fe_sq_n(e20, 20), e20);  // a^(2^40 - 1)
        Fe e50 = fe_mul(fe_sq_n(e40, 10), e10);  // a^(2^50 - 1)
        Fe e100 = fe_mul(fe_sq_n(e50, 50), e50); // a^(2^100 - 1)
        Fe e200 = fe_mul(fe_sq_n(e100, 100), e100);
        out_2_250_1 = fe_mul(fe_sq_n(e200, 50), e50);
        out_11 = a11;
    }

    // 1/a by Fermat's little theorem, a^(p - 2) = a^(2^255 - 21); 1/0 is 0
    inline Fe fe_invert(const Fe& a) noexcept {
        Fe e250, a11;
        fe_pow2_250_1(a, e250, a11);
        return fe_mul(fe_sq_n(e250, 5), a11);    // (2^250 - 1)·2^5 + 11
    }

    // a^((p - 5)/8) = a^(2^252 - 3), the exponent of the square root in
    // RFC 8032 §5.1.3
    inline Fe fe_pow22523(const Fe& a) noexcept {
        Fe e250, a11;
        fe_pow2_250_1(a, e250, a11);
        return fe_mul(fe_sq_n(e250, 2), a);      // (2^250 - 1)·4 + 1
    }

    // An element's limbs zeroed with stores the compiler keeps
    inline void fe_wipe(Fe& a) noexcept {
        secure_zero_object(a);
    }
}
