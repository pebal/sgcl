//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../constant_time.h"
#include "words.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#if SGCL_CRYPTO_ARM64_AES
#include <arm_neon.h>
#endif

// GHASH (SP 800-38D §6.4): Y = (Y ^ X)·H for each block X, in GF(2^128)
// modulo x^128 + x^7 + x^2 + x + 1. GCM writes a field element with the
// coefficient of x^0 in the high bit of the first byte; with the bits of
// every byte reversed, the 16 bytes read as a little-endian 128-bit number
// have the coefficient of x^i in bit i, and the arithmetic is the plain
// kind: a carry-less product of two such numbers is the product of the
// polynomials, and its upper half folds down by x^128 = x^7 + x^2 + x + 1,
// which is a carry-less multiplication by 0x87, twice (the first fold
// leaves up to seven bits above 128). Each block's bytes are reversed on
// the way in (RBIT on arm64, three masked swaps in C++), the result on the
// way out.
//
// arm64: PMULL multiplies 64 by 64 bits carry-less; a 128-bit product is
// four of them, the middle two against the power of H with its halves
// swapped, computed with the key (Karatsuba's three need the halves of
// every block XORed, an EXT and an EOR, which measured slower). Eight
// blocks are taken at once: Y' = (Y ^ X1)·H^8 ^ X2·H^7 ^ ... ^ X8·H, the
// eight products summed unreduced and reduced once, so that the eight
// chains run in parallel and the reduction costs an eighth. The powers
// H..H^8 are computed with the key.
//
// Portable: the same field and the same folds; the 64-bit carry-less
// product is made of integer multiplications of 32-bit words with holes —
// each operand split into four parts with a bit every fourth place, so
// that a column of the integer product sums at most eight bits and its
// parity, the carry-less bit, is not disturbed by carries from the next
// column. No table and no branch on H or the data.
namespace sgcl::crypto::detail {
    // A block's bits reversed within each byte (a word of eight bytes)
    inline uint64_t reverse_bits_in_bytes(uint64_t w) noexcept {
        w = ((w >> 1) & 0x5555555555555555ull) | ((w & 0x5555555555555555ull) << 1);
        w = ((w >> 2) & 0x3333333333333333ull) | ((w & 0x3333333333333333ull) << 2);
        w = ((w >> 4) & 0x0F0F0F0F0F0F0F0Full) | ((w & 0x0F0F0F0F0F0F0F0Full) << 4);
        return w;
    }

#if SGCL_CRYPTO_ARM64_AES
    struct GhashKey {
        std::array<uint64x2_t, 8> h;    // h[i] = H^(i+1)
        std::array<uint64x2_t, 8> hs;   // the same with its halves swapped
    };

    struct GhashState {
        uint64x2_t y;
    };

    inline uint64x2_t to_field(uint8x16_t block) noexcept {
        return vreinterpretq_u64_u8(vrbitq_u8(block));
    }

    inline uint8x16_t from_field(uint64x2_t v) noexcept {
        return vrbitq_u8(vreinterpretq_u8_u64(v));
    }

    // A product not yet reduced: low, middle and high 128 bits, at x^0,
    // x^64 and x^128
    struct Unreduced {
        uint64x2_t lo, mid, hi;
    };

    inline uint64x2_t pmull_lo(uint64x2_t a, uint64x2_t b) noexcept {
        return vreinterpretq_u64_p128(vmull_p64(vgetq_lane_u64(a, 0), vgetq_lane_u64(b, 0)));
    }

    inline uint64x2_t pmull_hi(uint64x2_t a, uint64x2_t b) noexcept {
        return vreinterpretq_u64_p128(vmull_high_p64(vreinterpretq_p64_u64(a), vreinterpretq_p64_u64(b)));
    }

    // a·h as four products of halves: a0·h0, a1·h1, and the middle
    // a0·h1 + a1·h0 from h with its halves swapped (hs), so that no block
    // needs its halves moved (Karatsuba's three products need an EXT and
    // an XOR per block, which cost more here than the fourth PMULL)
    inline void mul_add(Unreduced& acc, uint64x2_t a, uint64x2_t h, uint64x2_t hs) noexcept {
        acc.lo = veorq_u64(acc.lo, pmull_lo(a, h));
        acc.hi = veorq_u64(acc.hi, pmull_hi(a, h));
        acc.mid = veorq_u64(acc.mid, veorq_u64(pmull_lo(a, hs), pmull_hi(a, hs)));
    }

