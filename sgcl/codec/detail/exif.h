//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/os.h"

#include <cstddef>
#include <cstdint>

namespace sgcl::codec::detail {
    // The orientation of an EXIF block (CIPA DC-008): a TIFF structure from
    // its byte-order mark ("II" little-endian, "MM" big-endian, then 42 and
    // the offset of IFD0), tag 0x0112 of IFD0, a SHORT of 1 to 8. Anything
    // else, and a block that is not whole, is 1: the image as stored. The
    // one field the module reads and writes; the rest of the block is the
    // program's.

    // Where the block keeps the orientation: the offset of the value of
    // the first entry of tag 0x0112 in IFD0, a SHORT of one value or more
    // (held in the entry's first two bytes of value), and the byte order;
    // n when the block has no such entry or is not whole
    inline size_t exif_orientation_at(const uint8_t* p, size_t n, bool& little) noexcept {
        if (n < 8) {
            return n;
        }
        if (p[0] == 'I' && p[1] == 'I') {
            little = true;
        } else if (p[0] == 'M' && p[1] == 'M') {
            little = false;
        } else {
            return n;
        }
        auto u16 = [&](size_t at) -> uint32_t {
            return little ? p[at] | p[at + 1] << 8 : p[at] << 8 | p[at + 1];
        };
        auto u32 = [&](size_t at) -> uint32_t {
            return little ? u16(at) | u16(at + 2) << 16 : u16(at) << 16 | u16(at + 2);
        };
        if (u16(2) != 42) {
            return n;
        }
        const size_t ifd = u32(4);
        if (ifd > n - 2) {
            return n;
        }
        const size_t count = u16(ifd);
        for (size_t i = 0; i < count; ++i) {
            const size_t e = ifd + 2 + 12 * i;
            if (e > n - 12) {
                return n;
            }
            if (u16(e) == 0x0112) {
                return u16(e + 2) == 3 && u32(e + 4) >= 1 ? e + 8 : n;
            }
        }
        return n;
    }

    SGCL_INLINE_HOT unsigned exif_orientation(const uint8_t* p, size_t n) noexcept {
        bool little = false;
        const size_t at = exif_orientation_at(p, n, little);
        if (at == n) {
            return 1;
        }
        const unsigned v = little ? p[at] | p[at + 1] << 8 : p[at] << 8 | p[at + 1];
        return v >= 1 && v <= 8 ? v : 1;
    }

    // The orientation written into the block's entry, in its byte order;
    // false (nothing written) when the block has none
    inline bool set_exif_orientation(uint8_t* p, size_t n, unsigned v) noexcept {
        bool little = false;
        const size_t at = exif_orientation_at(p, n, little);
        if (at == n) {
            return false;
        }
        p[at + (little ? 0 : 1)] = uint8_t(v);
        p[at + (little ? 1 : 0)] = uint8_t(v >> 8);
        return true;
    }
}
