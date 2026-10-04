//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../constant_time.h"
#include "aes_core.h"
#include "words.h"
#include "../../core/detail/bytes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

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
// The path is the key's AES path (GcmKey sets both up together), chosen by
// the processor alone; the state, Y, is the same field element on both.
//
// Portable: the same field and the same folds; the 64-bit carry-less
// product is made of integer multiplications of 32-bit words with holes —
// each operand split into four parts with a bit every fourth place, so
// that a column of the integer product sums at most eight bits and its
// parity, the carry-less bit, is not disturbed by carries from the next
// column. No table and no branch on H or the data.
namespace sgcl::crypto::detail {
    // A block's bits reversed within each byte (a word of eight bytes)
    SGCL_INLINE_HOT uint64_t reverse_bits_in_bytes(uint64_t w) noexcept {
        w = ((w >> 1) & 0x5555555555555555ull) | ((w & 0x5555555555555555ull) << 1);
        w = ((w >> 2) & 0x3333333333333333ull) | ((w & 0x3333333333333333ull) << 2);
        w = ((w >> 4) & 0x0F0F0F0F0F0F0F0Full) | ((w & 0x0F0F0F0F0F0F0F0Full) << 4);
        return w;
    }

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
    SGCL_INLINE_HOT Wide clmul64(uint64_t a, uint64_t b) noexcept {
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

    SGCL_INLINE_HOT Field load_field(const unsigned char* p) noexcept {
        return {reverse_bits_in_bytes(load_le64(p)), reverse_bits_in_bytes(load_le64(p + 8))};
    }

    struct GhashPortableKey {
        Field h;
    };

    SGCL_INLINE_HOT void ghash_init_portable(GhashPortableKey& k, const unsigned char* h) noexcept {
        k.h = load_field(h);
    }

    inline void ghash_blocks_portable(const GhashPortableKey& k, Field& y, const unsigned char* p, size_t n) noexcept {
        for (size_t i = 0; i < n; ++i) {
            Field x = load_field(p + 16 * i);
            y = field_mul({y.lo ^ x.lo, y.hi ^ x.hi}, k.h);
        }
    }

#if defined(SGCL_CRYPTO_ARM64)
    struct GhashArm64Key {
        std::array<uint64x2_t, 8> h;    // h[i] = H^(i+1)
        std::array<uint64x2_t, 8> hs;   // the same with its halves swapped
    };

    // Y as a vector: the field element's words in its two lanes
    SGCL_INLINE_HOT uint64x2_t field_vector(const Field& y) noexcept {
        return vcombine_u64(vcreate_u64(y.lo), vcreate_u64(y.hi));
    }

    SGCL_INLINE_HOT Field field_of(uint64x2_t v) noexcept {
        return {vgetq_lane_u64(v, 0), vgetq_lane_u64(v, 1)};
    }

    SGCL_INLINE_HOT uint64x2_t to_field(uint8x16_t block) noexcept {
        return vreinterpretq_u64_u8(vrbitq_u8(block));
    }

    SGCL_INLINE_HOT uint8x16_t from_field(uint64x2_t v) noexcept {
        return vrbitq_u8(vreinterpretq_u8_u64(v));
    }

    // A product not yet reduced: low, middle and high 128 bits, at x^0,
    // x^64 and x^128
    struct Unreduced {
        uint64x2_t lo, mid, hi;
    };

    SGCL_TARGET_ARM64_CRYPTO
    inline uint64x2_t pmull_lo(uint64x2_t a, uint64x2_t b) noexcept {
        return vreinterpretq_u64_p128(vmull_p64(vgetq_lane_u64(a, 0), vgetq_lane_u64(b, 0)));
    }

    SGCL_TARGET_ARM64_CRYPTO
    inline uint64x2_t pmull_hi(uint64x2_t a, uint64x2_t b) noexcept {
        return vreinterpretq_u64_p128(vmull_high_p64(vreinterpretq_p64_u64(a), vreinterpretq_p64_u64(b)));
    }

    // a·h as four products of halves: a0·h0, a1·h1, and the middle
    // a0·h1 + a1·h0 from h with its halves swapped (hs), so that no block
    // needs its halves moved (Karatsuba's three products need an EXT and
    // an XOR per block, which cost more here than the fourth PMULL)
    SGCL_TARGET_ARM64_CRYPTO
    inline void mul_add(Unreduced& acc, uint64x2_t a, uint64x2_t h, uint64x2_t hs) noexcept {
        acc.lo = veorq_u64(acc.lo, pmull_lo(a, h));
        acc.hi = veorq_u64(acc.hi, pmull_hi(a, h));
        acc.mid = veorq_u64(acc.mid, veorq_u64(pmull_lo(a, hs), pmull_hi(a, hs)));
    }

    SGCL_TARGET_ARM64_CRYPTO
    inline void mul_first(Unreduced& acc, uint64x2_t a, uint64x2_t h, uint64x2_t hs) noexcept {
        acc.lo = pmull_lo(a, h);
        acc.hi = pmull_hi(a, h);
        acc.mid = veorq_u64(pmull_lo(a, hs), pmull_hi(a, hs));
    }

    // The 256-bit product as words p0..p3 is lo ^ mid·x^64 ^ hi·x^128;
    // p3·x^192 folds onto p1, p2 as p3·0x87·x^64, then p2·x^128 onto p0,
    // p1 as p2·0x87
    SGCL_TARGET_ARM64_CRYPTO
    inline uint64x2_t reduce(const Unreduced& acc) noexcept {
        const uint64x2_t poly = vdupq_n_u64(0x87);
        uint64x2_t x = veorq_u64(vextq_u64(acc.lo, acc.hi, 1), acc.mid);   // [p1, p2]
        x = veorq_u64(x, pmull_hi(acc.hi, poly));                           // p3·0x87 onto [p1, p2]
        uint64x2_t u = pmull_hi(x, poly);                                   // p2·0x87
        return veorq_u64(vzip1q_u64(acc.lo, x), u);                         // [p0, p1] ^ it
    }

    SGCL_INLINE_HOT uint64x2_t swap_halves(uint64x2_t v) noexcept {
        return vextq_u64(v, v, 1);
    }

    SGCL_TARGET_ARM64_CRYPTO
    inline uint64x2_t field_mul_arm64(uint64x2_t a, uint64x2_t b) noexcept {
        Unreduced acc;
        mul_first(acc, a, b, swap_halves(b));
        return reduce(acc);
    }

    SGCL_TARGET_ARM64_CRYPTO
    inline void ghash_init_arm64(GhashArm64Key& k, const unsigned char* h) noexcept {
        uint64x2_t h1 = to_field(vld1q_u8(h));
        uint64x2_t p = h1;
        for (int i = 0; i < 8; ++i) {
            k.h[i] = p;
            k.hs[i] = swap_halves(p);
            p = field_mul_arm64(p, h1);
        }
    }

    // n blocks (1..8) already in field form, x[0] carrying Y: the first
    // times H^n, the last times H
    SGCL_TARGET_ARM64_CRYPTO
    inline uint64x2_t ghash_many(const GhashArm64Key& k, const uint64x2_t* x, unsigned n) noexcept {
        Unreduced acc;
        mul_first(acc, x[0], k.h[n - 1], k.hs[n - 1]);
        for (unsigned i = 1; i < n; ++i) {
            mul_add(acc, x[i], k.h[n - 1 - i], k.hs[n - 1 - i]);
        }
        return reduce(acc);
    }

    // Eight blocks from registers (the ciphertext a seal has just made)
    SGCL_TARGET_ARM64_CRYPTO
    inline void ghash_eight(const GhashArm64Key& k, uint64x2_t& y, const uint8x16_t* blocks) noexcept {
        Unreduced acc;
        mul_first(acc, veorq_u64(to_field(blocks[0]), y), k.h[7], k.hs[7]);
        for (int i = 1; i < 8; ++i) {
            mul_add(acc, to_field(blocks[i]), k.h[7 - i], k.hs[7 - i]);
        }
        y = reduce(acc);
    }

    SGCL_TARGET_ARM64_CRYPTO
    inline void ghash_blocks_arm64(const GhashArm64Key& k, Field& state, const unsigned char* p, size_t n) noexcept {
        uint64x2_t y = field_vector(state);
        while (n >= 8) {
            uint8x16_t b[8];
            for (int i = 0; i < 8; ++i) {
                b[i] = vld1q_u8(p + 16 * i);
            }
            ghash_eight(k, y, b);
            p += 128;
            n -= 8;
        }
        if (n > 0) {
            uint64x2_t x[8];
            for (size_t i = 0; i < n; ++i) {
                x[i] = to_field(vld1q_u8(p + 16 * i));
            }
            x[0] = veorq_u64(x[0], y);
            y = ghash_many(k, x, unsigned(n));
        }
        state = field_of(y);
    }
#endif

#if defined(SGCL_CRYPTO_X86)
    // x86-64: PCLMULQDQ multiplies 64 by 64 bits carry-less, the halves
    // chosen by its immediate, so the middle products need no swapped key;
    // eight blocks at once against H^8..H and one reduction, the same folds
    // as arm64's. A block's bits reversed within its bytes by two lookups of
    // a nibble table (PSHUFB).
    struct GhashX86Key {
        std::array<__m128i, 8> h;   // h[i] = H^(i+1)
    };

    struct UnreducedX86 {
        __m128i lo, mid, hi;
    };

    SGCL_INLINE_X86_AES
    inline __m128i to_field_x86(__m128i b) noexcept {
        const __m128i table = _mm_setr_epi8(0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE, 0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF);
        const __m128i nibble = _mm_set1_epi8(0x0F);
        const __m128i lo = _mm_shuffle_epi8(table, _mm_and_si128(b, nibble));
        const __m128i hi = _mm_shuffle_epi8(table, _mm_and_si128(_mm_srli_epi16(b, 4), nibble));
        return _mm_or_si128(_mm_slli_epi16(lo, 4), hi);
    }

    SGCL_INLINE_X86_AES
    inline __m128i field_vector_x86(const Field& y) noexcept {
        return _mm_set_epi64x(int64_t(y.hi), int64_t(y.lo));
    }

    SGCL_INLINE_X86_AES
    inline Field field_of_x86(__m128i v) noexcept {
        return {uint64_t(_mm_cvtsi128_si64(v)), uint64_t(_mm_extract_epi64(v, 1))};
    }

    SGCL_INLINE_X86_AES
    inline void mul_first_x86(UnreducedX86& acc, __m128i a, __m128i h) noexcept {
        acc.lo = _mm_clmulepi64_si128(a, h, 0x00);
        acc.hi = _mm_clmulepi64_si128(a, h, 0x11);
        acc.mid = _mm_xor_si128(_mm_clmulepi64_si128(a, h, 0x01), _mm_clmulepi64_si128(a, h, 0x10));
    }

    SGCL_INLINE_X86_AES
    inline void mul_add_x86(UnreducedX86& acc, __m128i a, __m128i h) noexcept {
        acc.lo = _mm_xor_si128(acc.lo, _mm_clmulepi64_si128(a, h, 0x00));
        acc.hi = _mm_xor_si128(acc.hi, _mm_clmulepi64_si128(a, h, 0x11));
        acc.mid = _mm_xor_si128(acc.mid, _mm_xor_si128(_mm_clmulepi64_si128(a, h, 0x01), _mm_clmulepi64_si128(a, h, 0x10)));
    }

    // The folds of reduce() above: [p1, p2] ^= p3·0x87, then [p0, p1] ^= p2·0x87
    SGCL_INLINE_X86_AES
    inline __m128i reduce_x86(const UnreducedX86& acc) noexcept {
        const __m128i poly = _mm_set_epi64x(0, 0x87);
        __m128i x = _mm_xor_si128(_mm_alignr_epi8(acc.hi, acc.lo, 8), acc.mid);   // [p1, p2]
        x = _mm_xor_si128(x, _mm_clmulepi64_si128(acc.hi, poly, 0x01));          // p3·0x87 onto [p1, p2]
        const __m128i u = _mm_clmulepi64_si128(x, poly, 0x01);                    // p2·0x87
        return _mm_xor_si128(_mm_unpacklo_epi64(acc.lo, x), u);                   // [p0, p1] ^ it
    }

    SGCL_TARGET_X86_AES
    inline void ghash_init_x86(GhashX86Key& k, const unsigned char* h) noexcept {
        const __m128i h1 = to_field_x86(_mm_loadu_si128(reinterpret_cast<const __m128i*>(h)));
        __m128i p = h1;
        for (int i = 0; i < 8; ++i) {
            k.h[i] = p;
            UnreducedX86 acc;
            mul_first_x86(acc, p, h1);
            p = reduce_x86(acc);
        }
    }

    // n blocks (1..8) in field form, x[0] carrying Y: the first times H^n
    SGCL_TARGET_X86_AES
    inline __m128i ghash_many_x86(const GhashX86Key& k, const __m128i* x, unsigned n) noexcept {
        UnreducedX86 acc;
        mul_first_x86(acc, x[0], k.h[n - 1]);
        for (unsigned i = 1; i < n; ++i) {
            mul_add_x86(acc, x[i], k.h[n - 1 - i]);
        }
        return reduce_x86(acc);
    }

    // Eight blocks from registers (the ciphertext a seal has just made)
    SGCL_INLINE_X86_AES
    inline void ghash_eight_x86(const GhashX86Key& k, __m128i& y, const __m128i* blocks) noexcept {
        UnreducedX86 acc;
        mul_first_x86(acc, _mm_xor_si128(to_field_x86(blocks[0]), y), k.h[7]);
        for (int i = 1; i < 8; ++i) {
            mul_add_x86(acc, to_field_x86(blocks[i]), k.h[7 - i]);
        }
        y = reduce_x86(acc);
    }

    SGCL_TARGET_X86_AES
    inline void ghash_blocks_x86(const GhashX86Key& k, Field& state, const unsigned char* p, size_t n) noexcept {
        __m128i y = field_vector_x86(state);
        while (n >= 8) {
            __m128i b[8];
            for (int i = 0; i < 8; ++i) {
                b[i] = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + 16 * i));
            }
            ghash_eight_x86(k, y, b);
            p += 128;
            n -= 8;
        }
        if (n > 0) {
            __m128i x[8];
            for (size_t i = 0; i < n; ++i) {
                x[i] = to_field_x86(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p + 16 * i)));
            }
            x[0] = _mm_xor_si128(x[0], y);
            y = ghash_many_x86(k, x, unsigned(n));
        }
        state = field_of_x86(y);
    }
