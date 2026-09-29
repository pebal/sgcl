//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../constant_time.h"
#include "words.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#if SGCL_CRYPTO_NEON
#include <arm_neon.h>
#endif

// ChaCha20 (RFC 8439 §2.3): a state of sixteen 32-bit words — four
// constants, the key's eight, a block counter and the nonce's three — put
// through twenty rounds of additions, XORs and rotations (ten column rounds
// and ten diagonal rounds, in pairs), and added to itself; the sixteen words,
// little-endian, are 64 bytes of keystream. Nothing depends on the key but
// values: the cipher is constant-time by construction on any machine.
//
// On x86-64 the same shape: four blocks in SSE2 registers, eight in AVX2
// ones where the processor has them (cpu::wide()).
//
// On arm64 four blocks go at once in NEON registers, "vertically": register
// i holds word i of the four blocks, the counters in register 12 differing
// by 0..3, so that every step of a round is one instruction for the four
// blocks and a round's four quarter-rounds are four independent chains.
// The rotations by 16 and 8 are byte permutations (REV32, TBL), by 12 and 7
// a shift and a shift-insert. The four blocks are transposed back to
// consecutive keystream at the end. Two such groups run side by side (each
// in an array of its own, so that both stay in registers) and a ninth
// block in general registers in the same loop, which the integer units
// compute while the vector units are full: nine blocks a step. Elsewhere
// a block at a time in C++.
namespace sgcl::crypto::detail {
    // "expand 32-byte k"
    inline constexpr uint32_t chacha_constants[4] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};

    struct ChachaState {
        uint32_t key[8];
        uint32_t nonce[3];
    };

    inline void chacha_quarter(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) noexcept {
        a += b; d ^= a; d = rotl32(d, 16);
        c += d; b ^= c; b = rotl32(b, 12);
        a += b; d ^= a; d = rotl32(d, 8);
        c += d; b ^= c; b = rotl32(b, 7);
    }

    inline void chacha_rounds(uint32_t (&x)[16]) noexcept {
        for (int i = 0; i < 10; ++i) {
            chacha_quarter(x[0], x[4], x[8], x[12]);
            chacha_quarter(x[1], x[5], x[9], x[13]);
            chacha_quarter(x[2], x[6], x[10], x[14]);
            chacha_quarter(x[3], x[7], x[11], x[15]);
            chacha_quarter(x[0], x[5], x[10], x[15]);
            chacha_quarter(x[1], x[6], x[11], x[12]);
            chacha_quarter(x[2], x[7], x[8], x[13]);
            chacha_quarter(x[3], x[4], x[9], x[14]);
        }
    }

    inline void chacha_initial(const ChachaState& s, uint32_t counter, uint32_t (&x)[16]) noexcept {
        for (int i = 0; i < 4; ++i) {
            x[i] = chacha_constants[i];
        }
        for (int i = 0; i < 8; ++i) {
            x[4 + i] = s.key[i];
        }
        x[12] = counter;
        x[13] = s.nonce[0];
        x[14] = s.nonce[1];
        x[15] = s.nonce[2];
    }

    // §2.3: one block of keystream
    inline void chacha_block(const ChachaState& s, uint32_t counter, unsigned char* out) noexcept {
        uint32_t x[16], in[16];
        chacha_initial(s, counter, in);
        std::memcpy(x, in, sizeof x);
        chacha_rounds(x);
        for (int i = 0; i < 16; ++i) {
            store_le32(out + 4 * i, x[i] + in[i]);
        }
        secure_zero(x, sizeof x);
        secure_zero(in, sizeof in);
    }

    // HChaCha20 (draft-irtf-cfrg-xchacha §2.2): the state with the first 16
    // bytes of the nonce in words 12..15, the twenty rounds, and words 0..3
    // and 12..15 — without the final addition — as a new 256-bit key
    inline void hchacha20(const unsigned char* key, const unsigned char* nonce16, uint32_t (&subkey)[8]) noexcept {
        uint32_t x[16];
        for (int i = 0; i < 4; ++i) {
            x[i] = chacha_constants[i];
        }
        for (int i = 0; i < 8; ++i) {
            x[4 + i] = load_le32(key + 4 * i);
        }
        for (int i = 0; i < 4; ++i) {
            x[12 + i] = load_le32(nonce16 + 4 * i);
        }
        chacha_rounds(x);
        for (int i = 0; i < 4; ++i) {
            subkey[i] = x[i];
            subkey[4 + i] = x[12 + i];
        }
        secure_zero(x, sizeof x);
    }

