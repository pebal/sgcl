//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "paths.h"
#include "sha256.h"
#include "../secure_zero.h"
#include "../../core/detail/bytes.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

// BLAKE3 (the specification of Aumasson, Neves, O'Connor and Wilcox-O'Hearn,
// 2021): a binary tree over chunks of 1024 bytes. A chunk is sixteen blocks
// of 64 bytes chained by the compression function, which is BLAKE2s's with 7
// rounds, the message words permuted between rounds rather than chosen by a
// table, and four words of input — a 64-bit counter, the block's length and
// the domain flags — where BLAKE2s has its counter and final flag. A
// parent node compresses its two children's chaining values as one block.
// The root is compressed with the ROOT flag once per 64 bytes of output,
// the counter numbering the output blocks: an output of any length, read
// from any position.
//
// Every operation is an addition, a XOR or a rotation by a constant: no
// branch or address depends on the key or the input's bytes (the input's
// length and the position of the output are public).
//
// Paths. The portable one compresses a block at a time. Whole chunks are
// independent, and so are the parents of one level of a subtree: on arm64
// four of them go through NEON at once, a lane a chunk (or a parent), the
// sixteen message words of the four blocks transposed so that each G of a
// round is one run of vector operations over four independent compressions
// (no diagonalization: the lanes never mix). In arm64's minimum, no gate.
// On x86-64 the same four lanes on SSE2, x86-64's minimum. A single block
// (the last chunk, the root, an output block) takes the portable path.
namespace sgcl::crypto::detail {
    inline constexpr uint32_t blake3_chunk_start = 1;
    inline constexpr uint32_t blake3_chunk_end = 2;
    inline constexpr uint32_t blake3_parent = 4;
    inline constexpr uint32_t blake3_root = 8;
    inline constexpr uint32_t blake3_keyed_hash = 16;
    inline constexpr uint32_t blake3_derive_key_context = 32;
    inline constexpr uint32_t blake3_derive_key_material = 64;
    inline constexpr size_t blake3_chunk = 1024;
    inline constexpr size_t blake3_block = 64;

    // The message schedule: round r takes the words permuted r times by
    // the specification's permutation (2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12,
    // 5, 9, 14, 15, 8), so word i of round r is m[blake3_schedule[r][i]]
    struct Blake3Schedule {
        uint8_t s[7][16];
    };

    inline constexpr Blake3Schedule blake3_schedule = [] {
        constexpr uint8_t perm[16] = {2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8};
        Blake3Schedule x{};
        for (int i = 0; i < 16; ++i) {
            x.s[0][i] = uint8_t(i);
        }
        for (int r = 1; r < 7; ++r) {
            for (int i = 0; i < 16; ++i) {
                x.s[r][i] = x.s[r - 1][perm[i]];
            }
        }
        return x;
    }();

    // The compression function: the sixteen words of the state after the
    // seven rounds, before the feed-forward (out[i] = v[i] ^ v[i + 8] for
    // the chaining value, and v[i + 8] ^ cv[i] for the second half of an
    // output block)
    inline void blake3_rounds(uint32_t* v, const uint32_t* m) noexcept {
        auto g = [&](int a, int b, int c, int d, uint32_t x, uint32_t y) SGCL_CRYPTO_LAMBDA_INLINE {
            v[a] = v[a] + v[b] + x;
            v[d] = std::rotr(v[d] ^ v[a], 16);
            v[c] = v[c] + v[d];
            v[b] = std::rotr(v[b] ^ v[c], 12);
            v[a] = v[a] + v[b] + y;
            v[d] = std::rotr(v[d] ^ v[a], 8);
            v[c] = v[c] + v[d];
            v[b] = std::rotr(v[b] ^ v[c], 7);
        };
        [&]<size_t... R>(std::index_sequence<R...>) SGCL_CRYPTO_LAMBDA_INLINE {
            auto round = [&]<size_t Q>(std::integral_constant<size_t, Q>) SGCL_CRYPTO_LAMBDA_INLINE {
                constexpr const uint8_t* s = blake3_schedule.s[Q];
                g(0, 4, 8, 12, m[s[0]], m[s[1]]);
                g(1, 5, 9, 13, m[s[2]], m[s[3]]);
                g(2, 6, 10, 14, m[s[4]], m[s[5]]);
                g(3, 7, 11, 15, m[s[6]], m[s[7]]);
                g(0, 5, 10, 15, m[s[8]], m[s[9]]);
                g(1, 6, 11, 12, m[s[10]], m[s[11]]);
                g(2, 7, 8, 13, m[s[12]], m[s[13]]);
                g(3, 4, 9, 14, m[s[14]], m[s[15]]);
            };
            (round(std::integral_constant<size_t, R>()), ...);
        }(std::make_index_sequence<7>());
    }