    inline void mul_first(Unreduced& acc, uint64x2_t a, uint64x2_t h, uint64x2_t hs) noexcept {
        acc.lo = pmull_lo(a, h);
        acc.hi = pmull_hi(a, h);
        acc.mid = veorq_u64(pmull_lo(a, hs), pmull_hi(a, hs));
    }

    // The 256-bit product as words p0..p3 is lo ^ mid·x^64 ^ hi·x^128;
    // p3·x^192 folds onto p1, p2 as p3·0x87·x^64, then p2·x^128 onto p0,
    // p1 as p2·0x87
    inline uint64x2_t reduce(const Unreduced& acc) noexcept {
        const uint64x2_t poly = vdupq_n_u64(0x87);
        uint64x2_t x = veorq_u64(vextq_u64(acc.lo, acc.hi, 1), acc.mid);   // [p1, p2]
        x = veorq_u64(x, pmull_hi(acc.hi, poly));                           // p3·0x87 onto [p1, p2]
        uint64x2_t u = pmull_hi(x, poly);                                   // p2·0x87
        return veorq_u64(vzip1q_u64(acc.lo, x), u);                         // [p0, p1] ^ it
    }

    inline uint64x2_t swap_halves(uint64x2_t v) noexcept {
        return vextq_u64(v, v, 1);
    }

    inline uint64x2_t field_mul(uint64x2_t a, uint64x2_t b) noexcept {
        Unreduced acc;
        mul_first(acc, a, b, swap_halves(b));
        return reduce(acc);
    }

    inline void ghash_init(GhashKey& k, const unsigned char* h) noexcept {
        uint64x2_t h1 = to_field(vld1q_u8(h));
        uint64x2_t p = h1;
        for (int i = 0; i < 8; ++i) {
            k.h[i] = p;
            k.hs[i] = swap_halves(p);
            p = field_mul(p, h1);
        }
    }

    inline void ghash_start(GhashState& s) noexcept {
        s.y = vdupq_n_u64(0);
    }

    // n blocks (1..8) already in field form, x[0] carrying Y: the first
    // times H^n, the last times H
    inline uint64x2_t ghash_many(const GhashKey& k, const uint64x2_t* x, unsigned n) noexcept {
        Unreduced acc;
        mul_first(acc, x[0], k.h[n - 1], k.hs[n - 1]);
        for (unsigned i = 1; i < n; ++i) {
            mul_add(acc, x[i], k.h[n - 1 - i], k.hs[n - 1 - i]);
        }
        return reduce(acc);
    }

    // Eight blocks from registers (the ciphertext a seal has just made)
    inline void ghash_eight(const GhashKey& k, GhashState& s, const uint8x16_t* blocks) noexcept {
        Unreduced acc;
        mul_first(acc, veorq_u64(to_field(blocks[0]), s.y), k.h[7], k.hs[7]);
        for (int i = 1; i < 8; ++i) {
            mul_add(acc, to_field(blocks[i]), k.h[7 - i], k.hs[7 - i]);
        }
        s.y = reduce(acc);
    }

    inline void ghash_blocks(const GhashKey& k, GhashState& s, const unsigned char* p, size_t n) noexcept {
        while (n >= 8) {
            uint8x16_t b[8];
            for (int i = 0; i < 8; ++i) {
                b[i] = vld1q_u8(p + 16 * i);
            }
            ghash_eight(k, s, b);
            p += 128;
            n -= 8;
        }
        if (n > 0) {
            uint64x2_t x[8];
            for (size_t i = 0; i < n; ++i) {
                x[i] = to_field(vld1q_u8(p + 16 * i));
            }
            x[0] = veorq_u64(x[0], s.y);
            s.y = ghash_many(k, x, unsigned(n));
        }
    }

    inline void ghash_finish(const GhashState& s, unsigned char* out) noexcept {
        vst1q_u8(out, from_field(s.y));
    }
#else
    // 32 by 32 bits carry-less, by integer products of operands with holes
    inline uint64_t clmul32(uint32_t x, uint32_t y) noexcept {
        const uint64_t x0 = x & 0x11111111u, x1 = x & 0x22222222u, x2 = x & 0x44444444u, x3 = x & 0x88888888u;
        const uint64_t y0 = y & 0x11111111u, y1 = y & 0x22222222u, y2 = y & 0x44444444u, y3 = y & 0x88888888u;
        uint64_t z0 = (x0 * y0) ^ (x1 * y3) ^ (x2 * y2) ^ (x3 * y1);
        uint64_t z1 = (x0 * y1) ^ (x1 * y0) ^ (x2 * y3) ^ (x3 * y2);
        uint64_t z2 = (x0 * y2) ^ (x1 * y1) ^ (x2 * y0) ^ (x3 * y3);
        uint64_t z3 = (x0 * y3) ^ (x1 * y2) ^ (x2 * y1) ^ (x3 * y0);
        return (z0 & 0x1111111111111111ull) | (z1 & 0x2222222222222222ull) | (z2 & 0x4444444444444444ull) | (z3 & 0x8888888888888888ull);
    }

