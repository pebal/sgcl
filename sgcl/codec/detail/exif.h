//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>

namespace sgcl::codec::detail {
    // The orientation of an EXIF block (CIPA DC-008): a TIFF structure from
    // its byte-order mark ("II" little-endian, "MM" big-endian, then 42 and
    // the offset of IFD0), tag 0x0112 of IFD0, a SHORT of 1 to 8. Anything
    // else, and a block that is not whole, is 1: the image as stored. The
    // one field the module reads; the block itself is the program's.
    inline unsigned exif_orientation(const uint8_t* p, size_t n) noexcept {
        if (n < 8) {
            return 1;
        }
        bool little;
        if (p[0] == 'I' && p[1] == 'I') {
            little = true;
        } else if (p[0] == 'M' && p[1] == 'M') {
            little = false;
        } else {
            return 1;
        }
        auto u16 = [&](size_t at) -> uint32_t {
            return little ? p[at] | p[at + 1] << 8 : p[at] << 8 | p[at + 1];
        };
        auto u32 = [&](size_t at) -> uint32_t {
            return little ? u16(at) | u16(at + 2) << 16 : u16(at) << 16 | u16(at + 2);
        };
        if (u16(2) != 42) {
            return 1;
        }
        const size_t ifd = u32(4);
        if (ifd > n - 2) {
            return 1;
        }
        const size_t count = u16(ifd);
        for (size_t i = 0; i < count; ++i) {
            const size_t e = ifd + 2 + 12 * i;
            if (e > n - 12) {
                return 1;
            }
            if (u16(e) == 0x0112) {
                // SHORT, one value, held in the entry's first two bytes of value
                if (u16(e + 2) != 3 || u32(e + 4) < 1) {
                    return 1;
                }
                const unsigned v = u16(e + 8);
                return v >= 1 && v <= 8 ? v : 1;
            }
        }
        return 1;
    }
}