    SGCL_INLINE_HOT void blake3_load_block(uint32_t* m, const unsigned char* block) noexcept {
        for (int i = 0; i < 16; ++i) {
            m[i] = load_le32(block + 4 * i);
        }
    }

    SGCL_INLINE_HOT void blake3_init_state(uint32_t* v, const uint32_t* cv, uint64_t counter, uint32_t block_len,
                                           uint32_t flags) noexcept {
        for (int i = 0; i < 8; ++i) {
            v[i] = cv[i];
        }
        v[8] = sha256_iv[0];
        v[9] = sha256_iv[1];
        v[10] = sha256_iv[2];
        v[11] = sha256_iv[3];
        v[12] = uint32_t(counter);
        v[13] = uint32_t(counter >> 32);
        v[14] = block_len;
        v[15] = flags;
    }

    // One block into a chaining value, in place
    SGCL_INLINE_HOT void blake3_compress_in_place(uint32_t* cv, const uint32_t* m, uint64_t counter, uint32_t block_len,
                                                  uint32_t flags) noexcept {
        uint32_t v[16];
        blake3_init_state(v, cv, counter, block_len, flags);
        blake3_rounds(v, m);
        for (int i = 0; i < 8; ++i) {
            cv[i] = v[i] ^ v[i + 8];
        }
    }

    // One block of output: all sixteen words, little-endian, 64 bytes
    SGCL_INLINE_HOT void blake3_compress_xof(const uint32_t* cv, const uint32_t* m, uint64_t counter, uint32_t block_len,
                                             uint32_t flags, unsigned char* out) noexcept {
        uint32_t v[16];
        blake3_init_state(v, cv, counter, block_len, flags);
        blake3_rounds(v, m);
        for (int i = 0; i < 8; ++i) {
            store_le32(out + 4 * i, v[i] ^ v[i + 8]);
            store_le32(out + 32 + 4 * i, v[i + 8] ^ cv[i]);
        }
    }

    // The chaining values of `count` inputs of `blocks` whole blocks each
    // (chunks: 16 blocks, the counter the chunk's number, rising by one an
    // input; parents: one block, the counter 0 for all), the first block of
    // each with flags_start and the last with flags_end besides `flags`;
    // 32 bytes each into out. The portable form, one input after the other
    inline void blake3_hash_many_portable(const unsigned char* const* inputs, size_t count, size_t blocks, const uint32_t* key,
                                          uint64_t counter, bool increment, uint32_t flags, uint32_t flags_start,
                                          uint32_t flags_end, unsigned char* out) noexcept {
        for (size_t i = 0; i < count; ++i) {
            uint32_t cv[8];
            std::memcpy(cv, key, sizeof cv);
            for (size_t b = 0; b < blocks; ++b) {
                uint32_t m[16];
                blake3_load_block(m, inputs[i] + b * blake3_block);
                uint32_t f = flags | (b == 0 ? flags_start : 0) | (b + 1 == blocks ? flags_end : 0);
                blake3_compress_in_place(cv, m, counter, uint32_t(blake3_block), f);
            }
            for (int w = 0; w < 8; ++w) {
                store_le32(out + 32 * i + 4 * w, cv[w]);
            }
            counter += increment;
        }
    }

#if defined(SGCL_CRYPTO_NEON)
    // Four inputs at once, a lane each: the eight words of the chaining
    // values and the sixteen of each block's message in a register a word.
    // The 4x4 transposes of the message are two rounds of TRN.
    SGCL_INLINE_HOT void blake3_transpose4(uint32x4_t& a, uint32x4_t& b, uint32x4_t& c, uint32x4_t& d) noexcept {
        uint32x4_t t0 = vtrn1q_u32(a, b), t1 = vtrn2q_u32(a, b), t2 = vtrn1q_u32(c, d), t3 = vtrn2q_u32(c, d);
        a = vreinterpretq_u32_u64(vtrn1q_u64(vreinterpretq_u64_u32(t0), vreinterpretq_u64_u32(t2)));
        c = vreinterpretq_u32_u64(vtrn2q_u64(vreinterpretq_u64_u32(t0), vreinterpretq_u64_u32(t2)));
        b = vreinterpretq_u32_u64(vtrn1q_u64(vreinterpretq_u64_u32(t1), vreinterpretq_u64_u32(t3)));
        d = vreinterpretq_u32_u64(vtrn2q_u64(vreinterpretq_u64_u32(t1), vreinterpretq_u64_u32(t3)));
    }