#if SGCL_CRYPTO_NEON
    inline uint32x4_t rotl16_v(uint32x4_t v) noexcept {
        return vreinterpretq_u32_u16(vrev32q_u16(vreinterpretq_u16_u32(v)));
    }

    inline uint32x4_t rotl8_v(uint32x4_t v) noexcept {
        static const uint8_t index[16] = {3, 0, 1, 2, 7, 4, 5, 6, 11, 8, 9, 10, 15, 12, 13, 14};
        return vreinterpretq_u32_u8(vqtbl1q_u8(vreinterpretq_u8_u32(v), vld1q_u8(index)));
    }

    template<int N>
    inline uint32x4_t rotl_v(uint32x4_t v) noexcept {
        return vsriq_n_u32(vshlq_n_u32(v, N), v, 32 - N);
    }

    inline void chacha_quarter_v(uint32x4_t& a, uint32x4_t& b, uint32x4_t& c, uint32x4_t& d) noexcept {
        a = vaddq_u32(a, b); d = veorq_u32(d, a); d = rotl16_v(d);
        c = vaddq_u32(c, d); b = veorq_u32(b, c); b = rotl_v<12>(b);
        a = vaddq_u32(a, b); d = veorq_u32(d, a); d = rotl8_v(d);
        c = vaddq_u32(c, d); b = veorq_u32(b, c); b = rotl_v<7>(b);
    }

    inline void chacha_double_round_v(uint32x4_t (&x)[16]) noexcept {
        chacha_quarter_v(x[0], x[4], x[8], x[12]);
        chacha_quarter_v(x[1], x[5], x[9], x[13]);
        chacha_quarter_v(x[2], x[6], x[10], x[14]);
        chacha_quarter_v(x[3], x[7], x[11], x[15]);
        chacha_quarter_v(x[0], x[5], x[10], x[15]);
        chacha_quarter_v(x[1], x[6], x[11], x[12]);
        chacha_quarter_v(x[2], x[7], x[8], x[13]);
        chacha_quarter_v(x[3], x[4], x[9], x[14]);
    }

    // The four blocks of x after the rounds: the initial state added, words
    // 4q..4q+3 of the four transposed (block j's 16 bytes at 64j + 16q)
    // and XORed into 256 bytes of in
    inline void chacha_store4(uint32x4_t (&x)[16], const uint32_t (&init)[16], uint32x4_t counters, const unsigned char* in, unsigned char* out) noexcept {
        for (int i = 0; i < 16; ++i) {
            x[i] = vaddq_u32(x[i], i == 12 ? counters : vdupq_n_u32(init[i]));
        }
        for (int q = 0; q < 4; ++q) {
            uint32x4_t a = x[4 * q], b = x[4 * q + 1], c = x[4 * q + 2], d = x[4 * q + 3];
            uint32x4_t t0 = vtrn1q_u32(a, b), t1 = vtrn2q_u32(a, b), t2 = vtrn1q_u32(c, d), t3 = vtrn2q_u32(c, d);
            uint32x4_t r[4] = {
                vreinterpretq_u32_u64(vtrn1q_u64(vreinterpretq_u64_u32(t0), vreinterpretq_u64_u32(t2))),
                vreinterpretq_u32_u64(vtrn1q_u64(vreinterpretq_u64_u32(t1), vreinterpretq_u64_u32(t3))),
                vreinterpretq_u32_u64(vtrn2q_u64(vreinterpretq_u64_u32(t0), vreinterpretq_u64_u32(t2))),
                vreinterpretq_u32_u64(vtrn2q_u64(vreinterpretq_u64_u32(t1), vreinterpretq_u64_u32(t3))),
            };
            for (int j = 0; j < 4; ++j) {
                const size_t at = 64 * size_t(j) + 16 * size_t(q);
                vst1q_u8(out + at, veorq_u8(vld1q_u8(in + at), vreinterpretq_u8_u32(r[j])));
            }
        }
    }

    // 4·G consecutive blocks from counter (G = 1 or 2), XORed into in
    // (256·G bytes): the two groups' rounds side by side, each in its own
    // array so that both stay in registers
    template<int G>
    inline void chacha_xor_groups(const ChachaState& s, uint32_t counter, const unsigned char* in, unsigned char* out) noexcept {
        uint32_t init[16];
        chacha_initial(s, counter, init);
        const uint32_t lanes[4] = {0, 1, 2, 3};
        const uint32x4_t c0 = vaddq_u32(vdupq_n_u32(counter), vld1q_u32(lanes));
        const uint32x4_t c1 = vaddq_u32(c0, vdupq_n_u32(4));
        uint32x4_t x[16], y[16];
        for (int i = 0; i < 16; ++i) {
            x[i] = vdupq_n_u32(init[i]);
            if constexpr (G == 2) {
                y[i] = x[i];
            }
        }
        x[12] = c0;
        if constexpr (G == 2) {
            y[12] = c1;
        }
        for (int i = 0; i < 10; ++i) {
            chacha_double_round_v(x);
            if constexpr (G == 2) {
                chacha_double_round_v(y);
            }
        }
        chacha_store4(x, init, c0, in, out);
        if constexpr (G == 2) {
            chacha_store4(y, init, c1, in + 256, out + 256);
        }
        secure_zero(init, sizeof init);
    }

    // Nine blocks: eight in NEON registers and a ninth in general
    // registers, its rounds in the same loop, so that the integer units work
    // while the vector units are full (576 bytes)
    inline void chacha_xor_nine(const ChachaState& s, uint32_t counter, const unsigned char* in, unsigned char* out) noexcept {
        uint32_t init[16];
        chacha_initial(s, counter, init);
        const uint32_t lanes[4] = {0, 1, 2, 3};
        const uint32x4_t c0 = vaddq_u32(vdupq_n_u32(counter), vld1q_u32(lanes));
        const uint32x4_t c1 = vaddq_u32(c0, vdupq_n_u32(4));
        uint32x4_t x[16], y[16];
        uint32_t z[16];
        for (int i = 0; i < 16; ++i) {
            x[i] = vdupq_n_u32(init[i]);
            y[i] = x[i];
            z[i] = init[i];
        }
        x[12] = c0;
        y[12] = c1;
        z[12] = counter + 8;
        for (int i = 0; i < 10; ++i) {
            chacha_double_round_v(x);
            chacha_double_round_v(y);
            chacha_quarter(z[0], z[4], z[8], z[12]);
            chacha_quarter(z[1], z[5], z[9], z[13]);
            chacha_quarter(z[2], z[6], z[10], z[14]);
            chacha_quarter(z[3], z[7], z[11], z[15]);
            chacha_quarter(z[0], z[5], z[10], z[15]);
            chacha_quarter(z[1], z[6], z[11], z[12]);
            chacha_quarter(z[2], z[7], z[8], z[13]);
            chacha_quarter(z[3], z[4], z[9], z[14]);
        }
        chacha_store4(x, init, c0, in, out);
        chacha_store4(y, init, c1, in + 256, out + 256);
        init[12] = counter + 8;
        for (int i = 0; i < 16; ++i) {
            store_le32(out + 512 + 4 * i, load_le32(in + 512 + 4 * i) ^ (z[i] + init[i]));
        }
        secure_zero(init, sizeof init);
        secure_zero(z, sizeof z);
    }
