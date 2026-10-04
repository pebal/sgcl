//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../constant_time.h"
#include "words.h"

#include <array>
#include <cstddef>
#include <cstdint>

// AES in constant time without the instructions: bitsliced, from FIPS 197.
// A table-driven AES (the T tables, or the S-box as a table of 256 bytes)
// reads memory at addresses that depend on the key and the data, and the
// cache tells a process on the same machine which lines were read; this
// path reads no table at all. Four blocks (64 bytes) are held as eight
// 64-bit planes, plane b holding bit b of each of the 64 bytes (bit n of a
// plane is byte n: block n / 16, byte n % 16 of it, so a block is a 16-bit
// lane). Every step of a round is then a fixed sequence of ANDs, XORs and
// shifts over the planes, the same whatever the values:
//
// - SubBytes computes the S-box as FIPS 197 defines it, the multiplicative
//   inverse in GF(2^8) followed by the affine map; the inverse is x^254,
//   four multiplications and seven squarings of polynomials over the planes
//   (a squaring is linear: a spreading of the planes and a reduction).
//   Slower than a hand-minimized circuit, and correct by construction.
// - ShiftRows rotates each row's bits within a block's lane; MixColumns
//   rotates the bytes of each column (nibbles of a lane) and multiplies by
//   x with a fixed pattern of XORs of planes.
//
// The key schedule uses the same S-box one byte at a time, computed with
// masks rather than branches, and stores each round key bitsliced once,
// repeated for the four blocks.
namespace sgcl::crypto::detail {
    // --- GF(2^8) one byte at a time, for the key schedule and the tests ---

    // a·b modulo x^8 + x^4 + x^3 + x + 1, with masks in place of the
    // branches on the bits
    inline uint8_t gf_mul(uint8_t a, uint8_t b) noexcept {
        uint8_t r = 0;
        for (int i = 0; i < 8; ++i) {
            r ^= uint8_t(a & uint8_t(0u - (b & 1u)));
            uint8_t carry = uint8_t(0u - (a >> 7));
            a = uint8_t((a << 1) ^ (carry & 0x1b));
            b >>= 1;
        }
        return r;
    }

    SGCL_INLINE_HOT uint8_t rotl8(uint8_t v, int n) noexcept {
        return uint8_t(v << n | v >> (8 - n));
    }

    // FIPS 197 §5.1.1: the inverse (0 for 0, which x^254 gives) and the
    // affine map with the constant 0x63
    inline uint8_t sbox_byte(uint8_t x) noexcept {
        uint8_t x2 = gf_mul(x, x);
        uint8_t x3 = gf_mul(x2, x);
        uint8_t x6 = gf_mul(x3, x3);
        uint8_t x12 = gf_mul(x6, x6);
        uint8_t x15 = gf_mul(x12, x3);
        uint8_t x30 = gf_mul(x15, x15);
        uint8_t x60 = gf_mul(x30, x30);
        uint8_t x120 = gf_mul(x60, x60);
        uint8_t x240 = gf_mul(x120, x120);
        uint8_t x252 = gf_mul(x240, x12);
        uint8_t y = gf_mul(x252, x2);
        return uint8_t(y ^ rotl8(y, 1) ^ rotl8(y, 2) ^ rotl8(y, 3) ^ rotl8(y, 4) ^ 0x63);
    }

    // SubWord of the key schedule, the four bytes of a word at once
    SGCL_INLINE_HOT uint32_t sub_word_portable(uint32_t w) noexcept {
        return uint32_t(sbox_byte(uint8_t(w))) | uint32_t(sbox_byte(uint8_t(w >> 8))) << 8 | uint32_t(sbox_byte(uint8_t(w >> 16))) << 16 | uint32_t(sbox_byte(uint8_t(w >> 24))) << 24;
    }

    // --- the planes ---

    using Planes = std::array<uint64_t, 8>;

    // An 8x8 matrix of bits transposed, byte i the row i: bit j of byte i
    // becomes bit i of byte j (three exchanges of blocks: 1x1, 2x2, 4x4)
    inline uint64_t transpose_bits8(uint64_t x) noexcept {
        uint64_t t;
        t = (x ^ (x >> 7)) & 0x00AA00AA00AA00AAull;
        x ^= t ^ (t << 7);
        t = (x ^ (x >> 14)) & 0x0000CCCC0000CCCCull;
        x ^= t ^ (t << 14);
        t = (x ^ (x >> 28)) & 0x00000000F0F0F0F0ull;
        x ^= t ^ (t << 28);
        return x;
    }