    SGCL_INLINE_HOT void blake3_rounds4(uint32x4_t* v, const uint32x4_t* m) noexcept {
        static constexpr uint8_t rot8_index[16] = {1, 2, 3, 0, 5, 6, 7, 4, 9, 10, 11, 8, 13, 14, 15, 12};
        const uint8x16_t rot8 = vld1q_u8(rot8_index);
        auto g = [&](int a, int b, int c, int d, uint32x4_t x, uint32x4_t y) SGCL_CRYPTO_LAMBDA_INLINE {
            v[a] = vaddq_u32(vaddq_u32(v[a], v[b]), x);
            v[d] = vreinterpretq_u32_u16(vrev32q_u16(vreinterpretq_u16_u32(veorq_u32(v[d], v[a]))));
            v[c] = vaddq_u32(v[c], v[d]);
            uint32x4_t t = veorq_u32(v[b], v[c]);
            v[b] = vsriq_n_u32(vshlq_n_u32(t, 20), t, 12);
            v[a] = vaddq_u32(vaddq_u32(v[a], v[b]), y);
            v[d] = vreinterpretq_u32_u8(vqtbl1q_u8(vreinterpretq_u8_u32(veorq_u32(v[d], v[a])), rot8));
            v[c] = vaddq_u32(v[c], v[d]);
            t = veorq_u32(v[b], v[c]);
            v[b] = vsriq_n_u32(vshlq_n_u32(t, 25), t, 7);
        };
        [&]<size_t... R>(std::index_sequence<R...>) SGCL_CRYPTO_LAMBDA_INLINE {
            auto round = [&]<size_t Q>(std::integral_constant<size_t, Q>) SGCL_CRYPTO_LAMBDA_INLINE {
                constexpr const uint8_t* s = blake3_schedule.s[Q];
                g(0, 4, 8, 12, m[s[0]], m[s[1]]);
                g(1, 5, 9, 13, m[s[2]], m[s[3]]);
                g(2, 6, 10, 14, m[s[4]], m[s[5]]);
                g(3, 7, 11, 15, m[s[6]], m[s[7]]);
                g(0, 5, 10, 15, m[s[8]], m[s[9]]);
                g(1, 6, 11, 12, m[s[10]], m[s[11]]);
                g(2, 7, 8, 13, m[s[12]], m[s[13]]);
                g(3, 4, 9, 14, m[s[14]], m[s[15]]);
            };
            (round(std::integral_constant<size_t, R>()), ...);
        }(std::make_index_sequence<7>());
    }

