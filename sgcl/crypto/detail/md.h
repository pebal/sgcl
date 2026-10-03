//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "../../core/detail/bytes.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sgcl::crypto::detail {
    // The Merkle–Damgård construction of SHA-1 and SHA-2 (FIPS 180-4 §5):
    // the chaining words, a buffer of one block for what did not fill a
    // block yet, and the length so far. Traits give the word, the number of
    // words, the block size, the size of the length field (8 bytes, 16 for
    // SHA-384 and SHA-512) and compress(state, blocks, count), which takes
    // whole blocks. Whole blocks of an update go to compress straight from
    // the caller's memory; only the ends pass through the buffer.
    //
    // The length is counted in bytes in 64 bits: 2^64 bytes, past anything
    // a program hashes; the length field of SHA-512 takes its top three
    // bits in its upper half.
    template<class Traits>
    struct MdStream {
        using word = typename Traits::word;
        static constexpr size_t words = Traits::words;
        static constexpr size_t block = Traits::block;

        word state[words];
        unsigned char buffer[block];
        uint64_t length;
        uint32_t used;

        void init(const word* iv) noexcept {
            std::memcpy(state, iv, sizeof state);
            length = 0;
            used = 0;
        }

        void update(const unsigned char* p, size_t n) noexcept {
            // No bytes, nothing to do (an empty slice's data() is null)
            if (n == 0) {
                return;
            }
            length += n;
            if (used != 0) {
                size_t take = std::min(n, block - used);
                sgcl::detail::copy_bytes(buffer + used, p, take);
                used += uint32_t(take);
                p += take;
                n -= take;
                if (used < block) {
                    return;
                }
                Traits::compress(state, buffer, 1);
                used = 0;
            }
            if (n >= block) {
                Traits::compress(state, p, n / block);
                p += n / block * block;
                n %= block;
            }
            if (n != 0) {
                sgcl::detail::copy_bytes(buffer, p, n);
                used = uint32_t(n);
            }
        }

        // The padding (a 1 bit, zeros, the length in bits big-endian) and
        // the last one or two blocks, in place: the stream is used up. The
        // words go to out big-endian, all of them; a truncated digest
        // (SHA-224, SHA-384, SHA-512/256) takes its first bytes.
        //
        // The two zero runs stay libc's memset (an exception to DESIGN
        // 384): fill_bytes here, inlined right before the compression
        // reads the block back, took SHA-256 of 32 bytes from 31.5 ns to
        // 37.8 at the same code alignment (2026-10-03)
        void finish(unsigned char* out) noexcept {
            constexpr size_t field = Traits::length_bytes;
            uint64_t bits = length << 3;
            buffer[used++] = 0x80;
            if (used > block - field) {
                std::memset(buffer + used, 0, block - used);
                Traits::compress(state, buffer, 1);
                used = 0;
            }
            std::memset(buffer + used, 0, block - 8 - used);
            if constexpr (field == 16) {
                buffer[block - 9] = static_cast<unsigned char>(length >> 61);
            }
            store_be64(buffer + block - 8, bits);
            Traits::compress(state, buffer, 1);
            for (size_t i = 0; i < words; ++i) {
                if constexpr (sizeof(word) == 4) {
                    store_be32(out + 4 * i, state[i]);
                } else {
                    store_be64(out + 8 * i, state[i]);
                }
            }
        }
    };
}