#endif

#if defined(SGCL_CRYPTO_X86)
    // x86-64: four blocks in SSE2 registers, vertically as on NEON (SSE2 is
    // in the minimum: no gate), and eight in AVX2 registers where cpu::wide()
    // says so, word i of blocks 0..3 in the low lane and of 4..7 in the
    // high one. SSE2 rotates by shifts, AVX2 by 16 and 8 with VPSHUFB.
    template<int N>
    inline __m128i rotl_x(__m128i v) noexcept {
        return _mm_or_si128(_mm_slli_epi32(v, N), _mm_srli_epi32(v, 32 - N));
    }

    inline void chacha_quarter_x(__m128i& a, __m128i& b, __m128i& c, __m128i& d) noexcept {
        a = _mm_add_epi32(a, b); d = _mm_xor_si128(d, a); d = rotl_x<16>(d);
        c = _mm_add_epi32(c, d); b = _mm_xor_si128(b, c); b = rotl_x<12>(b);
        a = _mm_add_epi32(a, b); d = _mm_xor_si128(d, a); d = rotl_x<8>(d);
        c = _mm_add_epi32(c, d); b = _mm_xor_si128(b, c); b = rotl_x<7>(b);
    }

    // Four blocks from counter XORed into in (256 bytes)
    inline void chacha_xor_four_x86(const ChachaState& s, uint32_t counter, const unsigned char* in, unsigned char* out) noexcept {
        uint32_t init[16];
        chacha_initial(s, counter, init);
        const __m128i counters = _mm_add_epi32(_mm_set1_epi32(int(counter)), _mm_setr_epi32(0, 1, 2, 3));
        __m128i x[16];
        for (int i = 0; i < 16; ++i) {
            x[i] = _mm_set1_epi32(int(init[i]));
        }
        x[12] = counters;
        for (int i = 0; i < 10; ++i) {
            chacha_quarter_x(x[0], x[4], x[8], x[12]);
            chacha_quarter_x(x[1], x[5], x[9], x[13]);
            chacha_quarter_x(x[2], x[6], x[10], x[14]);
            chacha_quarter_x(x[3], x[7], x[11], x[15]);
            chacha_quarter_x(x[0], x[5], x[10], x[15]);
            chacha_quarter_x(x[1], x[6], x[11], x[12]);
            chacha_quarter_x(x[2], x[7], x[8], x[13]);
            chacha_quarter_x(x[3], x[4], x[9], x[14]);
        }
        for (int i = 0; i < 16; ++i) {
            x[i] = _mm_add_epi32(x[i], i == 12 ? counters : _mm_set1_epi32(int(init[i])));
        }
        // words 4q..4q+3 of the four blocks transposed: block j's 16 bytes at 64j + 16q
        for (int q = 0; q < 4; ++q) {
            const __m128i t0 = _mm_unpacklo_epi32(x[4 * q], x[4 * q + 1]), t1 = _mm_unpackhi_epi32(x[4 * q], x[4 * q + 1]);
            const __m128i t2 = _mm_unpacklo_epi32(x[4 * q + 2], x[4 * q + 3]), t3 = _mm_unpackhi_epi32(x[4 * q + 2], x[4 * q + 3]);
            const __m128i r[4] = {_mm_unpacklo_epi64(t0, t2), _mm_unpackhi_epi64(t0, t2), _mm_unpacklo_epi64(t1, t3), _mm_unpackhi_epi64(t1, t3)};
            for (int j = 0; j < 4; ++j) {
                const size_t at = 64 * size_t(j) + 16 * size_t(q);
                const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + at));
                _mm_storeu_si128(reinterpret_cast<__m128i*>(out + at), _mm_xor_si128(v, r[j]));
            }
        }
        for (auto& v : x) {
            v = _mm_setzero_si128();
        }
        secure_zero(init, sizeof init);
    }

    template<int N>
    SGCL_INLINE_X86_WIDE
    inline __m256i rotl_y(__m256i v) noexcept {
        if constexpr (N == 16) {
            return _mm256_shuffle_epi8(v, _mm256_setr_epi8(2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13,
                                                            2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13));
        } else if constexpr (N == 8) {
            return _mm256_shuffle_epi8(v, _mm256_setr_epi8(3, 0, 1, 2, 7, 4, 5, 6, 11, 8, 9, 10, 15, 12, 13, 14,
                                                            3, 0, 1, 2, 7, 4, 5, 6, 11, 8, 9, 10, 15, 12, 13, 14));
        } else {
            return _mm256_or_si256(_mm256_slli_epi32(v, N), _mm256_srli_epi32(v, 32 - N));
        }
    }

    SGCL_INLINE_X86_WIDE
    inline void chacha_quarter_y(__m256i& a, __m256i& b, __m256i& c, __m256i& d) noexcept {
        a = _mm256_add_epi32(a, b); d = _mm256_xor_si256(d, a); d = rotl_y<16>(d);
        c = _mm256_add_epi32(c, d); b = _mm256_xor_si256(b, c); b = rotl_y<12>(b);
        a = _mm256_add_epi32(a, b); d = _mm256_xor_si256(d, a); d = rotl_y<8>(d);
        c = _mm256_add_epi32(c, d); b = _mm256_xor_si256(b, c); b = rotl_y<7>(b);
    }

    // Eight blocks from counter XORed into in (512 bytes)
    SGCL_TARGET_X86_WIDE
    inline void chacha_xor_eight_x86(const ChachaState& s, uint32_t counter, const unsigned char* in, unsigned char* out) noexcept {
        uint32_t init[16];
        chacha_initial(s, counter, init);
        const __m256i counters = _mm256_add_epi32(_mm256_set1_epi32(int(counter)), _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7));
        __m256i x[16];
        for (int i = 0; i < 16; ++i) {
            x[i] = _mm256_set1_epi32(int(init[i]));
        }
        x[12] = counters;
        for (int i = 0; i < 10; ++i) {
            chacha_quarter_y(x[0], x[4], x[8], x[12]);
            chacha_quarter_y(x[1], x[5], x[9], x[13]);
            chacha_quarter_y(x[2], x[6], x[10], x[14]);
            chacha_quarter_y(x[3], x[7], x[11], x[15]);
            chacha_quarter_y(x[0], x[5], x[10], x[15]);
            chacha_quarter_y(x[1], x[6], x[11], x[12]);
            chacha_quarter_y(x[2], x[7], x[8], x[13]);
            chacha_quarter_y(x[3], x[4], x[9], x[14]);
        }
        for (int i = 0; i < 16; ++i) {
            x[i] = _mm256_add_epi32(x[i], i == 12 ? counters : _mm256_set1_epi32(int(init[i])));
        }
        // within each 128-bit lane as in the four-block path: the low lane
        // holds blocks 0..3, the high lane blocks 4..7
        for (int q = 0; q < 4; ++q) {
            const __m256i t0 = _mm256_unpacklo_epi32(x[4 * q], x[4 * q + 1]), t1 = _mm256_unpackhi_epi32(x[4 * q], x[4 * q + 1]);
            const __m256i t2 = _mm256_unpacklo_epi32(x[4 * q + 2], x[4 * q + 3]), t3 = _mm256_unpackhi_epi32(x[4 * q + 2], x[4 * q + 3]);
            const __m256i r[4] = {_mm256_unpacklo_epi64(t0, t2), _mm256_unpackhi_epi64(t0, t2), _mm256_unpacklo_epi64(t1, t3), _mm256_unpackhi_epi64(t1, t3)};
            for (int j = 0; j < 4; ++j) {
                for (int half = 0; half < 2; ++half) {
                    const size_t at = 64 * size_t(j + 4 * half) + 16 * size_t(q);
                    const __m128i k = half ? _mm256_extracti128_si256(r[j], 1) : _mm256_castsi256_si128(r[j]);
                    const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + at));
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(out + at), _mm_xor_si128(v, k));
                }
            }
        }
        for (auto& v : x) {
            v = _mm256_setzero_si256();
        }
        secure_zero(init, sizeof init);
    }