    inline void blake3_hash4_neon(const unsigned char* const* in, size_t blocks, const uint32_t* key, uint64_t counter,
                                  bool increment, uint32_t flags, uint32_t flags_start, uint32_t flags_end,
                                  unsigned char* out) noexcept {
        uint32x4_t h[8];
        for (int i = 0; i < 8; ++i) {
            h[i] = vdupq_n_u32(key[i]);
        }
        const uint64_t inc = increment ? 1 : 0;
        const uint32_t lo[4] = {uint32_t(counter), uint32_t(counter + inc), uint32_t(counter + 2 * inc), uint32_t(counter + 3 * inc)};
        const uint32_t hi[4] = {uint32_t(counter >> 32), uint32_t((counter + inc) >> 32), uint32_t((counter + 2 * inc) >> 32),
                                uint32_t((counter + 3 * inc) >> 32)};
        const uint32x4_t counter_lo = vld1q_u32(lo), counter_hi = vld1q_u32(hi);
        for (size_t b = 0; b < blocks; ++b) {
            uint32x4_t m[16];
            const size_t at = b * blake3_block;
            for (int q = 0; q < 4; ++q) {
                m[4 * q] = vreinterpretq_u32_u8(vld1q_u8(in[0] + at + 16 * q));
                m[4 * q + 1] = vreinterpretq_u32_u8(vld1q_u8(in[1] + at + 16 * q));
                m[4 * q + 2] = vreinterpretq_u32_u8(vld1q_u8(in[2] + at + 16 * q));
                m[4 * q + 3] = vreinterpretq_u32_u8(vld1q_u8(in[3] + at + 16 * q));
                blake3_transpose4(m[4 * q], m[4 * q + 1], m[4 * q + 2], m[4 * q + 3]);
            }
            uint32_t f = flags | (b == 0 ? flags_start : 0) | (b + 1 == blocks ? flags_end : 0);
            uint32x4_t v[16] = {h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7],
                                vdupq_n_u32(sha256_iv[0]), vdupq_n_u32(sha256_iv[1]), vdupq_n_u32(sha256_iv[2]), vdupq_n_u32(sha256_iv[3]),
                                counter_lo, counter_hi, vdupq_n_u32(uint32_t(blake3_block)), vdupq_n_u32(f)};
            blake3_rounds4(v, m);
            for (int i = 0; i < 8; ++i) {
                h[i] = veorq_u32(v[i], v[i + 8]);
            }
        }
        // back to a chaining value a lane: words 0-3 and 4-7 of each, transposed
        blake3_transpose4(h[0], h[1], h[2], h[3]);
        blake3_transpose4(h[4], h[5], h[6], h[7]);
        for (int lane = 0; lane < 4; ++lane) {
            vst1q_u8(out + 32 * lane, vreinterpretq_u8_u32(h[lane]));
            vst1q_u8(out + 32 * lane + 16, vreinterpretq_u8_u32(h[lane + 4]));
        }
    }

    // Four blocks of output at once: one root node, the counters t..t+3
    inline void blake3_xof4_neon(const uint32_t* cv, const uint32_t* block, uint32_t block_len, uint32_t flags, uint64_t t,
                                 unsigned char* out) noexcept {
        uint32x4_t m[16];
        for (int i = 0; i < 16; ++i) {
            m[i] = vdupq_n_u32(block[i]);
        }
        const uint32_t lo[4] = {uint32_t(t), uint32_t(t + 1), uint32_t(t + 2), uint32_t(t + 3)};
        const uint32_t hi[4] = {uint32_t(t >> 32), uint32_t((t + 1) >> 32), uint32_t((t + 2) >> 32), uint32_t((t + 3) >> 32)};
        uint32x4_t v[16] = {vdupq_n_u32(cv[0]), vdupq_n_u32(cv[1]), vdupq_n_u32(cv[2]), vdupq_n_u32(cv[3]),
                            vdupq_n_u32(cv[4]), vdupq_n_u32(cv[5]), vdupq_n_u32(cv[6]), vdupq_n_u32(cv[7]),
                            vdupq_n_u32(sha256_iv[0]), vdupq_n_u32(sha256_iv[1]), vdupq_n_u32(sha256_iv[2]), vdupq_n_u32(sha256_iv[3]),
                            vld1q_u32(lo), vld1q_u32(hi), vdupq_n_u32(block_len), vdupq_n_u32(flags)};
        blake3_rounds4(v, m);
        uint32x4_t w[16];
        for (int i = 0; i < 8; ++i) {
            w[i] = veorq_u32(v[i], v[i + 8]);
            w[i + 8] = veorq_u32(v[i + 8], vdupq_n_u32(cv[i]));
        }
        for (int q = 0; q < 4; ++q) {
            blake3_transpose4(w[4 * q], w[4 * q + 1], w[4 * q + 2], w[4 * q + 3]);
        }
        // w[4q + lane] holds words 4q..4q+3 of output block `lane`
        for (int lane = 0; lane < 4; ++lane) {
            for (int q = 0; q < 4; ++q) {
                vst1q_u8(out + 64 * lane + 16 * q, vreinterpretq_u8_u32(w[4 * q + lane]));
            }
        }
    }
#endif