    // An 8x8 matrix of bytes transposed, word k the row k: byte b of word
    // k becomes byte k of word b
    inline void transpose_bytes8(uint64_t (&w)[8]) noexcept {
        uint64_t r[8];
        for (int b = 0; b < 8; ++b) {
            uint64_t v = 0;
            for (int k = 0; k < 8; ++k) {
                v |= ((w[k] >> (8 * b)) & 0xff) << (8 * k);
            }
            r[b] = v;
        }
        for (int b = 0; b < 8; ++b) {
            w[b] = r[b];
        }
    }

    // 64 bytes into planes: word k (bytes 8k..8k+7) transposed as bits
    // gives, in its byte b, bit b of those eight bytes; the bytes b of the
    // eight words are plane b
    inline void bitslice(const unsigned char* in, Planes& s) noexcept {
        uint64_t w[8];
        for (int k = 0; k < 8; ++k) {
            w[k] = transpose_bits8(load_le64(in + 8 * k));
        }
        transpose_bytes8(w);
        for (int b = 0; b < 8; ++b) {
            s[b] = w[b];
        }
    }

    // The way back: both transpositions are their own inverses
    inline void unbitslice(const Planes& s, unsigned char* out) noexcept {
        uint64_t w[8];
        for (int b = 0; b < 8; ++b) {
            w[b] = s[b];
        }
        transpose_bytes8(w);
        for (int k = 0; k < 8; ++k) {
            store_le64(out + 8 * k, transpose_bits8(w[k]));
        }
    }

