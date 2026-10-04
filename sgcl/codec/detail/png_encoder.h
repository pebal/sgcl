//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "output.h"
#include "pixels.h"
#include "png_filter.h"
#include "../error.h"
#include "../image.h"
#include "../../compress/detail/deflate.h"
#include "../../compress/level.h"
#include "../../compress/zlib.h"
#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"
#include "../../core/slice.h"
#include "../../core/vector.h"
#include "../../hash/adler32.h"
#include "../../hash/crc32.h"
#include "../../io/stream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace sgcl::codec::detail {
    // PNG's four-byte numbers, a chunk's length and a side of IHDR, stop
    // at 2^31 - 1
    inline constexpr size_t PngChunkMax = 0x7FFFFFFFu;

    // The PNG encoder (PNG 3rd ed.): an image of any pixel format as the
    // PNG type that holds it, 8 or 16 bits a channel (gray, gray with
    // alpha, truecolor, truecolor with alpha; cmyk8 as truecolor, through
    // the conversion of image::convert), never interlaced, never with a
    // palette. Each row filtered by the one of the five filters whose
    // output has the least sum of absolute values, its bytes taken as
    // signed (the heuristic PNG's specification recommends and libpng
    // uses; level 0 stores, so filters none); the rows through one zlib
    // stream of the level asked (compress's Deflater), its output cut
    // into IDAT chunks of 64 KB. The EXIF block and the ICC profile of
    // the image go into eXIf and iCCP. Memory: the Deflater's, the rows
    // (the row above, the row as PNG has it, the two filtered candidates),
    // an output buffer; made once, none per row.
    template<class Sink>
    class PngEncoder {
    public:
        // most: the largest side and chunk, PngChunkMax but in the tests,
        // which hold the bounds at small values
        SGCL_INLINE_HOT PngEncoder(const image& im, int level, Sink& sink, size_t most = PngChunkMax) noexcept
        : _im(im), _level(level), _sink(sink), _most(most) {
        }

        // false when the sink failed, or for an image PNG cannot hold (a
        // side past 2^31 - 1, IHDR's limit), refused before anything is
        // written: the error in the sink's failure. Metadata a chunk cannot
        // hold (an EXIF block or a compressed profile past 2^31 - 1 bytes)
        // is left out, as JPEG leaves out what a segment does not hold
        bool run() noexcept(NothrowSink<Sink>) {
            const auto& s = ImageAccess::state(_im);
            if (s.width > _most || s.height > _most) {
                _sink.failure = error(errc::invalid_argument, 0, "png: a side past 2^31 - 1 pixels, more than IHDR holds");
                return false;
            }
            _format = s.format;
            uint8_t color, depth;
            switch (_format) {
                case pixel_format::gray8: color = 0; depth = 8; break;
                case pixel_format::gray_alpha8: color = 4; depth = 8; break;
                case pixel_format::rgb8: color = 2; depth = 8; break;
                case pixel_format::rgba8: color = 6; depth = 8; break;
                case pixel_format::gray16: color = 0; depth = 16; break;
                case pixel_format::gray_alpha16: color = 4; depth = 16; break;
                case pixel_format::rgb16: color = 2; depth = 16; break;
                case pixel_format::rgba16: color = 6; depth = 16; break;
                default: color = 2; depth = 8; break;   // cmyk8: truecolor
            }
            const uint32_t w = s.width, h = s.height;
            _bpp = (color == 0 ? 1 : color == 4 ? 2 : color == 2 ? 3 : 4) * (depth / 8);
            _rowbytes = size_t(w) * _bpp;

            if (!_put(reinterpret_cast<const uint8_t*>("\x89PNG\r\n\x1a\n"), 8)) {
                return false;
            }
            uint8_t ihdr[13];
            be32(ihdr, w);
            be32(ihdr + 4, h);
            ihdr[8] = depth;
            ihdr[9] = color;
            ihdr[10] = ihdr[11] = ihdr[12] = 0;
            if (!_chunk("IHDR", ihdr, 13)) {
                return false;
            }
            if (!s.icc.empty()) {
                // a name (the one libpng writes by default), its 0, method 0, the profile in zlib
                auto z = compress::zlib::compress(std::as_const(s.icc).as_slice(), {.level = compress::level(_level)});
                std::vector<uint8_t> body;
                body.reserve(13 + z.size());
                const char name[] = "ICC Profile";
                body.insert(body.end(), name, name + sizeof(name));   // with its 0
                body.push_back(0);
                const auto* zp = reinterpret_cast<const uint8_t*>(z.data());
                body.insert(body.end(), zp, zp + z.size());
                if (body.size() <= _most && !_chunk("iCCP", body.data(), body.size())) {
                    return false;
                }
            }
            if (!s.exif.empty() && s.exif.size() <= _most) {
                if (!_chunk("eXIf", reinterpret_cast<const uint8_t*>(s.exif.data()), s.exif.size())) {
                    return false;
                }
            }

            // The memory of the encoding: the rows, then the zlib stream's
            const size_t row = _rowbytes + 1;   // with its filter byte
            _memory.reset(new uint8_t[5 * row]);
            const uint8_t* zero = _memory.get();
            uint8_t* prior = _memory.get() + row;
            uint8_t* raw = prior + row;
            _best = raw + row;
            _trial = _best + row;
            // libc's memset, not fill_bytes: three rows may be megabytes,
            // and a large zero fill is libc's (whole cache lines, DESIGN 393)
            std::memset(_memory.get(), 0, 3 * row);
            // filtered rows: zlib's Z_FILTERED, as libpng has it whenever it filters
            _deflater = std::make_unique<compress::detail::Deflater>(_level, _level != 0);
            _z.reserve(OutputReserve);
            const uint8_t cmf = 0x78;
            const uint8_t hint = _level == 0 || _level == 1 || _level == compress::level::huffman_only ? 0 : _level < 6 ? 1 : _level == 6 ? 2 : 3;
            uint8_t flg = uint8_t(hint << 6);
            flg = uint8_t(flg + (31 - (cmf * 256 + flg) % 31) % 31);
            _z.push_back(cmf);
            _z.push_back(flg);

            const uint8_t* pixels = reinterpret_cast<const uint8_t*>(s.pixels.data());
            const ConvertRow to_rgb = _format == pixel_format::cmyk8 ? converter(pixel_format::cmyk8, pixel_format::rgb8) : nullptr;
            for (uint32_t y = 0; y < h; ++y) {
                const uint8_t* src = pixels + size_t(y) * s.stride;
                const uint8_t* cur;
                if (depth == 16) {
                    // the machine's order to PNG's, big-endian
                    for (size_t i = 0; i < _rowbytes; i += 2) {
                        uint16_t v;
                        std::memcpy(&v, src + i, 2);
                        raw[1 + i] = uint8_t(v >> 8);
                        raw[1 + i + 1] = uint8_t(v);
                    }
                    cur = raw + 1;
                } else if (to_rgb) {
                    to_rgb(reinterpret_cast<const std::byte*>(src), reinterpret_cast<std::byte*>(raw + 1), w);
                    cur = raw + 1;
                } else {
                    cur = src;
                }
                const bool converted = depth == 16 || to_rgb;
                const uint8_t* up = y == 0 ? zero + 1 : converted ? prior + 1 : src - s.stride;
                const uint8_t* filtered = _filter(cur, up);
                if (!_deflate(filtered, row)) {
                    return false;
                }
                if (converted) {
                    std::swap(prior, raw);   // this row is the next one's row above
                }
            }
            _deflater->finish(_z);
            uint8_t adler[4];
            be32(adler, _adler.value());
            _z.insert(_z.end(), adler, adler + 4);
            if (!_chunk("IDAT", _z.data(), _z.size())) {
                return false;
            }
            return _chunk("IEND", nullptr, 0);
        }

    private:
        static constexpr size_t IdatSize = 65536;
        static constexpr size_t OutputReserve = 262144;   // past the largest block the Deflater writes at once

        SGCL_INLINE_HOT static void be32(uint8_t* p, uint32_t v) noexcept {
            p[0] = uint8_t(v >> 24);
            p[1] = uint8_t(v >> 16);
            p[2] = uint8_t(v >> 8);
            p[3] = uint8_t(v);
        }

        SGCL_INLINE_HOT bool _put(const uint8_t* p, size_t n) noexcept(NothrowSink<Sink>) {
            return _sink.put(p, n);
        }

        bool _chunk(const char* type, const uint8_t* data, size_t n) noexcept(NothrowSink<Sink>) {
            uint8_t head[8];
            be32(head, uint32_t(n));
            std::memcpy(head + 4, type, 4);
            hash::crc32 c;
            c.update(slice<const byte>(reinterpret_cast<const byte*>(head + 4), 4));
            if (n) {
                c.update(slice<const byte>(reinterpret_cast<const byte*>(data), n));
            }
            uint8_t tail[4];
            be32(tail, c.value());
            return _put(head, 8) && (n == 0 || _put(data, n)) && _put(tail, 4);
        }

        // A filtered row (its filter byte first) through the zlib stream,
        // the full IDAT chunks it completes out
        bool _deflate(const uint8_t* filtered, size_t n) noexcept(NothrowSink<Sink>) {
            _adler.update(slice<const byte>(reinterpret_cast<const byte*>(filtered), n));
            _deflater->write(filtered, n, _z);
            while (_z.size() >= IdatSize) {
                if (!_chunk("IDAT", _z.data(), IdatSize)) {
                    return false;
                }
                _z.erase(_z.begin(), _z.begin() + IdatSize);
            }
            return true;
        }

        // The sum the heuristic minimizes: each byte as a signed value,
        // its magnitude
        static uint64_t _score(const uint8_t* p, size_t n) noexcept {
            uint64_t sum = 0;
            for (size_t i = 0; i < n; ++i) {
                const unsigned v = p[i];
                sum += v < 128 ? v : 256 - v;
            }
            return sum;
        }

        // The row with the filter of least score, into _best (its filter
        // byte at [0]); ties to the earlier filter, None first
        const uint8_t* _filter(const uint8_t* cur, const uint8_t* up) noexcept {

            const size_t n = _rowbytes;
            _best[0] = FilterNone;
            sgcl::detail::copy_bytes(_best + 1, cur, n);
            if (_level == 0) {
                return _best;
            }
            uint64_t best = _score(_best + 1, n);
            for (uint8_t f = FilterSub; f <= FilterPaeth; ++f) {
                _trial[0] = f;
                filter(f, cur, up, _trial + 1, n, _bpp);
                const uint64_t score = _score(_trial + 1, n);
                if (score < best) {
                    best = score;
                    std::swap(_best, _trial);
                }
            }
            return _best;
        }

        const image& _im;
        int _level;
        Sink& _sink;
        size_t _most;
        pixel_format _format = pixel_format::rgba8;
        unsigned _bpp = 1;
        size_t _rowbytes = 0;
        std::unique_ptr<uint8_t[]> _memory;
        uint8_t* _best = nullptr;
        uint8_t* _trial = nullptr;
        std::unique_ptr<compress::detail::Deflater> _deflater;
        std::vector<uint8_t> _z;
        hash::adler32 _adler;
    };
}
