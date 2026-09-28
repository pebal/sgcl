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

// The compression function of SHA-224 and SHA-256 (FIPS 180-4 §6.2.2): 64
// steps over eight words. Two paths: plain C++, and on arm64 the SHA-256
// instructions of ARMv8 (SHA256H and SHA256H2 for four steps on the two
// halves of the state, SHA256SU0/SU1 for four words of the schedule),
// chosen by cpu.h. The constants are the first 32 bits of the fractional
// parts of the cube roots of the first 64 primes (§4.2.2), the initial
// values those of the square roots of the first 8 primes (§5.3.3) and, for
// SHA-224, the second 32 bits of those of the 9th to 16th (§5.3.2).
namespace sgcl::crypto::detail {
    alignas(16) inline constexpr uint32_t sha256_k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
    };

    inline constexpr uint32_t sha256_iv[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au, 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };

    inline constexpr uint32_t sha224_iv[8] = {
        0xc1059ed8u, 0x367cd507u, 0x3070dd17u, 0xf70e5939u, 0xffc00b31u, 0x68581511u, 0x64f98fa7u, 0xbefa4fa4u,
    };

    inline void sha256_compress_portable(uint32_t* h, const unsigned char* p, size_t blocks) noexcept {
        for (; blocks != 0; --blocks, p += 64) {
            uint32_t w[16];
            for (int i = 0; i < 16; ++i) {
                w[i] = load_be32(p + 4 * i);
            }
            uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
            for (int t = 0; t < 64; ++t) {
                if (t >= 16) {
                    uint32_t w15 = w[(t - 15) & 15], w2 = w[(t - 2) & 15];
                    uint32_t s0 = std::rotr(w15, 7) ^ std::rotr(w15, 18) ^ (w15 >> 3);
                    uint32_t s1 = std::rotr(w2, 17) ^ std::rotr(w2, 19) ^ (w2 >> 10);
                    w[t & 15] += s0 + w[(t - 7) & 15] + s1;
                }
                uint32_t t1 = hh + (std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25)) + ((e & f) ^ (~e & g))
                            + sha256_k[t] + w[t & 15];
                uint32_t t2 = (std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
                hh = g;
                g = f;
                f = e;
                e = d + t1;
                d = c;
                c = b;
                b = a;
                a = t1 + t2;
            }
            h[0] += a;
            h[1] += b;
            h[2] += c;
            h[3] += d;
            h[4] += e;
            h[5] += f;
            h[6] += g;
            h[7] += hh;
        }
    }

#if defined(SGCL_CRYPTO_ARM64)
    // Sixteen rounds of four steps, the state in two registers, abcd and
    // efgh. Round q adds the constants to the schedule's words 4q..4q+3
    // (m[q % 4]); SHA256H gives the new abcd from both halves, SHA256H2 the
    // new efgh from the old abcd. The first twelve rounds make the words of
    // round q + 4 in the register they used: SHA256SU0 with the next
    // register, SHA256SU1 with the two after it.
    SGCL_CRYPTO_TARGET_SHA2
    inline void sha256_compress_arm64(uint32_t* h, const unsigned char* p, size_t blocks) noexcept {
        uint32x4_t abcd = vld1q_u32(h);
        uint32x4_t efgh = vld1q_u32(h + 4);
        for (; blocks != 0; --blocks, p += 64) {
            const uint32x4_t abcd0 = abcd;
            const uint32x4_t efgh0 = efgh;
            uint32x4_t m[4];
            for (int i = 0; i < 4; ++i) {
                m[i] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(p + 16 * i)));
            }
            [&]<size_t... Q>(std::index_sequence<Q...>) SGCL_CRYPTO_INLINE_SHA2 {
                auto round = [&]<size_t R>(std::integral_constant<size_t, R>) SGCL_CRYPTO_INLINE_SHA2 {
                    uint32x4_t t = vaddq_u32(m[R % 4], vld1q_u32(sha256_k + 4 * R));
                    uint32x4_t old = abcd;
                    abcd = vsha256hq_u32(abcd, efgh, t);
                    efgh = vsha256h2q_u32(efgh, old, t);
                    if constexpr (R < 12) {
                        m[R % 4] = vsha256su1q_u32(vsha256su0q_u32(m[R % 4], m[(R + 1) % 4]), m[(R + 2) % 4], m[(R + 3) % 4]);
                    }
                };
                (round(std::integral_constant<size_t, Q>()), ...);
            }(std::make_index_sequence<16>());
            abcd = vaddq_u32(abcd, abcd0);
            efgh = vaddq_u32(efgh, efgh0);
        }
        vst1q_u32(h, abcd);
        vst1q_u32(h + 4, efgh);
    }
#endif

    struct Sha256Traits {
        using word = uint32_t;
        static constexpr size_t words = 8;
        static constexpr size_t block = 64;
        static constexpr size_t length_bytes = 8;

        static void compress(uint32_t* h, const unsigned char* p, size_t blocks) noexcept {
#if defined(SGCL_CRYPTO_ARM64)
            if (cpu::sha256()) {
                sha256_compress_arm64(h, p, blocks);
                return;
            }
#endif
            sha256_compress_portable(h, p, blocks);
        }
    };
}