    // 64 by 64 bits carry-less (Karatsuba over the 32-bit halves)
    inline Wide clmul64(uint64_t a, uint64_t b) noexcept {
        uint32_t a0 = uint32_t(a), a1 = uint32_t(a >> 32), b0 = uint32_t(b), b1 = uint32_t(b >> 32);
        uint64_t lo = clmul32(a0, b0);
        uint64_t hi = clmul32(a1, b1);
        uint64_t mid = clmul32(a0 ^ a1, b0 ^ b1) ^ lo ^ hi;
        return {lo ^ (mid << 32), hi ^ (mid >> 32)};
    }

    struct Field {
        uint64_t lo, hi;   // coefficient of x^i in bit i of lo | hi << 64
    };

    inline Field field_mul(Field a, Field b) noexcept {
        Wide lo = clmul64(a.lo, b.lo);
        Wide hi = clmul64(a.hi, b.hi);
        Wide mid = clmul64(a.lo ^ a.hi, b.lo ^ b.hi);
        mid.lo ^= lo.lo ^ hi.lo;
        mid.hi ^= lo.hi ^ hi.hi;
        uint64_t p0 = lo.lo, p1 = lo.hi ^ mid.lo, p2 = hi.lo ^ mid.hi, p3 = hi.hi;
        // p3·x^192 ≡ p3·0x87·x^64, then p2·x^128 ≡ p2·0x87
        p1 ^= p3 ^ (p3 << 1) ^ (p3 << 2) ^ (p3 << 7);
        p2 ^= (p3 >> 63) ^ (p3 >> 62) ^ (p3 >> 57);
        p0 ^= p2 ^ (p2 << 1) ^ (p2 << 2) ^ (p2 << 7);
        p1 ^= (p2 >> 63) ^ (p2 >> 62) ^ (p2 >> 57);
        return {p0, p1};
    }

    inline Field load_field(const unsigned char* p) noexcept {
        return {reverse_bits_in_bytes(load_le64(p)), reverse_bits_in_bytes(load_le64(p + 8))};
    }

    struct GhashKey {
        Field h;
    };

    struct GhashState {
        Field y;
    };

    inline void ghash_init(GhashKey& k, const unsigned char* h) noexcept {
        k.h = load_field(h);
    }

    inline void ghash_start(GhashState& s) noexcept {
        s.y = {0, 0};
    }

    inline void ghash_blocks(const GhashKey& k, GhashState& s, const unsigned char* p, size_t n) noexcept {
        for (size_t i = 0; i < n; ++i) {
            Field x = load_field(p + 16 * i);
            s.y = field_mul({s.y.lo ^ x.lo, s.y.hi ^ x.hi}, k.h);
        }
    }

    inline void ghash_finish(const GhashState& s, unsigned char* out) noexcept {
        store_le64(out, reverse_bits_in_bytes(s.y.lo));
        store_le64(out + 8, reverse_bits_in_bytes(s.y.hi));
    }
#endif

    // The bytes that do not fill a block, padded with zeros (§7.1: the
    // AAD and the ciphertext are each padded to a whole block)
    inline void ghash_padded(const GhashKey& k, GhashState& s, const unsigned char* p, size_t n) noexcept {
        size_t whole = n & ~size_t(15);
        ghash_blocks(k, s, p, whole / 16);
        if (n > whole) {
            unsigned char last[16] = {};
            std::memcpy(last, p + whole, n - whole);
            ghash_blocks(k, s, last, 1);
            secure_zero(last, sizeof last);
        }
    }

    // The last block: the lengths of the AAD and of the ciphertext in bits,
    // each a 64-bit big-endian number
    inline void ghash_lengths(const GhashKey& k, GhashState& s, uint64_t aad_size, uint64_t text_size) noexcept {
        unsigned char block[16];
        store_be64(block, aad_size * 8);
        store_be64(block + 8, text_size * 8);
        ghash_blocks(k, s, block, 1);
    }
}
