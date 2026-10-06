//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "paths.h"
#include "../secure_zero.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// scrypt's ROMix (RFC 7914 §5): N blocks of 128 r bytes written in order,
// each the BlockMix of the one before, then read back N times at indices
// the data chooses, each XORed into the running block before its
// BlockMix. BlockMix (§4) runs the Salsa20/8 core (§3) over the 2r 64-byte
// pieces of a block, each XORed into the result of the one before, and
// writes the even results first and the odd ones after.
//
// The words are held as 32-bit numbers in the machine's order and the
// bytes converted once, at the start and the end of ROMix. One path, plain
// C++: the core's eight rounds are a chain of dependent operations within
// one block, and the blocks of BlockMix follow each other. Salsa20/8 on
// NEON (the words in diagonals, four quarter-rounds a vector operation,
// the rows by EXT) was measured at 84 ns a core against 45.5 for this
// one (Apple M2, 2026-10-05): the vector chain's latency, where the four
// scalar quarter-rounds run side by side.
namespace sgcl::crypto::detail {
    // Salsa20/8 over 16 words: x = core(x ^ in), in place over x
    SGCL_INLINE_HOT void salsa20_8_xor(uint32_t* x, const uint32_t* in) noexcept {
        uint32_t b[16];
        for (int i = 0; i < 16; ++i) {
            b[i] = x[i] ^ in[i];
        }
        uint32_t x0 = b[0], x1 = b[1], x2 = b[2], x3 = b[3], x4 = b[4], x5 = b[5], x6 = b[6], x7 = b[7];
        uint32_t x8 = b[8], x9 = b[9], x10 = b[10], x11 = b[11], x12 = b[12], x13 = b[13], x14 = b[14], x15 = b[15];
        for (int i = 0; i < 8; i += 2) {
            // the columns
            x4 ^= std::rotl(x0 + x12, 7);
            x8 ^= std::rotl(x4 + x0, 9);
            x12 ^= std::rotl(x8 + x4, 13);
            x0 ^= std::rotl(x12 + x8, 18);
            x9 ^= std::rotl(x5 + x1, 7);
            x13 ^= std::rotl(x9 + x5, 9);
            x1 ^= std::rotl(x13 + x9, 13);
            x5 ^= std::rotl(x1 + x13, 18);
            x14 ^= std::rotl(x10 + x6, 7);
            x2 ^= std::rotl(x14 + x10, 9);
            x6 ^= std::rotl(x2 + x14, 13);
            x10 ^= std::rotl(x6 + x2, 18);
            x3 ^= std::rotl(x15 + x11, 7);
            x7 ^= std::rotl(x3 + x15, 9);
            x11 ^= std::rotl(x7 + x3, 13);
            x15 ^= std::rotl(x11 + x7, 18);
            // the rows
            x1 ^= std::rotl(x0 + x3, 7);
            x2 ^= std::rotl(x1 + x0, 9);
            x3 ^= std::rotl(x2 + x1, 13);
            x0 ^= std::rotl(x3 + x2, 18);
            x6 ^= std::rotl(x5 + x4, 7);
            x7 ^= std::rotl(x6 + x5, 9);
            x4 ^= std::rotl(x7 + x6, 13);
            x5 ^= std::rotl(x4 + x7, 18);
            x11 ^= std::rotl(x10 + x9, 7);
            x8 ^= std::rotl(x11 + x10, 9);
            x9 ^= std::rotl(x8 + x11, 13);
            x10 ^= std::rotl(x9 + x8, 18);
            x12 ^= std::rotl(x15 + x14, 7);
            x13 ^= std::rotl(x12 + x15, 9);
            x14 ^= std::rotl(x13 + x12, 13);
            x15 ^= std::rotl(x14 + x13, 18);
        }
        x[0] = b[0] + x0;
        x[1] = b[1] + x1;
        x[2] = b[2] + x2;
        x[3] = b[3] + x3;
        x[4] = b[4] + x4;
        x[5] = b[5] + x5;
        x[6] = b[6] + x6;
        x[7] = b[7] + x7;
        x[8] = b[8] + x8;
        x[9] = b[9] + x9;
        x[10] = b[10] + x10;
        x[11] = b[11] + x11;
        x[12] = b[12] + x12;
        x[13] = b[13] + x13;
        x[14] = b[14] + x14;
        x[15] = b[15] + x15;
    }

    // out = BlockMix(in ^ v) (v may be null: BlockMix(in)), 32 r words each
    inline void scrypt_block_mix(const uint32_t* in, const uint32_t* v, uint32_t* out, uint32_t r) noexcept {
        uint32_t x[16];
        const size_t last = size_t(2 * r - 1) * 16;
        if (v) {
            for (int i = 0; i < 16; ++i) {
                x[i] = in[last + i] ^ v[last + i];
            }
        } else {
            std::memcpy(x, in + last, sizeof x);
        }
        for (uint32_t i = 0; i < 2 * r; ++i) {
            const uint32_t* piece = in + size_t(i) * 16;
            if (v) {
                uint32_t t[16];
                const uint32_t* vp = v + size_t(i) * 16;
                for (int k = 0; k < 16; ++k) {
                    t[k] = piece[k] ^ vp[k];
                }
                salsa20_8_xor(x, t);
            } else {
                salsa20_8_xor(x, piece);
            }
            std::memcpy(out + (size_t(i / 2) + size_t(i % 2) * r) * 16, x, sizeof x);
        }
    }

    // ROMix over one block of 128 r bytes at b, in place; v is N blocks of
    // scratch, xy two blocks
    inline void scrypt_ro_mix(unsigned char* b, uint32_t r, uint32_t n, uint32_t* v, uint32_t* xy) noexcept {
        const size_t words = size_t(32) * r;
        uint32_t* x = xy;
        uint32_t* y = xy + words;
        for (size_t i = 0; i < words; ++i) {
            x[i] = load_le32(b + 4 * i);
        }
        for (uint32_t i = 0; i < n; i += 2) {
            std::memcpy(v + size_t(i) * words, x, words * 4);
            scrypt_block_mix(x, nullptr, y, r);
            std::memcpy(v + size_t(i + 1) * words, y, words * 4);
            scrypt_block_mix(y, nullptr, x, r);
        }
        const size_t integerify = size_t(2 * r - 1) * 16;
        for (uint32_t i = 0; i < n; i += 2) {
            uint32_t j = x[integerify] & (n - 1);
            scrypt_block_mix(x, v + size_t(j) * words, y, r);
            j = y[integerify] & (n - 1);
            scrypt_block_mix(y, v + size_t(j) * words, x, r);
        }
        for (size_t i = 0; i < words; ++i) {
            store_le32(b + 4 * i, x[i]);
        }
    }

    // The scratch of ROMix: plain memory, zeroed before it is freed
    struct ScryptMemory {
        uint32_t* words = nullptr;
        size_t count = 0;

        explicit ScryptMemory(size_t n) noexcept
        : words(static_cast<uint32_t*>(::operator new(n * sizeof(uint32_t), std::align_val_t(64)))), count(n) {
        }

        ScryptMemory(const ScryptMemory&) = delete;
        ScryptMemory& operator=(const ScryptMemory&) = delete;

        ~ScryptMemory() {
            secure_zero(words, count * sizeof(uint32_t));
            ::operator delete(words, count * sizeof(uint32_t), std::align_val_t(64));
        }
    };
}
