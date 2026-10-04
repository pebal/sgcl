//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "paths.h"
#include "md.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <utility>

// The compression function of SHA-384, SHA-512 and SHA-512/256 (FIPS 180-4
// §6.4.2): 80 steps over eight words of 64 bits. Two paths: plain C++, and
// on arm64 the SHA-512 instructions of ARMv8.2 (SHA512H and SHA512H2 for
// two steps, SHA512SU0/SU1 for two words of the schedule), chosen by
// cpu.h. The constants are the first 64 bits of the fractional parts of the
// cube roots of the first 80 primes (§4.2.3); the initial values of
// SHA-512 those of the square roots of the first 8 primes, of SHA-384 of
// the 9th to 16th (§5.3.4, §5.3.5), and of SHA-512/256 what the IV
// generation function of §5.3.6 gives for "SHA-512/256".
namespace sgcl::crypto::detail {
    alignas(16) inline constexpr uint64_t sha512_k[80] = {
        0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full, 0xe9b5dba58189dbbcull,
        0x3956c25bf348b538ull, 0x59f111f1b605d019ull, 0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull,
        0xd807aa98a3030242ull, 0x12835b0145706fbeull, 0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull,
        0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull, 0xc19bf174cf692694ull,
        0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull, 0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull,
        0x2de92c6f592b0275ull, 0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull,
        0x983e5152ee66dfabull, 0xa831c66d2db43210ull, 0xb00327c898fb213full, 0xbf597fc7beef0ee4ull,
        0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull, 0x06ca6351e003826full, 0x142929670a0e6e70ull,
        0x27b70a8546d22ffcull, 0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull, 0x53380d139d95b3dfull,
        0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull, 0x92722c851482353bull,
        0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull, 0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull,
        0xd192e819d6ef5218ull, 0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull,
        0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull, 0x2748774cdf8eeb99ull, 0x34b0bcb5e19b48a8ull,
        0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull, 0x5b9cca4f7763e373ull, 0x682e6ff3d6b2b8a3ull,
        0x748f82ee5defb2fcull, 0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
        0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull, 0xc67178f2e372532bull,
        0xca273eceea26619cull, 0xd186b8c721c0c207ull, 0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull,
        0x06f067aa72176fbaull, 0x0a637dc5a2c898a6ull, 0x113f9804bef90daeull, 0x1b710b35131c471bull,
        0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull, 0x431d67c49c100d4cull,
        0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull, 0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull,
    };

    inline constexpr uint64_t sha512_iv[8] = {
        0x6a09e667f3bcc908ull, 0xbb67ae8584caa73bull, 0x3c6ef372fe94f82bull, 0xa54ff53a5f1d36f1ull,
        0x510e527fade682d1ull, 0x9b05688c2b3e6c1full, 0x1f83d9abfb41bd6bull, 0x5be0cd19137e2179ull,
    };

    inline constexpr uint64_t sha384_iv[8] = {
        0xcbbb9d5dc1059ed8ull, 0x629a292a367cd507ull, 0x9159015a3070dd17ull, 0x152fecd8f70e5939ull,
        0x67332667ffc00b31ull, 0x8eb44a8768581511ull, 0xdb0c2e0d64f98fa7ull, 0x47b5481dbefa4fa4ull,
    };

    inline constexpr uint64_t sha512_256_iv[8] = {
        0x22312194fc2bf72cull, 0x9f555fa3c84c64c2ull, 0x2393b86b6f53b151ull, 0x963877195940eabdull,
        0x96283ee2a88effe3ull, 0xbe5e1e2553863992ull, 0x2b0199fc2c85b8aaull, 0x0eb72ddc81c52ca2ull,
    };