#if defined(SGCL_CRYPTO_X86)
    // The NEON path's four lanes on SSE2 (x86-64's minimum, no gate): the
    // rotations by shifts, the transposes by unpacks. Compiled and checked
    // under the x86-64 target; not measured on an x86 machine.
    SGCL_INLINE_HOT void blake3_transpose4_sse2(__m128i& a, __m128i& b, __m128i& c, __m128i& d) noexcept {
        __m128i t0 = _mm_unpacklo_epi32(a, b), t1 = _mm_unpackhi_epi32(a, b);
        __m128i t2 = _mm_unpacklo_epi32(c, d), t3 = _mm_unpackhi_epi32(c, d);
        a = _mm_unpacklo_epi64(t0, t2);
        b = _mm_unpackhi_epi64(t0, t2);
        c = _mm_unpacklo_epi64(t1, t3);
        d = _mm_unpackhi_epi64(t1, t3);
    }

    SGCL_INLINE_HOT void blake3_rounds4_sse2(__m128i* v, const __m128i* m) noexcept {
        auto rot = [](__m128i x, int r) SGCL_CRYPTO_LAMBDA_INLINE {
            return _mm_or_si128(_mm_srli_epi32(x, r), _mm_slli_epi32(x, 32 - r));
        };
        auto g = [&](int a, int b, int c, int d, __m128i x, __m128i y) SGCL_CRYPTO_LAMBDA_INLINE {
            v[a] = _mm_add_epi32(_mm_add_epi32(v[a], v[b]), x);
            v[d] = rot(_mm_xor_si128(v[d], v[a]), 16);
            v[c] = _mm_add_epi32(v[c], v[d]);
            v[b] = rot(_mm_xor_si128(v[b], v[c]), 12);
            v[a] = _mm_add_epi32(_mm_add_epi32(v[a], v[b]), y);
            v[d] = rot(_mm_xor_si128(v[d], v[a]), 8);
            v[c] = _mm_add_epi32(v[c], v[d]);
            v[b] = rot(_mm_xor_si128(v[b], v[c]), 7);
        };
        [&]<size_t... R>(std::index_sequence<R...>) SGCL_CRYPTO_LAMBDA_INLINE {
            auto round = [&]<size_t Q>(std::integral_constant<size_t, Q>) SGCL_CRYPTO_LAMBDA_INLINE {
                constexpr const uint8_t* s = blake3_schedule.s[Q];
                g(0, 4, 8, 12, m[s[0]], m[s[1]]);
                g(1, 5, 9, 13, m[s[2]], m[s[3]]);
                g(2, 6, 10, 14, m[s[4]], m[s[5]]);
                g(3, 7, 11, 15, m[s[6]], m[s[7]]);
                g(0, 5, 10, 15, m[s[8]], m[s[9]]);
                g(1, 6, 11, 12, m[s[10]], m[s[11]]);
                g(2, 7, 8, 13, m[s[12]], m[s[13]]);
                g(3, 4, 9, 14, m[s[14]], m[s[15]]);
            };
            (round(std::integral_constant<size_t, R>()), ...);
        }(std::make_index_sequence<7>());
    }

    inline void blake3_hash4_sse2(const unsigned char* const* in, size_t blocks, const uint32_t* key, uint64_t counter,
                                  bool increment, uint32_t flags, uint32_t flags_start, uint32_t flags_end,
                                  unsigned char* out) noexcept {
        __m128i h[8];
        for (int i = 0; i < 8; ++i) {
            h[i] = _mm_set1_epi32(int(key[i]));
        }
        const uint64_t inc = increment ? 1 : 0;
        const __m128i counter_lo = _mm_setr_epi32(int(uint32_t(counter)), int(uint32_t(counter + inc)), int(uint32_t(counter + 2 * inc)),
                                                  int(uint32_t(counter + 3 * inc)));
        const __m128i counter_hi = _mm_setr_epi32(int(uint32_t(counter >> 32)), int(uint32_t((counter + inc) >> 32)),
                                                  int(uint32_t((counter + 2 * inc) >> 32)), int(uint32_t((counter + 3 * inc) >> 32)));
        for (size_t b = 0; b < blocks; ++b) {
            __m128i m[16];
            const size_t at = b * blake3_block;
            for (int q = 0; q < 4; ++q) {
                for (int lane = 0; lane < 4; ++lane) {
                    m[4 * q + lane] = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in[lane] + at + 16 * q));
                }
                blake3_transpose4_sse2(m[4 * q], m[4 * q + 1], m[4 * q + 2], m[4 * q + 3]);
            }
            uint32_t f = flags | (b == 0 ? flags_start : 0) | (b + 1 == blocks ? flags_end : 0);
            __m128i v[16] = {h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7],
                             _mm_set1_epi32(int(sha256_iv[0])), _mm_set1_epi32(int(sha256_iv[1])),
                             _mm_set1_epi32(int(sha256_iv[2])), _mm_set1_epi32(int(sha256_iv[3])),
                             counter_lo, counter_hi, _mm_set1_epi32(int(blake3_block)), _mm_set1_epi32(int(f))};
            blake3_rounds4_sse2(v, m);
            for (int i = 0; i < 8; ++i) {
                h[i] = _mm_xor_si128(v[i], v[i + 8]);
            }
        }
        blake3_transpose4_sse2(h[0], h[1], h[2], h[3]);
        blake3_transpose4_sse2(h[4], h[5], h[6], h[7]);
        for (int lane = 0; lane < 4; ++lane) {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 32 * lane), h[lane]);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 32 * lane + 16), h[lane + 4]);
        }
    }