#endif

    // n bytes (any number) of keystream from the start of block `counter`
    // XORed into in, to out (which may be in). The counter wraps modulo
    // 2^32 inside; the caller keeps it from doing so. With NEON: nine
    // blocks a step, then eight; a tail of more than two blocks through the
    // vector groups on a copy (a group of four costs less than three blocks
    // one at a time), one or two blocks in general registers.
    inline void chacha_xor(const ChachaState& s, uint32_t counter, const unsigned char* in, unsigned char* out, size_t n) noexcept {
#if SGCL_CRYPTO_NEON
        while (n >= 576) {
            chacha_xor_nine(s, counter, in, out);
            counter += 9;
            in += 576;
            out += 576;
            n -= 576;
        }
        if (n >= 512) {
            chacha_xor_groups<2>(s, counter, in, out);
            counter += 8;
            in += 512;
            out += 512;
            n -= 512;
        }
        if (n > 128) {
            // The bytes past n are XORed too and thrown away: zeros, so
            // that nothing reads memory never written
            unsigned char buffer[512] = {};
            std::memcpy(buffer, in, n);
            if (n > 256) {
                chacha_xor_groups<2>(s, counter, buffer, buffer);
            } else {
                chacha_xor_groups<1>(s, counter, buffer, buffer);
            }
            std::memcpy(out, buffer, n);
            secure_zero(buffer, sizeof buffer);
            return;
        }
#elif defined(SGCL_CRYPTO_X86)
        if (n >= 512 && sgcl::detail::cpu::wide()) {
            do {
                chacha_xor_eight_x86(s, counter, in, out);
                counter += 8;
                in += 512;
                out += 512;
                n -= 512;
            } while (n >= 512);
        }
        while (n >= 256) {
            chacha_xor_four_x86(s, counter, in, out);
            counter += 4;
            in += 256;
            out += 256;
            n -= 256;
        }
        if (n > 128) {
            // as on NEON: the bytes past n XORed too, zeros, thrown away
            unsigned char buffer[256] = {};
            std::memcpy(buffer, in, n);
            chacha_xor_four_x86(s, counter, buffer, buffer);
            std::memcpy(out, buffer, n);
            secure_zero(buffer, sizeof buffer);
            return;
        }
#endif
        unsigned char block[64];
        while (n > 0) {
            chacha_block(s, counter++, block);
            const size_t m = n < 64 ? n : 64;
            for (size_t i = 0; i < m; ++i) {
                out[i] = (unsigned char)(in[i] ^ block[i]);
            }
            in += m;
            out += m;
            n -= m;
        }
        secure_zero(block, sizeof block);
    }

    // n whole blocks
    inline void chacha_xor_blocks(const ChachaState& s, uint32_t counter, const unsigned char* in, unsigned char* out, size_t n) noexcept {
        chacha_xor(s, counter, in, out, n * 64);
    }

    // The keystream from block 0 into ks (at most 576 bytes), as many blocks
    // as serve a message of n bytes at once: block 0 is the one-time key of
    // the AEAD (RFC 8439 §2.6), the blocks after it the first of the text,
    // so that the key costs no block of its own. The bytes of keystream
    // made; 64 of them are block 0's.
    inline size_t chacha_first_blocks(const ChachaState& s, size_t n, unsigned char* ks) noexcept {
#if SGCL_CRYPTO_NEON
        if (n > 64) {
            const size_t made = n > 448 ? 576 : n > 192 ? 512 : 256;
            std::memset(ks, 0, made);
            if (made == 576) {
                chacha_xor_nine(s, 0, ks, ks);
            } else if (made == 512) {
                chacha_xor_groups<2>(s, 0, ks, ks);
            } else {
                chacha_xor_groups<1>(s, 0, ks, ks);
            }
            return made;
        }
#elif defined(SGCL_CRYPTO_X86)
        if (n > 64) {
            std::memset(ks, 0, 256);
            chacha_xor_four_x86(s, 0, ks, ks);
            return 256;
        }
#endif
        chacha_block(s, 0, ks);
        if (n > 0) {
            chacha_block(s, 1, ks + 64);
            return 128;
        }
        return 64;
    }

    inline void chacha_load(ChachaState& s, const unsigned char* key, const unsigned char* nonce12) noexcept {
        for (int i = 0; i < 8; ++i) {
            s.key[i] = load_le32(key + 4 * i);
        }
        for (int i = 0; i < 3; ++i) {
            s.nonce[i] = load_le32(nonce12 + 4 * i);
        }
    }
}