#endif

    // A key and its path, the path its AES key took (GcmKey)
    struct GhashKey {
        AesPath path = AesPath::portable;
        union {
            GhashPortableKey portable;
#if defined(SGCL_CRYPTO_ARM64)
            GhashArm64Key arm64;
#endif
#if defined(SGCL_CRYPTO_X86)
            GhashX86Key x86;
#endif
        };

        SGCL_INLINE_HOT GhashKey() noexcept
        : portable() {
        }
    };

    // Y, the same element of the field on every path
    struct GhashState {
        Field y;
    };

    inline void ghash_init(GhashKey& k, const unsigned char* h, AesPath path) noexcept {
#if defined(SGCL_CRYPTO_ARM64)
        if (path == AesPath::arm64) {
            k.path = path;
            k.arm64 = GhashArm64Key{};
            ghash_init_arm64(k.arm64, h);
            return;
        }
#endif
#if defined(SGCL_CRYPTO_X86)
        if (path == AesPath::x86) {
            k.path = path;
            k.x86 = GhashX86Key{};
            ghash_init_x86(k.x86, h);
            return;
        }
#endif
        (void)path;
        k.path = AesPath::portable;
        k.portable = GhashPortableKey{};
        ghash_init_portable(k.portable, h);
    }

    SGCL_INLINE_HOT void ghash_start(GhashState& s) noexcept {
        s.y = {0, 0};
    }

    SGCL_INLINE_HOT void ghash_blocks(const GhashKey& k, GhashState& s, const unsigned char* p, size_t n) noexcept {
#if defined(SGCL_CRYPTO_ARM64)
        if (k.path == AesPath::arm64) {
            ghash_blocks_arm64(k.arm64, s.y, p, n);
            return;
        }
#endif
#if defined(SGCL_CRYPTO_X86)
        if (k.path == AesPath::x86) {
            ghash_blocks_x86(k.x86, s.y, p, n);
            return;
        }
#endif
        ghash_blocks_portable(k.portable, s.y, p, n);
    }

    SGCL_INLINE_HOT void ghash_finish(const GhashState& s, unsigned char* out) noexcept {
        store_le64(out, reverse_bits_in_bytes(s.y.lo));
        store_le64(out + 8, reverse_bits_in_bytes(s.y.hi));
    }

    // The bytes that do not fill a block, padded with zeros (§7.1: the
    // AAD and the ciphertext are each padded to a whole block)
    inline void ghash_padded(const GhashKey& k, GhashState& s, const unsigned char* p, size_t n) noexcept {
        size_t whole = n & ~size_t(15);
        ghash_blocks(k, s, p, whole / 16);
        if (n > whole) {
            unsigned char last[16] = {};
            sgcl::detail::copy_bytes(last, p + whole, n - whole);
            ghash_blocks(k, s, last, 1);
            secure_zero(last, sizeof last);
        }
    }

    // The last block: the lengths of the AAD and of the ciphertext in bits,
    // each a 64-bit big-endian number
    SGCL_INLINE_HOT void ghash_lengths(const GhashKey& k, GhashState& s, uint64_t aad_size, uint64_t text_size) noexcept {
        unsigned char block[16];
        store_be64(block, aad_size * 8);
        store_be64(block + 8, text_size * 8);
        ghash_blocks(k, s, block, 1);
    }
}