#endif

    // hash_many: four at a time where there is a vector path, and a
    // remainder of two or three as four lanes too (one or two lanes of
    // repeated input thrown away: one pass of four costs less than two
    // compressions one by one); a single input the portable way
    inline void blake3_hash_many(const unsigned char* const* inputs, size_t count, size_t blocks, const uint32_t* key,
                                 uint64_t counter, bool increment, uint32_t flags, uint32_t flags_start, uint32_t flags_end,
                                 unsigned char* out) noexcept {
#if defined(SGCL_CRYPTO_NEON) || defined(SGCL_CRYPTO_X86)
        auto four = [&](const unsigned char* const* in, unsigned char* to) {
#if defined(SGCL_CRYPTO_NEON)
            blake3_hash4_neon(in, blocks, key, counter, increment, flags, flags_start, flags_end, to);
#else
            blake3_hash4_sse2(in, blocks, key, counter, increment, flags, flags_start, flags_end, to);
#endif
        };
        while (count >= 4) {
            four(inputs, out);
            inputs += 4;
            count -= 4;
            out += 4 * 32;
            counter += increment ? 4 : 0;
        }
        if (count >= 2) {
            const unsigned char* in[4] = {inputs[0], inputs[1], inputs[count - 1], inputs[count - 1]};
            unsigned char cvs[4 * 32];
            four(in, cvs);
            std::memcpy(out, cvs, count * 32);
            secure_zero(cvs, sizeof cvs);
            return;
        }
#endif
        blake3_hash_many_portable(inputs, count, blocks, key, counter, increment, flags, flags_start, flags_end, out);
    }

    // The most chunks a subtree hashed at once from the caller's memory
    inline constexpr size_t blake3_max_subtree = 64;

    // The `chunks` whole chunks at p (a power of two, at most
    // blake3_max_subtree, the first numbered `counter`, a multiple of
    // `chunks`) as the two chaining values under their subtree's top node,
    // which is left uncompressed — it may be the root — or as one chaining
    // value for one chunk: every chunk, then each level of parents but the
    // top, by hash_many. 64 bytes into cv_out, 32 for one chunk
    inline void blake3_subtree(const unsigned char* p, size_t chunks, const uint32_t* key, uint64_t counter, uint32_t flags,
                               unsigned char* cv_out) noexcept {
        unsigned char cvs[blake3_max_subtree * 32];
        const unsigned char* inputs[blake3_max_subtree];
        for (size_t i = 0; i < chunks; ++i) {
            inputs[i] = p + i * blake3_chunk;
        }
        blake3_hash_many(inputs, chunks, blake3_chunk / blake3_block, key, counter, true, flags, blake3_chunk_start,
                         blake3_chunk_end, cvs);
        // each level: the pairs of chaining values are the parents' blocks, in place
        for (size_t n = chunks; n > 2; n /= 2) {
            for (size_t i = 0; i < n / 2; ++i) {
                inputs[i] = cvs + 64 * i;
            }
            blake3_hash_many(inputs, n / 2, 1, key, 0, false, flags | blake3_parent, 0, 0, cvs);
        }
        std::memcpy(cv_out, cvs, chunks == 1 ? 32 : 64);
        secure_zero(cvs, chunks * 32);
    }

    // The incremental hasher (the specification's §5.1.2): the chunk being
    // filled — its chaining value, the block not yet compressed, how many
    // were — and the stack of the chaining values of the complete subtrees
    // to its left. A block or a chunk is compressed only when more input
    // follows it, and two entries of the stack are merged into their parent
    // only when more input follows them (the merges are lazy): until then
    // the pair may be the root's children. Merged, the stack holds one
    // entry per set bit of the number of chunks before the one being
    // filled; one more entry waits at most (55 cover 2^54 chunks, 2^64
    // bytes).
    struct Blake3State {
        uint32_t key[8];
        uint32_t cv[8];
        uint64_t chunk_counter;
        unsigned char block[blake3_block];
        uint8_t block_len;
        uint8_t blocks_compressed;
        uint8_t stack_len;
        uint8_t wipe;   // a keyed or derive_key state: zeroed by the owner's destructor
        uint32_t flags;
        uint32_t stack[55][8];

        void init(const uint32_t* k, uint32_t mode, bool secret) noexcept {
            std::memcpy(key, k, sizeof key);
            flags = mode;
            wipe = secret;
            reset();
        }

        SGCL_INLINE_HOT void reset() noexcept {
            std::memcpy(cv, key, sizeof cv);
            chunk_counter = 0;
            block_len = 0;
            blocks_compressed = 0;
            stack_len = 0;
        }

        SGCL_INLINE_HOT size_t chunk_len() const noexcept {
            return size_t(blocks_compressed) * blake3_block + block_len;
        }

        SGCL_INLINE_HOT uint32_t start_flag() const noexcept {
            return blocks_compressed == 0 ? blake3_chunk_start : 0;
        }

        // bytes into the chunk being filled, up to its end
        void chunk_update(const unsigned char* p, size_t n) noexcept {
            while (n != 0) {
                if (block_len == blake3_block) {
                    uint32_t m[16];
                    blake3_load_block(m, block);
                    blake3_compress_in_place(cv, m, chunk_counter, uint32_t(blake3_block), flags | start_flag());
                    ++blocks_compressed;
                    block_len = 0;
                }
                // whole blocks straight from p while more follows them
                while (block_len == 0 && n > blake3_block) {
                    uint32_t m[16];
                    blake3_load_block(m, p);
                    blake3_compress_in_place(cv, m, chunk_counter, uint32_t(blake3_block), flags | start_flag());
                    ++blocks_compressed;
                    p += blake3_block;
                    n -= blake3_block;
                }
                size_t take = std::min(n, blake3_block - block_len);
                sgcl::detail::copy_bytes(block + block_len, p, take);
                block_len = uint8_t(block_len + take);
                p += take;
                n -= take;
            }
        }

        // the parent of the stack's top two entries, in their place
        void merge_top() noexcept {
            uint32_t m[16];
            std::memcpy(m, stack[stack_len - 2], 32);
            std::memcpy(m + 8, stack[stack_len - 1], 32);
            uint32_t parent[8];
            std::memcpy(parent, key, sizeof parent);
            blake3_compress_in_place(parent, m, 0, uint32_t(blake3_block), flags | blake3_parent);
            std::memcpy(stack[stack_len - 2], parent, 32);
            --stack_len;
        }

        // more input follows `total` chunks: the pairs waiting merged, to
        // one entry per set bit of total
        SGCL_INLINE_HOT void merge(uint64_t total) noexcept {
            while (stack_len > unsigned(std::popcount(total))) {
                merge_top();
            }
        }

        SGCL_INLINE_HOT void push(const unsigned char* cv_bytes) noexcept {
            for (int i = 0; i < 8; ++i) {
                stack[stack_len][i] = load_le32(cv_bytes + 4 * i);
            }
            ++stack_len;
        }

        // the chunk being filled is complete and more input follows: its
        // chaining value onto the stack, and a new chunk
        void finish_chunk() noexcept {
            uint32_t m[16];
            blake3_load_block(m, block);
            blake3_compress_in_place(cv, m, chunk_counter, block_len, flags | start_flag() | blake3_chunk_end);
            unsigned char bytes_cv[32];
            for (int i = 0; i < 8; ++i) {
                store_le32(bytes_cv + 4 * i, cv[i]);
            }
            merge(chunk_counter);
            push(bytes_cv);
            ++chunk_counter;
            std::memcpy(cv, key, sizeof cv);
            block_len = 0;
            blocks_compressed = 0;
        }

        void update(const unsigned char* p, size_t n) noexcept {
            if (n == 0) {
                return;
            }
            if (chunk_len() != 0) {
                size_t take = std::min(n, blake3_chunk - chunk_len());
                chunk_update(p, take);
                p += take;
                n -= take;
                if (n == 0) {
                    return;
                }
                finish_chunk();
            }
            // whole subtrees straight from p while more than a chunk is
            // left: the largest power of two of chunks that fits, that the
            // counter is a multiple of, at most blake3_max_subtree; a
            // subtree may take the input to its end (it is two chunks at
            // least then, and its top node stays uncompressed on the stack)
            while (n > blake3_chunk) {
                size_t chunks = std::min(std::bit_floor(n / blake3_chunk), blake3_max_subtree);
                while ((chunk_counter & (chunks - 1)) != 0) {
                    chunks /= 2;
                }
                unsigned char cv_bytes[64];
                blake3_subtree(p, chunks, key, chunk_counter, flags, cv_bytes);
                merge(chunk_counter);
                push(cv_bytes);
                if (chunks > 1) {
                    push(cv_bytes + 32);
                }
                chunk_counter += chunks;
                p += chunks * blake3_chunk;
                n -= chunks * blake3_chunk;
            }
            if (n != 0) {
                merge(chunk_counter);
                chunk_update(p, n);
            }
        }

        // The root node: the chunk being filled (the whole input, when the
        // stack is empty), or the parent of the stack's entries and it,
        // folded from the right. What a root is: a chaining value, a block,
        // its length and its flags, compressed with ROOT per output block
        struct Root {
            uint32_t cv[8];
            uint32_t m[16];
            uint32_t block_len;
            uint32_t flags;
            uint64_t counter;   // the node's own, for a non-root chaining value of it
        };

        void root(Root& r) const noexcept {
            size_t i = stack_len;
            if (chunk_len() != 0 || stack_len == 0) {
                // the chunk being filled (the whole input, when the stack is empty)
                unsigned char last[blake3_block] = {};
                std::memcpy(last, block, block_len);
                std::memcpy(r.cv, cv, sizeof r.cv);
                blake3_load_block(r.m, last);
                r.block_len = block_len;
                r.flags = flags | start_flag() | blake3_chunk_end;
                r.counter = chunk_counter;
                secure_zero(last, sizeof last);
            } else {
                // the input ended with a subtree: its top node over the last two entries
                std::memcpy(r.cv, key, sizeof r.cv);
                std::memcpy(r.m, stack[i - 2], 32);
                std::memcpy(r.m + 8, stack[i - 1], 32);
                r.block_len = uint32_t(blake3_block);
                r.flags = flags | blake3_parent;
                r.counter = 0;
                i -= 2;
            }
            while (i-- != 0) {
                // the node so far as a non-root chaining value, the right child
                uint32_t right[8];
                std::memcpy(right, r.cv, sizeof right);
                blake3_compress_in_place(right, r.m, r.counter, r.block_len, r.flags);
                std::memcpy(r.cv, key, sizeof r.cv);
                std::memcpy(r.m, stack[i], 32);
                std::memcpy(r.m + 8, right, 32);
                r.block_len = uint32_t(blake3_block);
                r.flags = flags | blake3_parent;
                r.counter = 0;
            }
        }

        // out.size() bytes of the output from byte `position`
        void output(unsigned char* out, size_t n, uint64_t position) const noexcept {
            if (n == 0) {
                return;
            }
            Root r;
            root(r);
            uint64_t t = position / blake3_block;
            size_t skip = size_t(position % blake3_block);
            unsigned char buf[4 * blake3_block];
            const uint32_t f = r.flags | blake3_root;
            if (skip != 0) {
                blake3_compress_xof(r.cv, r.m, t, r.block_len, f, buf);
                size_t take = std::min(n, blake3_block - skip);
                std::memcpy(out, buf + skip, take);
                out += take;
                n -= take;
                ++t;
            }
#if defined(SGCL_CRYPTO_NEON)
            while (n >= 4 * blake3_block) {
                blake3_xof4_neon(r.cv, r.m, r.block_len, f, t, out);
                out += 4 * blake3_block;
                n -= 4 * blake3_block;
                t += 4;
            }
#endif
            while (n >= blake3_block) {
                blake3_compress_xof(r.cv, r.m, t, r.block_len, f, out);
                out += blake3_block;
                n -= blake3_block;
                ++t;
            }
            if (n != 0) {
                blake3_compress_xof(r.cv, r.m, t, r.block_len, f, buf);
                std::memcpy(out, buf, n);
            }
            secure_zero(buf, sizeof buf);
            secure_zero(&r, sizeof r);
        }
    };
}
