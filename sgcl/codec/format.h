//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/slice.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace sgcl::codec {
    // The file formats the module reads
    enum class format : uint8_t {
        png,
        jpeg,
        gif,
        webp,
        heif,   // HEIC and HEIF: read and written through the system's codec (heif.h)
        avif    // AVIF: read through the system's codec, as HEIF (heif.h)
    };

    namespace detail {
        // The bytes sniff may need: 12 for most formats, the ftyp box of
        // HEIF and AVIF when their brand is among the compatible ones
        inline constexpr size_t SniffBytes = 64;

        // 1 for a brand of HEIF with HEVC, 2 for one of AVIF, 0 otherwise
        inline int isobmff_brand(const unsigned char* b) noexcept {
            for (const char* heif : {"heic", "heix", "hevc", "hevx", "heim", "heis", "hevm", "hevs"}) {
                if (std::memcmp(b, heif, 4) == 0) {
                    return 1;
                }
            }
            if (std::memcmp(b, "avif", 4) == 0 || std::memcmp(b, "avis", 4) == 0) {
                return 2;
            }
            return 0;
        }
    }

    // The format of a file by its signature, from its first bytes (12 are
    // enough for each but a HEIF or AVIF file whose major brand is a
    // general one: its ftyp box, up to 64); nullopt for anything else, or
    // for too few bytes to tell. Says what the file claims to be: a file
    // with a PNG signature and a broken body is png here, and an error of
    // decode.
    //   PNG   89 50 4E 47 0D 0A 1A 0A
    //   JPEG  FF D8 FF (SOI and the first byte of the next marker)
    //   GIF   "GIF87a" or "GIF89a"
    //   WebP  "RIFF", 4 bytes of size, "WEBP"
    //   HEIF  an ftyp box first whose major brand is heic, heix, hevc, hevx,
    //         heim, heis, hevm or hevs, or (a major brand mif1, msf1,
    //         miaf) whose first compatible brand of HEIF or AVIF is one
    //   AVIF  the same with the brand avif or avis
    inline optional<format> sniff(const slice<const byte>& head) noexcept {
        const auto* p = reinterpret_cast<const unsigned char*>(head.data());
        const size_t n = head.size();
        if (n >= 8 && std::memcmp(p, "\x89PNG\r\n\x1a\n", 8) == 0) {
            return format::png;
        }
        if (n >= 3 && p[0] == 0xFF && p[1] == 0xD8 && p[2] == 0xFF) {
            return format::jpeg;
        }
        if (n >= 6 && (std::memcmp(p, "GIF87a", 6) == 0 || std::memcmp(p, "GIF89a", 6) == 0)) {
            return format::gif;
        }
        if (n >= 12 && std::memcmp(p, "RIFF", 4) == 0 && std::memcmp(p + 8, "WEBP", 4) == 0) {
            return format::webp;
        }
        if (n >= 12 && std::memcmp(p + 4, "ftyp", 4) == 0) {
            if (int k = detail::isobmff_brand(p + 8)) {
                return k == 1 ? format::heif : format::avif;
            }
            // the compatible brands, after the minor version, within the
            // box and the bytes given
            const size_t box = size_t(p[0]) << 24 | size_t(p[1]) << 16 | size_t(p[2]) << 8 | size_t(p[3]);
            const size_t end = std::min(box, n);
            for (size_t at = 16; at + 4 <= end; at += 4) {
                if (int k = detail::isobmff_brand(p + at)) {
                    return k == 1 ? format::heif : format::avif;
                }
            }
        }
        return nullopt;
    }
}
