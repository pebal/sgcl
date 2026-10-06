//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "paths.h"
#include "sha256.h"
#include "sha512.h"
#include "../secure_zero.h"
#include "../../core/detail/bytes.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

// The compression function F of BLAKE2b and BLAKE2s (RFC 7693 §3.2): the
// state's eight words and the initialization vector make sixteen working
// words, the byte counter and the final flag go into four of them, and 12
// rounds (BLAKE2b, 64-bit words) or 10 (BLAKE2s, 32-bit words) of eight G
// mixes run over them, each round taking the sixteen message words in the
// order of its row of SIGMA (§2.7); the state is then XORed with both
// halves. The IVs are SHA-512's and SHA-256's (§2.6). Every operation is an
// addition, a XOR or a rotation by a constant: nothing depends on the data
// in its branches or addresses, a keyed hash included.
//
// One path, plain C++ with the rounds unrolled and the SIGMA indices known
// to the compiler, on every processor. NEON was written and measured
// slower (Apple M2, 64 KB, 2026-10-05): BLAKE2b with the rows in pairs of
// registers and its rotations as XAR of the SHA-3 extension 0.88 GB/s
// against the scalar 1.05, BLAKE2s with a row a register and the message
// gathered by TBL 0.33 against 0.62. One block's sixteen G are a chain of
// dependent operations, and a vector operation's latency (two to three
// cycles) is what the chain pays, where the four G of a step run side by
// side on the scalar units at one cycle each; vectors pay only across
// independent blocks (BLAKE2bp, BLAKE3's chunks), which sequential BLAKE2
// does not have.
namespace sgcl::crypto::detail {
    // §2.7: the message schedule; rounds 10 and 11 of BLAKE2b reuse rows 0 and 1
    inline constexpr uint8_t blake2_sigma[12][16] = {
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
        {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
        {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
        {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
        {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
        {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
        {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
        {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
        {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
        {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
        {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
    };

    // The two variants: the word, the block (16 words), the rounds and the
    // rotations of G (§2.1), the IV, and the limits of the parameter block
    struct Blake2bTraits {
        using word = uint64_t;
        static constexpr size_t block = 128;
        static constexpr int rounds = 12;
        static constexpr int r1 = 32, r2 = 24, r3 = 16, r4 = 63;
        static constexpr size_t max_digest = 64;
        static constexpr size_t max_key = 64;
        static constexpr size_t salt = 16;   // and the personalization
        static constexpr const uint64_t* iv = sha512_iv;

        SGCL_INLINE_HOT static word load(const unsigned char* p) noexcept {
            return load_le64(p);
        }

        SGCL_INLINE_HOT static void store(unsigned char* p, word w) noexcept {
            store_le64(p, w);
        }
    };

    struct Blake2sTraits {
        using word = uint32_t;
        static constexpr size_t block = 64;
        static constexpr int rounds = 10;
        static constexpr int r1 = 16, r2 = 12, r3 = 8, r4 = 7;
        static constexpr size_t max_digest = 32;
        static constexpr size_t max_key = 32;
        static constexpr size_t salt = 8;
        static constexpr const uint32_t* iv = sha256_iv;

        SGCL_INLINE_HOT static word load(const unsigned char* p) noexcept {
            return load_le32(p);
        }

        SGCL_INLINE_HOT static void store(unsigned char* p, word w) noexcept {
            store_le32(p, w);
        }
    };

    // F over `blocks` whole blocks at p, the counter t (bytes, two words:
    // the low and the high) advanced by inc for each, the last one with the
    // final flag when `last` (f0 = all ones; f1, the last node's flag of
    // tree hashing, stays 0)
    template<class Tr>
    inline void blake2_compress_portable(typename Tr::word* h, const unsigned char* p, size_t blocks,
                                         typename Tr::word* t, size_t inc, bool last) noexcept {
        using W = typename Tr::word;
        for (; blocks != 0; --blocks, p += Tr::block) {
            t[0] += W(inc);
            t[1] += W(t[0] < W(inc));
            W m[16];
            for (int i = 0; i < 16; ++i) {
                m[i] = Tr::load(p + sizeof(W) * i);
            }
            W v[16];
            for (int i = 0; i < 8; ++i) {
                v[i] = h[i];
                v[i + 8] = Tr::iv[i];
            }
            v[12] ^= t[0];
            v[13] ^= t[1];
            v[14] ^= W(0) - W(last && blocks == 1);
            [&]<size_t... R>(std::index_sequence<R...>) SGCL_CRYPTO_LAMBDA_INLINE {
                auto round = [&]<size_t Q>(std::integral_constant<size_t, Q>) SGCL_CRYPTO_LAMBDA_INLINE {
                    constexpr const uint8_t* s = blake2_sigma[Q];
                    auto g = [&](int a, int b, int c, int d, W x, W y) SGCL_CRYPTO_LAMBDA_INLINE {
                        v[a] = v[a] + v[b] + x;
                        v[d] = std::rotr(W(v[d] ^ v[a]), Tr::r1);
                        v[c] = v[c] + v[d];
                        v[b] = std::rotr(W(v[b] ^ v[c]), Tr::r2);
                        v[a] = v[a] + v[b] + y;
                        v[d] = std::rotr(W(v[d] ^ v[a]), Tr::r3);
                        v[c] = v[c] + v[d];
                        v[b] = std::rotr(W(v[b] ^ v[c]), Tr::r4);
                    };
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
            }(std::make_index_sequence<Tr::rounds>());
            for (int i = 0; i < 8; ++i) {
                h[i] ^= v[i] ^ v[i + 8];
            }
        }
    }

    template<class Tr>
    SGCL_INLINE_HOT void blake2_compress(typename Tr::word* h, const unsigned char* p, size_t blocks, typename Tr::word* t,
                                         size_t inc, bool last) noexcept {
        blake2_compress_portable<Tr>(h, p, blocks, t, inc, last);
    }

    // The incremental hash: the chaining words, the counter, one block of
    // buffer that always keeps the last block back (F needs to know it is
    // the last), and what reset() goes back to: the words after the
    // parameter block, and the key's block when there is one (§3.3: a key
    // is the first block of the message, padded with zeros)
    template<class Tr>
    struct Blake2State {
        using word = typename Tr::word;
        static constexpr size_t block = Tr::block;

        word h[8];
        word t[2];
        unsigned char buffer[block];
        uint32_t used;
        uint8_t digest_size;
        uint8_t key_size;
        word start[8];
        unsigned char key[Tr::max_key];

        // the parameter block of §2.5 (the sequential mode: fanout and
        // depth 1, leaf length, node offset, node depth and inner length
        // 0), with the salt and the personalization zero-padded to their
        // fields; the sizes checked by the caller
        void init(size_t size, const unsigned char* k, size_t kk, const unsigned char* salt, size_t salt_size,
                  const unsigned char* personal, size_t personal_size) noexcept {
            unsigned char param[sizeof(word) * 8] = {};
            param[0] = static_cast<unsigned char>(size);
            param[1] = static_cast<unsigned char>(kk);
            param[2] = 1;
            param[3] = 1;
            constexpr size_t salt_at = sizeof(word) == 8 ? 32 : 16;
            if (salt_size != 0) {
                sgcl::detail::copy_bytes(param + salt_at, salt, salt_size);
            }
            if (personal_size != 0) {
                sgcl::detail::copy_bytes(param + salt_at + Tr::salt, personal, personal_size);
            }
            for (int i = 0; i < 8; ++i) {
                start[i] = Tr::iv[i] ^ Tr::load(param + sizeof(word) * i);
            }
            digest_size = uint8_t(size);
            key_size = uint8_t(kk);
            std::memset(key, 0, sizeof key);
            if (kk != 0) {
                sgcl::detail::copy_bytes(key, k, kk);
            }
            reset();
        }

        SGCL_INLINE_HOT void reset() noexcept {
            std::memcpy(h, start, sizeof h);
            t[0] = t[1] = 0;
            if (key_size != 0) {
                std::memset(buffer, 0, block);
                std::memcpy(buffer, key, sizeof key);
                used = uint32_t(block);
            } else {
                used = 0;
            }
        }

        void update(const unsigned char* p, size_t n) noexcept {
            if (n == 0) {
                return;
            }
            // the buffer is emptied only when more bytes follow it
            if (used != 0) {
                size_t take = std::min(n, block - used);
                sgcl::detail::copy_bytes(buffer + used, p, take);
                used += uint32_t(take);
                p += take;
                n -= take;
                if (n == 0) {
                    return;
                }
                blake2_compress<Tr>(h, buffer, 1, t, block, false);
                used = 0;
            }
            // whole blocks straight from the caller's memory, all but the
            // last one, which may be the message's last
            if (n > block) {
                size_t blocks = (n - 1) / block;
                blake2_compress<Tr>(h, p, blocks, t, block, false);
                p += blocks * block;
                n -= blocks * block;
            }
            sgcl::detail::copy_bytes(buffer, p, n);
            used = uint32_t(n);
        }

        // the last block, zero-padded, with the final flag; the digest the
        // first digest_size bytes of the words, little-endian. In place:
        // the state is used up
        void finish(unsigned char* out) noexcept {
            std::memset(buffer + used, 0, block - used);
            blake2_compress<Tr>(h, buffer, 1, t, used, true);
            unsigned char full[sizeof(word) * 8];
            for (int i = 0; i < 8; ++i) {
                Tr::store(full + sizeof(word) * i, h[i]);
            }
            std::memcpy(out, full, digest_size);
            secure_zero(full, sizeof full);
        }
    };
}