    // a·b in GF(2^8), 64 products at once: the schoolbook product of two
    // polynomials of degree 7 (columns c0..c14), then x^8 = x^4 + x^3 + x + 1
    // folds the columns 14..8 down. The fold is linear, so each output
    // plane is a fixed XOR of columns, worked out once (fold c14 onto c10,
    // c9, c7, c6, then c13 and so on down to c8) and written out: straight
    // code, which the compiler keeps in registers
    inline void bs_mul(const Planes& a, const Planes& b, Planes& r) noexcept {
        const uint64_t a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3], a4 = a[4], a5 = a[5], a6 = a[6], a7 = a[7];
        const uint64_t b0 = b[0], b1 = b[1], b2 = b[2], b3 = b[3], b4 = b[4], b5 = b[5], b6 = b[6], b7 = b[7];
        const uint64_t c0 = (a0 & b0);
        const uint64_t c1 = (a0 & b1) ^ (a1 & b0);
        const uint64_t c2 = (a0 & b2) ^ (a1 & b1) ^ (a2 & b0);
        const uint64_t c3 = (a0 & b3) ^ (a1 & b2) ^ (a2 & b1) ^ (a3 & b0);
        const uint64_t c4 = (a0 & b4) ^ (a1 & b3) ^ (a2 & b2) ^ (a3 & b1) ^ (a4 & b0);
        const uint64_t c5 = (a0 & b5) ^ (a1 & b4) ^ (a2 & b3) ^ (a3 & b2) ^ (a4 & b1) ^ (a5 & b0);
        const uint64_t c6 = (a0 & b6) ^ (a1 & b5) ^ (a2 & b4) ^ (a3 & b3) ^ (a4 & b2) ^ (a5 & b1) ^ (a6 & b0);
        const uint64_t c7 = (a0 & b7) ^ (a1 & b6) ^ (a2 & b5) ^ (a3 & b4) ^ (a4 & b3) ^ (a5 & b2) ^ (a6 & b1) ^ (a7 & b0);
        const uint64_t c8 = (a1 & b7) ^ (a2 & b6) ^ (a3 & b5) ^ (a4 & b4) ^ (a5 & b3) ^ (a6 & b2) ^ (a7 & b1);
        const uint64_t c9 = (a2 & b7) ^ (a3 & b6) ^ (a4 & b5) ^ (a5 & b4) ^ (a6 & b3) ^ (a7 & b2);
        const uint64_t c10 = (a3 & b7) ^ (a4 & b6) ^ (a5 & b5) ^ (a6 & b4) ^ (a7 & b3);
        const uint64_t c11 = (a4 & b7) ^ (a5 & b6) ^ (a6 & b5) ^ (a7 & b4);
        const uint64_t c12 = (a5 & b7) ^ (a6 & b6) ^ (a7 & b5);
        const uint64_t c13 = (a6 & b7) ^ (a7 & b6);
        const uint64_t c14 = (a7 & b7);
        r[0] = c0 ^ c8 ^ c12 ^ c13;
        r[1] = c1 ^ c8 ^ c9 ^ c12 ^ c14;
        r[2] = c2 ^ c9 ^ c10 ^ c13;
        r[3] = c3 ^ c8 ^ c10 ^ c11 ^ c12 ^ c13 ^ c14;
        r[4] = c4 ^ c8 ^ c9 ^ c11 ^ c14;
        r[5] = c5 ^ c9 ^ c10 ^ c12;
        r[6] = c6 ^ c10 ^ c11 ^ c13;
        r[7] = c7 ^ c11 ^ c12 ^ c14;
    }

    // a² in GF(2^8): squaring is linear over GF(2) (plane i moves to column
    // 2i), and with the same fold each output plane is a fixed XOR of input
    // planes
    inline void bs_square(const Planes& a, Planes& r) noexcept {
        const uint64_t s0 = a[0] ^ a[4] ^ a[6], s1 = a[4] ^ a[6] ^ a[7], s2 = a[1] ^ a[5], s3 = a[4] ^ a[5] ^ a[6] ^ a[7], s4 = a[2] ^ a[4] ^ a[7], s5 = a[5] ^ a[6], s6 = a[3] ^ a[5], s7 = a[6] ^ a[7];
        r[0] = s0;
        r[1] = s1;
        r[2] = s2;
        r[3] = s3;
        r[4] = s4;
        r[5] = s5;
        r[6] = s6;
        r[7] = s7;
    }

    // x^254, the inverse of x (and 0 for 0), by the chain 2, 3, 6, 12, 15,
    // 30, 60, 120, 240, 252, 254
    inline void bs_inverse(Planes& x) noexcept {
        Planes x2, x3, x12, t;
        bs_square(x, x2);
        bs_mul(x2, x, x3);
        bs_square(x3, t);
        bs_square(t, x12);
        bs_mul(x12, x3, t);   // x^15
        bs_square(t, t);
        bs_square(t, t);
        bs_square(t, t);
        bs_square(t, t);      // x^240
        bs_mul(t, x12, t);    // x^252
        bs_mul(t, x2, x);     // x^254
    }

    // FIPS 197 §5.1.1, bit i of the output: bits i, i-4..i-1 of the input
    // and bit i of 0x63 (planes 0, 1, 5 and 6 complemented)
    inline void bs_sub_bytes(Planes& s) noexcept {
        bs_inverse(s);
        Planes y = s;
        for (int i = 0; i < 8; ++i) {
            s[i] = y[i] ^ y[(i + 7) & 7] ^ y[(i + 6) & 7] ^ y[(i + 5) & 7] ^ y[(i + 4) & 7];
        }
        s[0] = ~s[0];
        s[1] = ~s[1];
        s[5] = ~s[5];
        s[6] = ~s[6];
    }

    // §5.3.2: the inverse of the affine map (bits i-1, i-3, i-6 and 0x05),
    // then the inverse in the field, which is its own inverse
    inline void bs_inv_sub_bytes(Planes& s) noexcept {
        Planes y = s;
        for (int i = 0; i < 8; ++i) {
            s[i] = y[(i + 7) & 7] ^ y[(i + 5) & 7] ^ y[(i + 2) & 7];
        }
        s[0] = ~s[0];
        s[2] = ~s[2];
        bs_inverse(s);
    }

    // The bits of each 16-bit lane rotated right by k (1..15): lane bit p
    // takes lane bit p + k, the high k bits take the low ones
    SGCL_INLINE_HOT uint64_t rotr_lanes16(uint64_t x, int k) noexcept {
        uint64_t low = 0x0001000100010001ull * ((uint64_t(1) << (16 - k)) - 1);
        return ((x >> k) & low) | ((x << (16 - k)) & ~low);
    }

    // §5.1.2: row r of the state (bits r, r+4, r+8, r+12 of a lane, byte
    // r + 4c being row r, column c) moves r columns left, which is the
    // lane rotated right by 4r
    inline void bs_shift_rows(Planes& s) noexcept {
        for (auto& p : s) {
            uint64_t r0 = p & 0x1111111111111111ull;
            uint64_t r1 = p & 0x2222222222222222ull;
            uint64_t r2 = p & 0x4444444444444444ull;
            uint64_t r3 = p & 0x8888888888888888ull;
            p = r0 | rotr_lanes16(r1, 4) | rotr_lanes16(r2, 8) | rotr_lanes16(r3, 12);
        }
    }

    inline void bs_inv_shift_rows(Planes& s) noexcept {
        for (auto& p : s) {
            uint64_t r0 = p & 0x1111111111111111ull;
            uint64_t r1 = p & 0x2222222222222222ull;
            uint64_t r2 = p & 0x4444444444444444ull;
            uint64_t r3 = p & 0x8888888888888888ull;
            p = r0 | rotr_lanes16(r1, 12) | rotr_lanes16(r2, 8) | rotr_lanes16(r3, 4);
        }
    }

    // Row r of each column takes row r+1 (r+2): the nibbles rotated
    SGCL_INLINE_HOT uint64_t rot_column1(uint64_t x) noexcept {
        return ((x >> 1) & 0x7777777777777777ull) | ((x << 3) & 0x8888888888888888ull);
    }

    SGCL_INLINE_HOT uint64_t rot_column2(uint64_t x) noexcept {
        return ((x >> 2) & 0x3333333333333333ull) | ((x << 2) & 0xCCCCCCCCCCCCCCCCull);
    }

    // x·a in GF(2^8) over the planes (x^8 = x^4 + x^3 + x + 1)
    inline void bs_xtime(Planes& a) noexcept {
        uint64_t t = a[7];
        a[7] = a[6];
        a[6] = a[5];
        a[5] = a[4];
        a[4] = a[3] ^ t;
        a[3] = a[2] ^ t;
        a[2] = a[1];
        a[1] = a[0] ^ t;
        a[0] = t;
    }

    // §5.1.3: s'r = 2·sr ^ 3·sr+1 ^ sr+2 ^ sr+3, which is
    // 2·(sr ^ sr+1) ^ sr+1 ^ (sr+2 ^ sr+3)
    inline void bs_mix_columns(Planes& s) noexcept {
        Planes t, r1;
        for (int i = 0; i < 8; ++i) {
            r1[i] = rot_column1(s[i]);
            t[i] = s[i] ^ r1[i];
        }
        Planes x = t;
        bs_xtime(x);
        for (int i = 0; i < 8; ++i) {
            s[i] = x[i] ^ r1[i] ^ rot_column2(t[i]);
        }
    }

    // §5.3.3 as MixColumns after a step: with u = 4·(s0 ^ s2) and
    // v = 4·(s1 ^ s3), InvMixColumns(s) = MixColumns(s0^u, s1^v, s2^u, s3^v)
    inline void bs_inv_mix_columns(Planes& s) noexcept {
        Planes u;
        for (int i = 0; i < 8; ++i) {
            u[i] = s[i] ^ rot_column2(s[i]);
        }
        bs_xtime(u);
        bs_xtime(u);
        for (int i = 0; i < 8; ++i) {
            s[i] ^= u[i];
        }
        bs_mix_columns(s);
    }

    inline void bs_add_key(Planes& s, const Planes& k) noexcept {
        for (int i = 0; i < 8; ++i) {
            s[i] ^= k[i];
        }
    }

    // The round keys bitsliced, each repeated in the four lanes
    struct AesPortableKey {
        std::array<Planes, 15> rk;
        unsigned rounds;
    };

    inline void aes_portable_load_round_keys(AesPortableKey& k, const unsigned char* round_keys, unsigned rounds) noexcept {
        k.rounds = rounds;
        unsigned char four[64];
        for (unsigned r = 0; r <= rounds; ++r) {
            for (int b = 0; b < 4; ++b) {
                std::memcpy(four + 16 * b, round_keys + 16 * r, 16);
            }
            bitslice(four, k.rk[r]);
        }
        secure_zero(four, sizeof four);
    }

    // FIPS 197 §5.1, four blocks
    inline void aes_portable_encrypt4(const AesPortableKey& k, const unsigned char* in, unsigned char* out) noexcept {
        Planes s;
        bitslice(in, s);
        bs_add_key(s, k.rk[0]);
        for (unsigned r = 1; r < k.rounds; ++r) {
            bs_sub_bytes(s);
            bs_shift_rows(s);
            bs_mix_columns(s);
            bs_add_key(s, k.rk[r]);
        }
        bs_sub_bytes(s);
        bs_shift_rows(s);
        bs_add_key(s, k.rk[k.rounds]);
        unbitslice(s, out);
        secure_zero_object(s);
    }

    // §5.3, the inverse cipher, four blocks
    inline void aes_portable_decrypt4(const AesPortableKey& k, const unsigned char* in, unsigned char* out) noexcept {
        Planes s;
        bitslice(in, s);
        bs_add_key(s, k.rk[k.rounds]);
        for (unsigned r = k.rounds - 1; r >= 1; --r) {
            bs_inv_shift_rows(s);
            bs_inv_sub_bytes(s);
            bs_add_key(s, k.rk[r]);
            bs_inv_mix_columns(s);
        }
        bs_inv_shift_rows(s);
        bs_inv_sub_bytes(s);
        bs_add_key(s, k.rk[0]);
        unbitslice(s, out);
        secure_zero_object(s);
    }
}
