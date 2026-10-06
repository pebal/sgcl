//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "image.h"
#include "options.h"
#include "detail/input.h"
#include "detail/output.h"
#include "detail/pixels.h"
#include "../core/aliases.h"
#include "../core/detail/bytes.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/vector.h"
#include "../io/stream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace sgcl::codec {
    namespace detail {
        // A channel of a pixel by its mask: the bits under it, widened to 8
        // bits by repeating them (5 bits abcde as abcdeabc, as ffmpeg and
        // Chromium widen them); a mask of none gives `none`
        struct BmpMask {
            uint32_t mask = 0;
            unsigned shift = 0;
            uint32_t max = 0;

            SGCL_INLINE_HOT explicit BmpMask(uint32_t m = 0) noexcept
            : mask(m) {
                if (m) {
                    shift = unsigned(__builtin_ctz(m));
                    max = m >> shift;
                    // the bits a channel has: a run from the lowest set
                    while (max & (max + 1)) {
                        max |= max >> 1;   // a mask with holes is read as its whole span
                    }
                }
            }

            SGCL_INLINE_HOT uint8_t of(uint32_t px, uint8_t none) const noexcept {
                if (!mask) {
                    return none;
                }
                const uint32_t v = (px & mask) >> shift;
                if (max == 255) {
                    return uint8_t(v);
                }
                const int n = 32 - __builtin_clz(max);   // the channel's bits
                uint32_t r = 0;
                for (int at = 8 - n;; at -= n) {
                    r |= at >= 0 ? v << at : v >> -at;
                    if (at <= 0) {
                        break;
                    }
                }
                return uint8_t(r);
            }
        };

        // The BMP decoder (Microsoft's BITMAPFILEHEADER and BITMAPCOREHEADER,
        // BITMAPINFOHEADER, V2, V3, V4, V5 and OS/2's 64-byte header read as
        // the first 40 bytes of it) over an input of MemoryInput's shape:
        // 1, 2, 4 and 8 bits through a palette, 16 (5-5-5 by default), 24,
        // 32; BI_BITFIELDS and BI_ALPHABITFIELDS masks; RLE8 and RLE4; rows
        // bottom-up, or top-down for a negative height. Alpha as Chromium
        // and Go take it: from an alpha mask (V3 and later headers, or
        // ALPHABITFIELDS), and for 32 bits without masks only under a V4 or
        // V5 header; any other 32-bit pixel opaque. Pixels an RLE stream
        // skips (its delta and end-of-line codes) are transparent black.
        //
        // The image: gray8 when every palette entry is a gray, rgb8 or rgba8
        // otherwise (rgba8 where there is alpha), or decode_options.want.
        // JPEG and PNG inside a BMP (BI_JPEG, BI_PNG) are unsupported.
        template<class Input>
        class BmpDecoder {
        public:
            SGCL_INLINE_HOT BmpDecoder(Input& in, const decode_options& o, bool in_icon = false) noexcept
            : _in(in), _o(o), _icon(in_icon) {
            }

            // The file: its header first; or, for an icon's entry (in_icon),
            // the DIB header straight away with the height doubled for the
            // AND mask that follows the pixels
            expected<image, error> run() noexcept(NothrowInput<Input>) {
                if (_o.want && !valid(*_o.want)) {
                    return unexpected(error(errc::invalid_argument, 0, "bmp: decode_options.want outside the list"));
                }
                uint32_t data_offset = 0;
                if (!_icon) {
                    uint8_t f[14];
                    if (!_read(f, 14)) {
                        return unexpected(_short());
                    }
                    if (f[0] != 'B' || f[1] != 'M') {
                        return unexpected(error(errc::corrupt, 0, "bmp: not a BMP signature"));
                    }
                    data_offset = le32(f + 10);
                }
                uint8_t h[124] = {};
                if (!_read(h, 4)) {
                    return unexpected(_short());
                }
                const uint32_t hsize = le32(h);
                if (hsize != 12 && hsize != 40 && hsize != 52 && hsize != 56 && hsize != 64 && hsize != 108 && hsize != 124) {
                    return unexpected(error(errc::unsupported, _in.offset() - 4, "bmp: a DIB header of an unknown size"));
                }
                if (!_read(h + 4, hsize - 4)) {
                    return unexpected(_short());
                }
                int64_t width, height;
                unsigned planes, bpp;
                uint32_t compression = 0, colors_used = 0;
                if (hsize == 12) {
                    width = le16(h + 4);
                    height = int16_t(le16(h + 6));
                    planes = le16(h + 8);
                    bpp = le16(h + 10);
                } else {
                    width = int32_t(le32(h + 4));
                    height = int32_t(le32(h + 8));
                    planes = le16(h + 12);
                    bpp = le16(h + 14);
                    compression = hsize == 64 ? (le32(h + 16) == 3 ? 99 : le32(h + 16)) : le32(h + 16);   // OS/2's 3 is Huffman 1D
                    colors_used = le32(h + 32);
                }
                const bool top_down = height < 0;
                if (top_down) {
                    height = -height;
                }
                if (_icon) {
                    height /= 2;   // the XOR bitmap and the AND mask
                }
                if (planes != 1) {
                    return unexpected(error(errc::corrupt, 0, "bmp: planes other than 1"));
                }
                if (width <= 0 || height <= 0 || width > 0x7FFFFFFF || height > 0x7FFFFFFF) {
                    return unexpected(error(errc::corrupt, 0, "bmp: a side of zero or a negative width"));
                }
                if (auto e = check_size(uint32_t(width), uint32_t(height), _o.limits, 0)) {
                    return unexpected(*e);
                }
                const uint32_t w = uint32_t(width), ht = uint32_t(height);
                // the masks: in the header from V2 on, after an INFO header
                // for BITFIELDS (3) and ALPHABITFIELDS (4)
                uint32_t masks[4] = {0, 0, 0, 0};
                const bool fields = compression == 3 || compression == 6;
                if (fields && (hsize == 40 || hsize == 64)) {
                    uint8_t m[16];
                    const size_t n = compression == 6 ? 16 : 12;
                    if (!_read(m, n)) {
                        return unexpected(_short());
                    }
                    for (size_t i = 0; i < n / 4; ++i) {
                        masks[i] = le32(m + 4 * i);
                    }
                } else if (hsize >= 52) {
                    for (size_t i = 0; i < (hsize >= 56 ? 4u : 3u); ++i) {
                        masks[i] = le32(h + 40 + 4 * i);
                    }
                }
                const bool rle = compression == 1 || compression == 2;
                if (compression == 4 || compression == 5) {
                    return unexpected(error(errc::unsupported, 0, "bmp: JPEG or PNG inside a BMP"));
                }
                if (compression > 6 || (compression == 1 && bpp != 8) || (compression == 2 && bpp != 4) || (fields && bpp != 16 && bpp != 32)) {
                    return unexpected(error(errc::corrupt, 0, "bmp: a compression its bit depth does not take"));
                }
                if (bpp != 1 && bpp != 2 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) {
                    return unexpected(error(errc::corrupt, 0, "bmp: a bit depth outside 1, 2, 4, 8, 16, 24, 32"));
                }
                if (rle && top_down) {
                    return unexpected(error(errc::corrupt, 0, "bmp: RLE rows top-down"));
                }
                // the palette
                uint8_t palette[256][3] = {};
                unsigned entries = 0;
                bool gray_palette = false;
                if (bpp <= 8) {
                    const unsigned most = 1u << bpp;
                    const unsigned each = hsize == 12 ? 3 : 4;
                    entries = colors_used == 0 || colors_used > most ? most : colors_used;
                    // no count given: as many as fit before the pixels (Chromium's reading of short palettes)
                    const uint64_t here = _in.offset();
                    if (colors_used == 0 && !_icon && data_offset > here) {
                        entries = unsigned(std::min<uint64_t>(entries, (data_offset - here) / each));
                    }
                    uint8_t raw[256 * 4];
                    if (!_read(raw, size_t(entries) * each)) {
                        return unexpected(_short());
                    }
                    if (colors_used > most && !_skip(uint64_t(colors_used - most) * each)) {
                        return unexpected(_short());
                    }
                    gray_palette = true;
                    for (unsigned i = 0; i < entries; ++i) {
                        palette[i][0] = raw[each * i + 2];
                        palette[i][1] = raw[each * i + 1];
                        palette[i][2] = raw[each * i];
                        gray_palette = gray_palette && palette[i][0] == palette[i][1] && palette[i][1] == palette[i][2];
                    }
                }
                // to the pixels
                if (!_icon && data_offset) {
                    if (data_offset < _in.offset()) {
                        return unexpected(error(errc::corrupt, 10, "bmp: the pixels' offset inside the headers"));
                    }
                    if (!_skip(data_offset - _in.offset())) {
                        return unexpected(_short());
                    }
                }
                // what the pixels are
                BmpMask mr, mg, mb, ma;
                bool alpha = false;
                if (bpp == 16 || bpp == 32) {
                    if (fields || (hsize >= 52 && compression == 3)) {
                        mr = BmpMask(masks[0]);
                        mg = BmpMask(masks[1]);
                        mb = BmpMask(masks[2]);
                        ma = BmpMask(masks[3]);
                    } else if (bpp == 16) {
                        mr = BmpMask(0x7C00);
                        mg = BmpMask(0x03E0);
                        mb = BmpMask(0x001F);
                    } else {
                        mr = BmpMask(0x00FF0000);
                        mg = BmpMask(0x0000FF00);
                        mb = BmpMask(0x000000FF);
                        if (hsize >= 108 || _icon) {
                            ma = BmpMask(0xFF000000);
                        }
                    }
                    alpha = ma.mask != 0;
                }
                const pixel_format native = alpha || rle ? pixel_format::rgba8 : (bpp <= 8 && gray_palette) ? pixel_format::gray8 : pixel_format::rgb8;
                _native = native;
                image out(w, ht, native);
                auto& s = ImageAccess::state(out);
                uint8_t* pixels = reinterpret_cast<uint8_t*>(s.pixels.data());
                if (rle) {
                    if (!_rle(pixels, w, ht, bpp, palette, entries)) {
                        return unexpected(_err);
                    }
                } else {
                    const size_t row_bytes = ((size_t(w) * bpp + 31) / 32) * 4;
                    std::unique_ptr<uint8_t[]> row(new uint8_t[row_bytes]);
                    for (uint32_t r = 0; r < ht; ++r) {
                        if (!_read(row.get(), row_bytes)) {
                            return unexpected(_short());
                        }
                        const uint32_t y = top_down ? r : ht - 1 - r;
                        uint8_t* d = pixels + size_t(y) * s.stride;
                        const uint8_t* p = row.get();
                        if (bpp <= 8) {
                            const unsigned per = 8 / bpp, m = (1u << bpp) - 1;
                            for (uint32_t x = 0; x < w; ++x) {
                                unsigned i = (p[x / per] >> (8 - bpp - (x % per) * bpp)) & m;
                                i = i < entries ? i : 0;   // an index past the palette: its first entry
                                if (native == pixel_format::gray8) {
                                    d[x] = palette[i][0];
                                } else {
                                    d[3 * x] = palette[i][0];
                                    d[3 * x + 1] = palette[i][1];
                                    d[3 * x + 2] = palette[i][2];
                                }
                            }
                        } else if (bpp == 24) {
                            for (uint32_t x = 0; x < w; ++x) {
                                d[3 * x] = p[3 * x + 2];
                                d[3 * x + 1] = p[3 * x + 1];
                                d[3 * x + 2] = p[3 * x];
                            }
                        } else {
                            const unsigned step = bpp / 8;
                            const unsigned out_step = alpha ? 4 : 3;
                            for (uint32_t x = 0; x < w; ++x) {
                                const uint32_t px = step == 2 ? uint32_t(p[2 * x]) | uint32_t(p[2 * x + 1]) << 8 : le32(p + 4 * x);
                                uint8_t* q = d + out_step * x;
                                q[0] = mr.of(px, 0);
                                q[1] = mg.of(px, 0);
                                q[2] = mb.of(px, 0);
                                if (alpha) {
                                    q[3] = ma.of(px, 255);
                                }
                            }
                        }
                    }
                    if (_icon) {
                        if (!_and_mask(out, w, ht, alpha)) {
                            return unexpected(_err);
                        }
                    }
                }
                if (_o.want && *_o.want != native) {
                    return out.convert(*_o.want);
                }
                return out;
            }

            // The format the pixels came in (before decode_options.want)
            SGCL_INLINE_HOT pixel_format native() const noexcept {
                return _native;
            }

        private:
            SGCL_INLINE_HOT static uint32_t le32(const uint8_t* p) noexcept {
                return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
            }

            SGCL_INLINE_HOT static uint32_t le16(const uint8_t* p) noexcept {
                return uint32_t(p[0]) | uint32_t(p[1]) << 8;
            }

            // RLE8 and RLE4 (bottom-up): runs of an index, absolute runs
            // (padded to 16 bits), end of line, end of bitmap, delta; the
            // pixels never written transparent
            bool _rle(uint8_t* px, uint32_t w, uint32_t ht, unsigned bpp, const uint8_t (*palette)[3], unsigned entries) noexcept(NothrowInput<Input>) {
                uint32_t x = 0, y = 0;   // y counted from the bottom row
                auto put = [&](unsigned index) {
                    if (x < w && y < ht) {
                        const unsigned i = index < entries ? index : 0;
                        uint8_t* q = px + (size_t(ht - 1 - y) * w + x) * 4;
                        q[0] = palette[i][0];
                        q[1] = palette[i][1];
                        q[2] = palette[i][2];
                        q[3] = 255;
                    }
                    ++x;
                };
                for (;;) {
                    uint8_t pair[2];
                    if (!_read(pair, 2)) {
                        _err = _short();
                        return false;
                    }
                    if (pair[0] > 0) {
                        for (unsigned k = 0; k < pair[0]; ++k) {
                            put(bpp == 8 ? pair[1] : (k & 1) ? pair[1] & 15 : pair[1] >> 4);
                        }
                        continue;
                    }
                    if (pair[1] == 0) {
                        x = 0;
                        if (++y >= ht) {
                            return true;   // a stream that ends with the image (no end of bitmap) is taken
                        }
                    } else if (pair[1] == 1) {
                        return true;
                    } else if (pair[1] == 2) {
                        uint8_t d[2];
                        if (!_read(d, 2)) {
                            _err = _short();
                            return false;
                        }
                        x += d[0];
                        y += d[1];
                        if (y >= ht) {
                            return true;
                        }
                    } else {
                        const unsigned n = pair[1];
                        const size_t bytes = bpp == 8 ? n : (n + 1) / 2;
                        uint8_t buf[256];
                        if (!_read(buf, bytes + (bytes & 1))) {
                            _err = _short();
                            return false;
                        }
                        for (unsigned k = 0; k < n; ++k) {
                            put(bpp == 8 ? buf[k] : (k & 1) ? buf[k / 2] & 15 : buf[k / 2] >> 4);
                        }
                    }
                }
            }

            // An icon entry's AND mask (1 bit a pixel, rows padded to 32
            // bits, bottom-up): a set bit transparent, used where the
            // pixels carry no alpha of their own (their alpha all zero, or
            // no alpha channel)
            bool _and_mask(image& out, uint32_t w, uint32_t ht, bool alpha) noexcept(NothrowInput<Input>) {
                auto& s = ImageAccess::state(out);
                uint8_t* px = reinterpret_cast<uint8_t*>(s.pixels.data());
                bool any_alpha = false;
                if (alpha) {
                    for (size_t i = 3; i < s.pixels.size(); i += 4) {
                        any_alpha |= px[i] != 0;
                    }
                }
                const size_t row_bytes = ((size_t(w) + 31) / 32) * 4;
                std::unique_ptr<uint8_t[]> row(new uint8_t[row_bytes]);
                if (alpha && any_alpha) {
                    return true;   // the mask is there, and read past by the caller's bounds
                }
                if (!alpha) {
                    // rgb or gray in: rgba with the mask
                    image rgba = out.convert(pixel_format::rgba8);
                    out = rgba;
                    _native = pixel_format::rgba8;
                }
                auto& t = ImageAccess::state(out);
                uint8_t* q = reinterpret_cast<uint8_t*>(t.pixels.data());
                for (uint32_t r = 0; r < ht; ++r) {
                    if (!_read(row.get(), row_bytes)) {
                        // a missing mask: every pixel opaque (as browsers show it)
                        for (size_t i = 3; i < t.pixels.size(); i += 4) {
                            q[i] = 255;
                        }
                        return true;
                    }
                    uint8_t* d = q + size_t(ht - 1 - r) * t.stride;
                    for (uint32_t x = 0; x < w; ++x) {
                        d[4 * x + 3] = (row[x >> 3] >> (7 - (x & 7))) & 1 ? 0 : 255;
                    }
                }
                return true;
            }

            bool _read(uint8_t* out, size_t n) noexcept(NothrowInput<Input>) {
                while (n > 0) {
                    const uint8_t* p;
                    size_t got;
                    if (!_in.peek(n, p, got)) {
                        _io = true;
                        return false;
                    }
                    if (got == 0) {
                        return false;
                    }
                    sgcl::detail::copy_bytes(out, p, got);
                    _in.consume(got);
                    out += got;
                    n -= got;
                }
                return true;
            }

            bool _skip(uint64_t n) noexcept(NothrowInput<Input>) {
                while (n > 0) {
                    const uint8_t* p;
                    size_t got;
                    if (!_in.peek(size_t(std::min<uint64_t>(n, 1u << 20)), p, got)) {
                        _io = true;
                        return false;
                    }
                    if (got == 0) {
                        return false;
                    }
                    _in.consume(got);
                    n -= got;
                }
                return true;
            }

            error _short() noexcept {
                if (_io && _in.failure) {
                    return *_in.failure;
                }
                return error(errc::unexpected_end, _in.offset(), "bmp: the data ends before the last pixel");
            }

            Input& _in;
            const decode_options& _o;
            bool _icon;
            error _err;
            bool _io = false;
            pixel_format _native = pixel_format::rgb8;
        };

        // The encoder: gray8 as 8 bits through a palette of the 256 grays,
        // an image with alpha as 32 bits under a V4 header with its masks
        // (what Go and browsers read alpha from), any other as 24 bits;
        // 16-bit channels to 8, CMYK through RGB. rows bottom-up. With
        // `icon`, the entry of an ICO file: no file header, the height
        // doubled, an AND mask after the pixels (of the transparent ones)
        template<class Sink>
        bool bmp_encode(const image& im, Sink& sink, bool icon = false) noexcept(NothrowSink<Sink>) {
            const auto& s = ImageAccess::state(im);
            const bool has_alpha = alpha(s.format) || icon;
            const bool gray8_out = s.format == pixel_format::gray8 && !icon;
            const unsigned bpp = has_alpha ? 32 : gray8_out ? 8 : 24;
            const pixel_format target = has_alpha ? pixel_format::rgba8 : gray8_out ? pixel_format::gray8 : pixel_format::rgb8;
            const uint32_t w = s.width, ht = s.height;
            const uint64_t row_bytes = ((uint64_t(w) * bpp + 31) / 32) * 4;
            const uint64_t mask_row = icon ? ((uint64_t(w) + 31) / 32) * 4 : 0;
            const uint32_t header = icon ? 40 : has_alpha ? 108 : 40;
            const uint32_t palette = gray8_out ? 1024 : 0;
            const uint64_t data = (row_bytes + mask_row) * ht;
            const uint64_t file = (icon ? 0 : 14) + header + palette + data;
            if (w > 0x7FFFFFFF || ht > (icon ? 0x3FFFFFFFu : 0x7FFFFFFFu) || file > 0xFFFFFFFFu) {
                sink.failure = error(errc::invalid_argument, 0, "bmp: an image past what BMP's 32-bit sizes hold");
                return false;
            }
            uint8_t h[14 + 108] = {};
            auto le16 = [](uint8_t* p, uint32_t v) {
                p[0] = uint8_t(v);
                p[1] = uint8_t(v >> 8);
            };
            auto le32 = [](uint8_t* p, uint32_t v) {
                p[0] = uint8_t(v);
                p[1] = uint8_t(v >> 8);
                p[2] = uint8_t(v >> 16);
                p[3] = uint8_t(v >> 24);
            };
            uint8_t* d = h;
            if (!icon) {
                h[0] = 'B';
                h[1] = 'M';
                le32(h + 2, uint32_t(file));
                le32(h + 10, 14 + header + palette);
                d = h + 14;
            }
            le32(d, header);
            le32(d + 4, w);
            le32(d + 8, icon ? ht * 2 : ht);
            le16(d + 12, 1);
            le16(d + 14, bpp);
            le32(d + 16, has_alpha && !icon ? 3 : 0);   // BI_BITFIELDS with the masks of the V4 header
            le32(d + 20, uint32_t(data));
            le32(d + 24, 2835);   // 72 dpi
            le32(d + 28, 2835);
            le32(d + 32, gray8_out ? 256 : 0);
            if (header == 108) {
                le32(d + 40, 0x00FF0000);
                le32(d + 44, 0x0000FF00);
                le32(d + 48, 0x000000FF);
                le32(d + 52, 0xFF000000);
                std::memcpy(d + 56, "BGRs", 4);   // LCS_sRGB, its bytes as the header has them
            }
            if (!sink.put(h, (icon ? 0 : 14) + header)) {
                return false;
            }
            if (gray8_out) {
                uint8_t pal[1024];
                for (unsigned i = 0; i < 256; ++i) {
                    pal[4 * i] = pal[4 * i + 1] = pal[4 * i + 2] = uint8_t(i);
                    pal[4 * i + 3] = 0;
                }
                if (!sink.put(pal, 1024)) {
                    return false;
                }
            }
            const ConvertRow convert = s.format == target ? nullptr : converter(s.format, target);
            std::unique_ptr<uint8_t[]> row(new uint8_t[size_t(w) * 4]);
            std::unique_ptr<uint8_t[]> out(new uint8_t[size_t(row_bytes)]());
            const uint8_t* pixels = reinterpret_cast<const uint8_t*>(s.pixels.data());
            for (uint32_t r = 0; r < ht; ++r) {
                const uint32_t y = ht - 1 - r;
                const uint8_t* src = pixels + size_t(y) * s.stride;
                if (convert) {
                    convert(reinterpret_cast<const std::byte*>(src), reinterpret_cast<std::byte*>(row.get()), w);
                    src = row.get();
                }
                uint8_t* o = out.get();
                if (bpp == 8) {
                    sgcl::detail::copy_bytes(o, src, w);
                } else if (bpp == 24) {
                    for (uint32_t x = 0; x < w; ++x) {
                        o[3 * x] = src[3 * x + 2];
                        o[3 * x + 1] = src[3 * x + 1];
                        o[3 * x + 2] = src[3 * x];
                    }
                } else {
                    for (uint32_t x = 0; x < w; ++x) {
                        o[4 * x] = src[4 * x + 2];
                        o[4 * x + 1] = src[4 * x + 1];
                        o[4 * x + 2] = src[4 * x];
                        o[4 * x + 3] = src[4 * x + 3];
                    }
                }
                if (!sink.put(o, size_t(row_bytes))) {
                    return false;
                }
            }
            if (icon) {
                // the AND mask: a pixel of alpha 0 transparent
                std::unique_ptr<uint8_t[]> m(new uint8_t[size_t(mask_row)]);
                for (uint32_t r = 0; r < ht; ++r) {
                    const uint32_t y = ht - 1 - r;
                    const uint8_t* src = pixels + size_t(y) * s.stride;
                    if (convert) {
                        convert(reinterpret_cast<const std::byte*>(src), reinterpret_cast<std::byte*>(row.get()), w);
                        src = row.get();
                    }
                    std::memset(m.get(), 0, size_t(mask_row));
                    for (uint32_t x = 0; x < w; ++x) {
                        if (src[4 * x + 3] == 0) {
                            m[x >> 3] = uint8_t(m[x >> 3] | (0x80 >> (x & 7)));
                        }
                    }
                    if (!sink.put(m.get(), size_t(mask_row))) {
                        return false;
                    }
                }
            }
            return true;
        }
    }

    // BMP, Windows' bitmap: every header from BITMAPCOREHEADER to V5, 1 to
    // 32 bits, RLE4 and RLE8, bit fields (detail::BmpDecoder says which
    // alpha is read). encode writes gray8 through a palette of grays, an
    // image with alpha as 32 bits with a V4 header, any other as 24 bits.
    class bmp {
    public:
        SGCL_INLINE_HOT static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {}) noexcept {
            detail::MemoryInput in(data);
            return detail::BmpDecoder<detail::MemoryInput>(in, o).run();
        }

        SGCL_INLINE_HOT static expected<image, error> decode(const io::reader& in, const decode_options& o = {}) {
            detail::ReaderInput source(in);
            return detail::BmpDecoder<detail::ReaderInput>(source, o).run();
        }

        // The file as bytes: errc::invalid_argument for an image past BMP's
        // 32-bit sizes (a side past 2^31 − 1, a file past 4 GB)
        SGCL_INLINE_HOT static expected<vector<byte>, error> encode(const image& im) noexcept {
            vector<byte> out;
            detail::VectorSink sink{out, nullopt};
            if (!detail::bmp_encode(im, sink)) {
                return unexpected(*sink.failure);
            }
            return out;
        }

        SGCL_INLINE_HOT static expected<void, error> encode(const image& im, const io::writer& out) {
            detail::WriterSink sink{out, 0, nullopt};
            if (!detail::bmp_encode(im, sink)) {
                return unexpected(*sink.failure);
            }
            return {};
        }
    };
}
