//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "cpu.h"
#include "md.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <utility>

// The compression function of SHA-1 (FIPS 180-4 §6.1.2): 80 steps over five
// words, the schedule of sixteen words kept as a ring. Two paths: plain C++,
// and on arm64 the SHA-1 instructions of ARMv8 (SHA1C, SHA1P, SHA1M for
// four steps each with their function, SHA1H for the rotation of a,
// SHA1SU0/SU1 for four words of the schedule), chosen by cpu.h.
namespace sgcl::crypto::detail {
    inline constexpr uint32_t sha1_iv[5] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u};

    inline void sha1_compress_portable(uint32_t* h, const unsigned char* p, size_t blocks) noexcept {
        for (; blocks != 0; --blocks, p += 64) {
            uint32_t w[16];
            for (int i = 0; i < 16; ++i) {
                w[i] = load_be32(p + 4 * i);
            }
            uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
            for (int t = 0; t < 80; ++t) {
                if (t >= 16) {
                    w[t & 15] = std::rotl(w[(t - 3) & 15] ^ w[(t - 8) & 15] ^ w[(t - 14) & 15] ^ w[t & 15], 1);
                }
                uint32_t f, k;
                if (t < 20) {
                    f = (b & c) | (~b & d);            // Ch
                    k = 0x5a827999u;
                } else if (t < 40) {
                    f = b ^ c ^ d;                     // Parity
                    k = 0x6ed9eba1u;
                } else if (t < 60) {
                    f = (b & c) | (b & d) | (c & d);   // Maj
                    k = 0x8f1bbcdcu;
                } else {
                    f = b ^ c ^ d;
                    k = 0xca62c1d6u;
                }
                uint32_t tmp = std::rotl(a, 5) + f + e + k + w[t & 15];
                e = d;
                d = c;
                c = std::rotl(b, 30);
                b = a;
                a = tmp;
            }
            h[0] += a;
            h[1] += b;
            h[2] += c;
            h[3] += d;
            h[4] += e;
        }
    }

#if defined(SGCL_CRYPTO_ARM64)
    // Twenty rounds of four steps. Round q takes the schedule's words
    // 4q..4q+3 (m[q % 4]) plus the constant of its twenty steps and runs
    // them with the function of its fifth of the steps; the e of the next
    // round is a of this one rotated by 30 (SHA1H), taken before the round
    // changes a. Round q, once its words are used, makes the words of round
    // q + 4 in the same register: SHA1SU0 over the three oldest, SHA1SU1
    // with the newest.
    SGCL_CRYPTO_TARGET_SHA2
    inline void sha1_compress_arm64(uint32_t* h, const unsigned char* p, size_t blocks) noexcept {
        static constexpr uint32_t k[4] = {0x5a827999u, 0x6ed9eba1u, 0x8f1bbcdcu, 0xca62c1d6u};
        uint32x4_t abcd = vld1q_u32(h);
        uint32_t e = h[4];
        for (; blocks != 0; --blocks, p += 64) {
            const uint32x4_t abcd0 = abcd;
            const uint32_t e0 = e;
            uint32x4_t m[4];
            for (int i = 0; i < 4; ++i) {
                m[i] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(p + 16 * i)));
            }
            [&]<size_t... Q>(std::index_sequence<Q...>) SGCL_CRYPTO_INLINE_SHA2 {
                auto round = [&]<size_t R>(std::integral_constant<size_t, R>) SGCL_CRYPTO_INLINE_SHA2 {
                    uint32x4_t t = vaddq_u32(m[R % 4], vdupq_n_u32(k[R / 5]));
                    uint32_t next_e = vsha1h_u32(vgetq_lane_u32(abcd, 0));
                    if constexpr (R < 5) {
                        abcd = vsha1cq_u32(abcd, e, t);
                    } else if constexpr (R < 10 || R >= 15) {
                        abcd = vsha1pq_u32(abcd, e, t);
                    } else {
                        abcd = vsha1mq_u32(abcd, e, t);
                    }
                    e = next_e;
                    if constexpr (R < 16) {
                        m[R % 4] = vsha1su1q_u32(vsha1su0q_u32(m[R % 4], m[(R + 1) % 4], m[(R + 2) % 4]), m[(R + 3) % 4]);
                    }
                };
                (round(std::integral_constant<size_t, Q>()), ...);
            }(std::make_index_sequence<20>());
            abcd = vaddq_u32(abcd, abcd0);
            e += e0;
        }
        vst1q_u32(h, abcd);
        h[4] = e;
    }
#endif

    struct Sha1Traits {
        using word = uint32_t;
        static constexpr size_t words = 5;
        static constexpr size_t block = 64;
        static constexpr size_t length_bytes = 8;

        static void compress(uint32_t* h, const unsigned char* p, size_t blocks) noexcept {
#if defined(SGCL_CRYPTO_ARM64)
            if (cpu::sha1()) {
                sha1_compress_arm64(h, p, blocks);
                return;
            }
#endif
            sha1_compress_portable(h, p, blocks);
        }
    };
}
