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
        avif,   // AVIF: read through the system's codec, as HEIF (heif.h)
        bmp,
        tiff,
        ico,    // ICO and CUR
        qoi,
        pnm,    // PBM, PGM, PPM and PAM
        jxl     // JPEG XL: read through the system's codec, as HEIF (jxl.h)
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
    //         heim, heis, hevm or hevs, or, under any other major brand
    //         (mif1, msf1, miaf, isom...), whose first compatible brand of
    //         HEIF or AVIF is one
    //   AVIF  the same with the brand avif or avis
    //   BMP   "BM", then at byte 14 a DIB header's size (12, 40, 52, 56,
    //         64, 108 or 124): 18 bytes
    //   TIFF  "II" 42 0 or "MM" 0 42
    //   ICO   0 0 1 0 (CUR: 0 0 2 0), a count not zero, the first entry's
    //         reserved byte 0: 10 bytes
    //   QOI   "qoif"
    //   PNM   'P', a digit 1 to 7, whitespace
    //   JXL   FF 0A (a bare codestream), or the container's signature box
    //         00 00 00 0C 'JXL ' 0D 0A 87 0A
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
        if (n >= 18 && p[0] == 'B' && p[1] == 'M') {
            const uint32_t dib = uint32_t(p[14]) | uint32_t(p[15]) << 8 | uint32_t(p[16]) << 16 | uint32_t(p[17]) << 24;
            if (dib == 12 || dib == 40 || dib == 52 || dib == 56 || dib == 64 || dib == 108 || dib == 124) {
                return format::bmp;
            }
        }
        if (n >= 4 && (std::memcmp(p, "II\x2a\0", 4) == 0 || std::memcmp(p, "MM\0\x2a", 4) == 0)) {
            return format::tiff;
        }
        if (n >= 10 && p[0] == 0 && p[1] == 0 && (p[2] == 1 || p[2] == 2) && p[3] == 0 && (p[4] | p[5]) != 0 && p[9] == 0) {
            return format::ico;
        }
        if (n >= 4 && std::memcmp(p, "qoif", 4) == 0) {
            return format::qoi;
        }
        if (n >= 2 && p[0] == 0xFF && p[1] == 0x0A) {
            return format::jxl;
        }
        if (n >= 12 && std::memcmp(p, "\0\0\0\x0cJXL \r\n\x87\n", 12) == 0) {
            return format::jxl;
        }
        if (n >= 3 && p[0] == 'P' && p[1] >= '1' && p[1] <= '7' &&
            (p[2] == ' ' || p[2] == '\t' || p[2] == '\n' || p[2] == '\r' || p[2] == '\v' || p[2] == '\f')) {
            return format::pnm;
        }
        if (n >= 12 && std::memcmp(p + 4, "ftyp", 4) == 0) {
            if (int k = detail::isobmff_brand(p + 8)) {
                return k == 1 ? format::heif : format::avif;
            }
            // the compatible brands, after the minor version, within the
            // box and the bytes given
            // (within the first SniffBytes, as a stream's head holds them: a
            // file sniffs the same from memory and from a stream)
            const size_t box = size_t(p[0]) << 24 | size_t(p[1]) << 16 | size_t(p[2]) << 8 | size_t(p[3]);
            const size_t end = std::min({box, n, detail::SniffBytes});
            for (size_t at = 16; at + 4 <= end; at += 4) {
                if (int k = detail::isobmff_brand(p + at)) {
                    return k == 1 ? format::heif : format::avif;
                }
            }
        }
        return nullopt;
    }
}