    inline void sha512_compress_portable(uint64_t* h, const unsigned char* p, size_t blocks) noexcept {
        for (; blocks != 0; --blocks, p += 128) {
            uint64_t w[16];
            for (int i = 0; i < 16; ++i) {
                w[i] = load_be64(p + 8 * i);
            }
            uint64_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
            for (int t = 0; t < 80; ++t) {
                if (t >= 16) {
                    uint64_t w15 = w[(t - 15) & 15], w2 = w[(t - 2) & 15];
                    uint64_t s0 = std::rotr(w15, 1) ^ std::rotr(w15, 8) ^ (w15 >> 7);
                    uint64_t s1 = std::rotr(w2, 19) ^ std::rotr(w2, 61) ^ (w2 >> 6);
                    w[t & 15] += s0 + w[(t - 7) & 15] + s1;
                }
                uint64_t t1 = hh + (std::rotr(e, 14) ^ std::rotr(e, 18) ^ std::rotr(e, 41)) + ((e & f) ^ (~e & g))
                            + sha512_k[t] + w[t & 15];
                uint64_t t2 = (std::rotr(a, 28) ^ std::rotr(a, 34) ^ std::rotr(a, 39)) + ((a & b) ^ (a & c) ^ (b & c));
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
    // Forty rounds of two steps. The state is four registers of two words,
    // ab, cd, ef and gh (a in the low lane of ab), plus a fifth: a round
    // writes the new ab over gh and the new ef into the free register, so
    // the names move over five registers and come back to their places
    // every five rounds (s below, the order of each round in the table).
    //
    // Round j: the two words of the schedule plus their constants, halves
    // swapped, go into gh (h + KW[j], g + KW[j+1]); SHA512H takes that with
    // (f, g) and (d, e) and gives the two T1 of the round; ef' = cd + T1
    // (the e of both steps); SHA512H2 takes the T1 with c and ab and gives
    // ab' (the a of both steps). The first 32 rounds make the schedule's
    // words for round j + 8 in the register they used: SHA512SU0 with the
    // next register (W[t+1]), SHA512SU1 with the last one (W[t+14..15])
    // and W[t+9..10], which straddles two registers.
    SGCL_TARGET_ARM64_SHA3
    inline void sha512_compress_arm64(uint64_t* h, const unsigned char* p, size_t blocks) noexcept {
        // which of the five registers is ab, cd, ef, gh and the free one, round by round
        static constexpr int order[5][5] = {{0, 1, 2, 3, 4}, {3, 0, 4, 2, 1}, {2, 3, 1, 4, 0}, {4, 2, 0, 1, 3}, {1, 4, 3, 0, 2}};
        uint64x2_t s[5];
        s[0] = vld1q_u64(h);
        s[1] = vld1q_u64(h + 2);
        s[2] = vld1q_u64(h + 4);
        s[3] = vld1q_u64(h + 6);
        for (; blocks != 0; --blocks, p += 128) {
            const uint64x2_t ab0 = s[0], cd0 = s[1], ef0 = s[2], gh0 = s[3];
            uint64x2_t m[8];
            for (int i = 0; i < 8; ++i) {
                m[i] = vreinterpretq_u64_u8(vrev64q_u8(vld1q_u8(p + 16 * i)));
            }
            [&]<size_t... J>(std::index_sequence<J...>) SGCL_INLINE_ARM64_SHA3 {
                auto round = [&]<size_t R>(std::integral_constant<size_t, R>) SGCL_INLINE_ARM64_SHA3 {
                    constexpr int ab = order[R % 5][0], cd = order[R % 5][1], ef = order[R % 5][2];
                    constexpr int gh = order[R % 5][3], next = order[R % 5][4];
                    uint64x2_t kw = vaddq_u64(m[R % 8], vld1q_u64(sha512_k + 2 * R));
                    kw = vextq_u64(kw, kw, 1);
                    uint64x2_t fg = vextq_u64(s[ef], s[gh], 1);
                    uint64x2_t de = vextq_u64(s[cd], s[ef], 1);
                    s[gh] = vaddq_u64(s[gh], kw);
                    s[gh] = vsha512hq_u64(s[gh], fg, de);
                    s[next] = vaddq_u64(s[cd], s[gh]);
                    s[gh] = vsha512h2q_u64(s[gh], s[cd], s[ab]);
                    if constexpr (R < 32) {
                        m[R % 8] = vsha512su1q_u64(vsha512su0q_u64(m[R % 8], m[(R + 1) % 8]), m[(R + 7) % 8],
                                                   vextq_u64(m[(R + 4) % 8], m[(R + 5) % 8], 1));
                    }
                };
                (round(std::integral_constant<size_t, J>()), ...);
            }(std::make_index_sequence<40>());
            // forty rounds are eight turns of five: every name is back in its register
            s[0] = vaddq_u64(s[0], ab0);
            s[1] = vaddq_u64(s[1], cd0);
            s[2] = vaddq_u64(s[2], ef0);
            s[3] = vaddq_u64(s[3], gh0);
        }
        vst1q_u64(h, s[0]);
        vst1q_u64(h + 2, s[1]);
        vst1q_u64(h + 4, s[2]);
        vst1q_u64(h + 6, s[3]);
    }
#endif

    struct Sha512Traits {
        using word = uint64_t;
        static constexpr size_t words = 8;
        static constexpr size_t block = 128;
        static constexpr size_t length_bytes = 16;

        SGCL_INLINE_HOT static void compress(uint64_t* h, const unsigned char* p, size_t blocks) noexcept {
#if defined(SGCL_CRYPTO_ARM64)
            if (sgcl::detail::cpu::sha512()) {
                sha512_compress_arm64(h, p, blocks);
                return;
            }
#endif
            sha512_compress_portable(h, p, blocks);
        }
    };
}
