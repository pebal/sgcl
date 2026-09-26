//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <bit>
#include <cstdint>
#include <cstring>

// The generator under math::random: ChaCha8Rand as the C2SP specifies it
// (c2sp.org/chacha8rand), which is what Go's math/rand/v2 ChaCha8 and its
// runtime's generator are, so that one key gives the same stream here and
// there. In short: a key of 32 bytes is the key of ChaCha with eight
// rounds and a zero nonce; sixteen blocks are made with the counters 0 to
// 15, each block's input added back to its output for the key words only
// (the constants, the counter and the nonce are public, so adding them
// buys nothing); the blocks go out four at a time, interleaved a word of
// each in turn; and of the 1024 bytes the last 32 are the next key, the
// other 992 the output. A key is thus used once and forgotten: what was
// drawn before cannot be worked back from the state.
namespace sgcl::math::detail {
    // Four lanes of 32 bits, one word of each of the four blocks: the
    // compiler's generic vector (clang and gcc; the module needs their
    // __int128 anyway), which it lowers to one NEON or SSE register and
    // each operation to one instruction. Written as plain loops over the
    // lanes instead, the same code stayed scalar — with the quarter round
    // inlined by force, 128 scalar rotations and 400 loads and stores a
    // call — so here the vector is spelled out, and no intrinsic of any
    // one instruction set is named.
    using Lanes = uint32_t __attribute__((vector_size(16)));

    template<int N>
    [[gnu::always_inline]] inline Lanes rotate(Lanes v) noexcept {
        return (v << N) | (v >> (32 - N));
    }

    [[gnu::always_inline]] inline void chacha8_quarter(Lanes& a, Lanes& b, Lanes& c, Lanes& d) noexcept {
        a += b; d = rotate<16>(d ^ a);
        c += d; b = rotate<12>(b ^ c);
        a += b; d = rotate<8>(d ^ a);
        c += d; b = rotate<7>(b ^ c);
    }

    // Four blocks at once, counters counter .. counter + 3, into 32 words
    // of output: the state of the four side by side, one vector of four
    // lanes for each of the sixteen words, which is the interleaving the
    // specification asks for as it lies in memory.
    inline void chacha8_blocks(const uint64_t (&key)[4], uint32_t counter, uint64_t (&out)[32]) noexcept {
        const uint32_t k[8] = {
            uint32_t(key[0]), uint32_t(key[0] >> 32), uint32_t(key[1]), uint32_t(key[1] >> 32),
            uint32_t(key[2]), uint32_t(key[2] >> 32), uint32_t(key[3]), uint32_t(key[3] >> 32)};
        Lanes x[16];
        x[0] = Lanes{0x61707865, 0x61707865, 0x61707865, 0x61707865};
        x[1] = Lanes{0x3320646e, 0x3320646e, 0x3320646e, 0x3320646e};
        x[2] = Lanes{0x79622d32, 0x79622d32, 0x79622d32, 0x79622d32};
        x[3] = Lanes{0x6b206574, 0x6b206574, 0x6b206574, 0x6b206574};
        for (int w = 0; w < 8; ++w) {
            x[4 + w] = Lanes{k[w], k[w], k[w], k[w]};
        }
        x[12] = Lanes{counter, counter + 1, counter + 2, counter + 3};
        x[13] = Lanes{0, 0, 0, 0};
        x[14] = Lanes{0, 0, 0, 0};
        x[15] = Lanes{0, 0, 0, 0};
        for (int round = 0; round < 4; ++round) {   // eight rounds: four of columns and diagonals
            chacha8_quarter(x[0], x[4], x[8], x[12]);
            chacha8_quarter(x[1], x[5], x[9], x[13]);
            chacha8_quarter(x[2], x[6], x[10], x[14]);
            chacha8_quarter(x[3], x[7], x[11], x[15]);
            chacha8_quarter(x[0], x[5], x[10], x[15]);
            chacha8_quarter(x[1], x[6], x[11], x[12]);
            chacha8_quarter(x[2], x[7], x[8], x[13]);
            chacha8_quarter(x[3], x[4], x[9], x[14]);
        }
        // The key added back, and only the key
        for (int w = 0; w < 8; ++w) {
            x[4 + w] += Lanes{k[w], k[w], k[w], k[w]};
        }
        // Word w of lanes 0 and 1, then of lanes 2 and 3, little-endian:
        // the bytes as they lie in x
        for (int w = 0; w < 16; ++w) {
            out[2 * w] = uint64_t(x[w][0]) | uint64_t(x[w][1]) << 32;
            out[2 * w + 1] = uint64_t(x[w][2]) | uint64_t(x[w][3]) << 32;
        }
    }

    // The state: the key of the current sixteen blocks, the four of them
    // made last and where the reading is in them. 296 bytes, nothing a
    // collector has to look at.
    struct ChaCha8 {
        uint64_t buffer[32];
        uint64_t key[4];
        uint32_t counter;   // of the first of the four blocks in the buffer: 0, 4, 8, 12
        uint32_t next;      // the next word of the buffer to hand out
        uint32_t end;       // 32, or 28 in the last four blocks, whose last four words are the next key

        void init(const uint64_t (&k)[4]) noexcept {
            std::memcpy(key, k, sizeof key);
            counter = 0;
            chacha8_blocks(key, 0, buffer);
            next = 0;
            end = 32;
        }

        void init(const unsigned char (&bytes)[32]) noexcept {
            uint64_t k[4];
            for (int i = 0; i < 4; ++i) {
                uint64_t w = 0;
                for (int b = 7; b >= 0; --b) {
                    w = w << 8 | bytes[i * 8 + b];
                }
                k[i] = w;
            }
            init(k);
        }

        uint64_t take() noexcept {
            if (next == end) [[unlikely]] {
                refill();
            }
            return buffer[next++];
        }

        // The next four blocks; after the last four of sixteen, the key
        // their last four words made
        void refill() noexcept {
            counter += 4;
            if (counter == 16) {
                std::memcpy(key, buffer + 28, sizeof key);
                counter = 0;
            }
            chacha8_blocks(key, counter, buffer);
            next = 0;
            end = counter == 12 ? 28 : 32;
        }
    };
}
