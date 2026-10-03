//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sgcl::codec::detail {
    // The zig-zag order of T.81 (Figure A.6): the index in the block, row
    // by row, of the k-th coefficient of the stream
    inline constexpr uint8_t ZigZag[64] = {
        0, 1, 8, 16, 9, 2, 3, 10,
        17, 24, 32, 25, 18, 11, 4, 5,
        12, 19, 26, 33, 40, 48, 41, 34,
        27, 20, 13, 6, 7, 14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36,
        29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46,
        53, 60, 61, 54, 47, 55, 62, 63
    };

    // A Huffman table of a DHT segment (T.81, B.2.4.2 and Annex C): the
    // count of codes of each length 1..16 and the symbols in the order of
    // their codes, the codes themselves canonical. Decoded (F.2.2.3) by
    // the largest code of each length and the index of its first symbol;
    // a table of the next 9 bits gives the symbol and the length of every
    // code that short in one lookup.
    struct HuffmanTable {
        static constexpr unsigned FastBits = 9;

        // An AC table's coefficient whole in the next FastBits bits: the
        // code and its value's bits both there, the value as EXTEND makes
        // it (F.2.2.1), the run of zeros before it, the bits they take;
        // length 0 where not (a longer code, a longer value, EOB, ZRL)
        struct AcFast {
            int16_t value;
            uint8_t run;
            uint8_t length;
        };

        bool defined = false;
        uint16_t fast[1u << FastBits];   // (length << 8) | symbol; 0: a longer code
        AcFast ac_fast[1u << FastBits];
        int32_t maxcode[18];             // the largest code of each length, -1 for none
        int32_t offset[17];              // symbol index = code + offset[length]
        uint8_t symbols[256];

        // False for a table no code can be made of: more than 256 symbols,
        // lengths that overflow their bits, a DC symbol past 15
        bool build(const uint8_t* counts, const uint8_t* values, bool dc) noexcept {
            unsigned total = 0;
            for (int l = 0; l < 16; ++l) {
                total += counts[l];
            }
            if (total > 256) {
                return false;
            }
            sgcl::detail::copy_bytes(symbols, values, total);
            if (dc) {
                for (unsigned i = 0; i < total; ++i) {
                    if (symbols[i] > 15) {
                        return false;
                    }
                }
            }
            std::memset(fast, 0, sizeof(fast));
            int32_t code = 0;
            unsigned k = 0;
            for (int l = 1; l <= 16; ++l) {
                const unsigned n = counts[l - 1];
                offset[l] = int32_t(k) - code;
                for (unsigned i = 0; i < n; ++i, ++k, ++code) {
                    // each code in its l bits, and never all ones (C.2)
                    if (code + 1 >= (int32_t(1) << l)) {
                        return false;
                    }
                    if (unsigned(l) <= FastBits) {
                        const unsigned shift = FastBits - l;
                        const unsigned first = unsigned(code) << shift;
                        for (unsigned j = 0; j < (1u << shift); ++j) {
                            fast[first + j] = uint16_t(l << 8 | symbols[k]);
                        }
                    }
                }
                maxcode[l] = n ? code - 1 : -1;
                code <<= 1;
            }
            maxcode[17] = 0x7FFFFFFF;
            for (unsigned look = 0; look < (1u << FastBits); ++look) {
                ac_fast[look] = AcFast{0, 0, 0};
                const unsigned e = fast[look];
                const unsigned l = e >> 8, s = e & 15, r = (e >> 4) & 15;
                if (dc || !e || s == 0 || l + s > FastBits) {
                    continue;
                }
                const int v = int(look >> (FastBits - l - s)) & ((1 << s) - 1);
                ac_fast[look] = AcFast{int16_t(v < (1 << (s - 1)) ? v - (1 << s) + 1 : v), uint8_t(r), uint8_t(l + s)};
            }
            defined = true;
            return true;
        }
    };

    // The typical tables of T.81 Annex K.3 (Tables K.3 to K.6), as the
    // counts of each code length and the symbols in the order of their
    // codes: what a scan takes for table 0 (luminance) or 1 (chrominance)
    // when the file defines none, as a Motion-JPEG frame does not (and as
    // libjpeg-turbo does). Data of the norm, the codes it prints for each
    // symbol regenerated from these (C.2) and checked equal to them.
    namespace annex_k {
        inline constexpr uint8_t DcLuminanceBits[16] = {0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
        inline constexpr uint8_t DcChrominanceBits[16] = {0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
        inline constexpr uint8_t DcValues[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

        inline constexpr uint8_t AcLuminanceBits[16] = {0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125};
        inline constexpr uint8_t AcLuminanceValues[162] = {
            0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
            0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
            0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
            0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
            0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
            0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
            0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
            0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
            0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
            0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
            0xf9, 0xfa
        };

        inline constexpr uint8_t AcChrominanceBits[16] = {0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119};
        inline constexpr uint8_t AcChrominanceValues[162] = {
            0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
            0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
            0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
            0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
            0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
            0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
            0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
            0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
            0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
            0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
            0xf9, 0xfa
        };

        // The typical table of that class and number (0 or 1) into t
        inline bool build(HuffmanTable& t, bool dc, unsigned number) noexcept {
            if (dc) {
                return t.build(number == 0 ? DcLuminanceBits : DcChrominanceBits, DcValues, true);
            }
            return t.build(number == 0 ? AcLuminanceBits : AcChrominanceBits, number == 0 ? AcLuminanceValues : AcChrominanceValues, false);
        }
    }
}
