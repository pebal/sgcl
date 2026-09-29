//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// PNG files made by the tests, chunk by chunk, for what the corpus has no
// file of: a chunk out of place, a bad Adler-32, metadata, a palette index
// past the palette, a file of any height for the allocation probe.
#pragma once

#include "common.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace codec_test {
    inline std::string be32(uint32_t v) {
        return std::string{char(v >> 24), char(v >> 16), char(v >> 8), char(v)};
    }

    inline std::string png_chunk(const std::string& type, const std::string& data) {
        sgcl::hash::crc32 c;
        c.update(bytes(type));
        c.update(bytes(data));
        return be32(uint32_t(data.size())) + type + data + be32(c.value());
    }

    inline std::string png_signature() {
        return std::string("\x89PNG\r\n\x1a\n", 8);
    }

    inline std::string png_ihdr(uint32_t w, uint32_t h, int depth, int color, int interlace = 0) {
        return png_chunk("IHDR", be32(w) + be32(h) + std::string{char(depth), char(color), 0, 0, char(interlace)});
    }

    inline std::string zlib_of(const std::string& raw) {
        auto z = sgcl::compress::zlib::compress(bytes(raw));
        return std::string(reinterpret_cast<const char*>(z.data()), z.size());
    }

    // A whole file: IHDR, the chunks given, the filtered rows (their
    // filter bytes in them) in one IDAT, IEND
    inline std::string png_file(uint32_t w, uint32_t h, int depth, int color, const std::string& rows, const std::string& before = "", int interlace = 0) {
        return png_signature() + png_ihdr(w, h, depth, color, interlace) + before + png_chunk("IDAT", zlib_of(rows)) + png_chunk("IEND", "");
    }

    // The filtered rows of a random image (w × h, `row` bytes a row), each
    // row's filter the next of the five, random bytes after it
    inline std::string random_rows(uint32_t h, size_t row, uint32_t seed) {
        std::mt19937 rng(seed);
        std::string out;
        for (uint32_t y = 0; y < h; ++y) {
            out += char(y % 5);
            for (size_t i = 0; i < row; ++i) {
                out += char(rng());
            }
        }
        return out;
    }

    // The same for an interlaced image of `bits` bits a pixel: the seven
    // passes of Adam7 one after another, a pass of no pixels no rows
    inline std::string random_interlaced_rows(uint32_t w, uint32_t h, unsigned bits, uint32_t seed) {
        static constexpr uint32_t X0[7] = {0, 4, 0, 2, 0, 1, 0}, Y0[7] = {0, 0, 4, 0, 2, 0, 1};
        static constexpr uint32_t DX[7] = {8, 8, 4, 4, 2, 2, 1}, DY[7] = {8, 8, 8, 4, 4, 2, 2};
        std::string out;
        for (int p = 0; p < 7; ++p) {
            const uint32_t pw = w > X0[p] ? (w - X0[p] + DX[p] - 1) / DX[p] : 0;
            const uint32_t ph = h > Y0[p] ? (h - Y0[p] + DY[p] - 1) / DY[p] : 0;
            if (pw && ph) {
                out += random_rows(ph, (size_t(pw) * bits + 7) / 8, seed + p);
            }
        }
        return out;
    }
}
