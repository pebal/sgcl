//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/array.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sgcl::hash::detail {
    // Eight bytes as a little-endian word, from any address: a memcpy of a
    // known length is one load; a big-endian machine, where the result of a
    // hash must not change, puts the word together byte by byte
    inline uint64_t load_le64(const unsigned char* p) noexcept {
        uint64_t w;
        if constexpr (std::endian::native == std::endian::little) {
            std::memcpy(&w, p, 8);
        } else {
            w = 0;
            for (int i = 7; i >= 0; --i) {
                w = w << 8 | p[i];
            }
        }
        return w;
    }

    // Four bytes as a little-endian word, the same way
    inline uint32_t load_le32(const unsigned char* p) noexcept {
        uint32_t w;
        if constexpr (std::endian::native == std::endian::little) {
            std::memcpy(&w, p, 4);
        } else {
            w = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
        }
        return w;
    }

    // A word's bytes in the other order: written out, which compilers turn
    // into the one instruction (rev, bswap) and MSVC compiles as it is
    inline uint64_t swap64(uint64_t v) noexcept {
        v = (v & 0x00ff00ff00ff00ffull) << 8 | (v >> 8 & 0x00ff00ff00ff00ffull);
        v = (v & 0x0000ffff0000ffffull) << 16 | (v >> 16 & 0x0000ffff0000ffffull);
        return v << 32 | v >> 32;
    }

    inline uint32_t swap32(uint32_t v) noexcept {
        v = (v & 0x00ff00ffu) << 8 | (v >> 8 & 0x00ff00ffu);
        return v << 16 | v >> 16;
    }

    inline const unsigned char* bytes(const byte* p) noexcept {
        return reinterpret_cast<const unsigned char*>(p);
    }

    // The low N bytes of v, most significant first: a digest as Go's Sum
    // writes it
    template<size_t N>
    array<byte, N> big_endian(uint64_t v) noexcept {
        static_assert(N <= 8);
        array<byte, N> out;
        for (size_t i = 0; i < N; ++i) {
            out[i] = byte(v >> (8 * (N - 1 - i)));
        }
        return out;
    }
}
