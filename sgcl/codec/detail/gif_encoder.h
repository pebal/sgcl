//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "output.h"
#include "pixels.h"
#include "quantize.h"
#include "../error.h"
#include "../frames.h"
#include "../image.h"
#include "../../compress/lzw.h"
#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"
#include "../../core/slice.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace sgcl::codec::detail {
    // gif::options, outside the class: a default member initializer of a
    // nested struct is not usable in the enclosing class's default arguments
    struct GifOptions {
        int colors = 256;          // 2..256: the most entries of a palette, a transparent one counted
        bool dither = true;        // Floyd–Steinberg where the image has more colors than the palette
        uint32_t loop_count = 0;   // an animation's plays, as frames::loop_count says: 0 forever, 1 once, n times
    };

    // The GIF encoder (GIF89a): a still image as one frame of the whole
    // screen, an animation of whole canvases as frames of what changed.
    //
    // A pixel of alpha below 128 is transparent, any other opaque (GIF has
    // one bit); each frame gets a palette of its own (detail/quantize.h),
    // the first frame's the global table, a later one's a local table
    // unless it is the same. An animation's frame after the first covers
    // the pixels that differ from what the frame before leaves, the
    // unchanged ones inside its rectangle written as the transparent index;
    // where a pixel goes from opaque to transparent, which drawing over a
    // canvas cannot do, the frame before is disposed of to the background
    // (cleared to transparent, as the decoder and browsers show it), its
    // rectangle grown over those pixels. Delays in hundredths, rounded to
    // the nearest. The LZW data is compress's encoder, cut into sub-blocks
    // of 255 bytes.
    //
    // Memory: a row of the image and of its indices; the quantizer's
    // tables, a frame at a time (an image of few colors: a byte a pixel of
    // indices; any other: about 1.6 MB of histogram, candidates and colors
    // met, and 512 KB more with dithering); for an animation, three
    // canvases of 4 bytes a pixel.
    template<class Sink>
    class GifEncoder {
    public:
        SGCL_INLINE_HOT GifEncoder(Sink& sink, const GifOptions& o) noexcept
        : _sink(sink), _o(o) {
        }

        // A still image; false when the sink failed or for what GIF cannot
        // hold (a side past 65535, colors outside 2..256), the error in the
        // sink's failure
        bool still(const image& im) noexcept(NothrowSink<Sink>) {
            const auto& s = ImageAccess::state(im);
            if (!_check(s.width, s.height)) {
                return false;
            }
            _w = s.width;
            _h = s.height;
            _row.reset(new uint32_t[_w]);
            _scratch.reset(new uint8_t[size_t(_w) * 4]);
            const uint8_t* pixels = reinterpret_cast<const uint8_t*>(s.pixels.data());
            const size_t stride = s.stride;
            const pixel_format f = s.format;
            auto rows = [&](uint32_t y) -> const uint32_t* {
                shown_row(pixels + size_t(y) * stride, f, _row.get(), _w, _scratch.get());
                return _row.get();
            };
            if (!_header()) {
                return false;
            }
            if (!_frame(rows, rows, 0, 0, _w, _h, 0, 0, false)) {
                return false;
            }
            return _put("\x3B", 1);
        }

        // An animation of whole canvases; false as still, and for no frame
        // or a frame of another size than the first
        bool animation(const slice<const frame>& frames) noexcept(NothrowSink<Sink>) {
            if (frames.size() == 0) {
                return _refuse("gif: an animation of no frame");
            }
            const auto& first = ImageAccess::state(frames[0].picture);
            if (!_check(first.width, first.height)) {
                return false;
            }
            _w = first.width;
            _h = first.height;
            for (size_t i = 1; i < frames.size(); ++i) {
                const auto& s = ImageAccess::state(frames[i].picture);
                if (s.width != _w || s.height != _h) {
                    return _refuse("gif: a frame of another size than the first");
                }
            }
            const size_t area = size_t(_w) * _h;
            _scratch.reset(new uint8_t[size_t(_w) * 4]);
            _row.reset(new uint32_t[_w]);
            // what the frame before leaves (the frame shown, disposed of
            // as it is), the frame and the one after it, all shown
            std::unique_ptr<uint32_t[]> left(new uint32_t[area]());   // the canvas starts transparent
            std::unique_ptr<uint32_t[]> cur(new uint32_t[area]);
            std::unique_ptr<uint32_t[]> next(new uint32_t[area]);
            _shown(frames[0].picture, cur.get());
            _animated = true;
            if (!_header()) {
                return false;
            }
            for (size_t i = 0; i < frames.size(); ++i) {
                const bool more = i + 1 < frames.size();
                if (more) {
                    _shown(frames[i + 1].picture, next.get());
                }
                // the rectangle: the first frame the whole screen, a later
                // one the pixels that differ from what is left
                uint32_t x0 = 0, y0 = 0, x1 = _w, y1 = _h;
                if (i > 0) {
                    _bounds(cur.get(), left.get(), x0, y0, x1, y1);
                }
                // disposal 2 when the next frame turns a pixel transparent
                // that this one shows: the rectangle grown over every such
                // pixel, cleared after this frame
                bool clear = false;
                if (more) {
                    uint32_t cx0 = _w, cy0 = _h, cx1 = 0, cy1 = 0;
                    for (uint32_t y = 0; y < _h; ++y) {
                        const uint32_t* c = cur.get() + size_t(y) * _w;
                        const uint32_t* n = next.get() + size_t(y) * _w;
                        for (uint32_t x = 0; x < _w; ++x) {
                            if (c[x] != 0 && n[x] == 0) {
                                cx0 = std::min(cx0, x);
                                cx1 = std::max(cx1, x + 1);
                                cy0 = std::min(cy0, y);
                                cy1 = y + 1;
                            }
                        }
                    }
                    if (cx1 > cx0) {
                        clear = true;
                        x0 = std::min(x0, cx0);
                        y0 = std::min(y0, cy0);
                        x1 = std::max(x1, cx1);
                        y1 = std::max(y1, cy1);
                    }
                }
                const uint32_t* c = cur.get();
                const uint32_t* l = left.get();
                const uint32_t w = _w;
                // the rectangle's own pixels
                auto plain = [&, x0, y0](uint32_t y) -> const uint32_t* {
                    return c + size_t(y0 + y) * w + x0;
                };
                auto rows = [&, x0, y0](uint32_t y) -> const uint32_t* {
                    const uint32_t* cr = c + size_t(y0 + y) * w + x0;
                    if (i == 0) {
                        return cr;
                    }
                    // a pixel the same as what is left: transparent, kept
                    const uint32_t* lr = l + size_t(y0 + y) * w + x0;
                    for (uint32_t x = 0; x < x1 - x0; ++x) {
                        _row[x] = cr[x] == lr[x] ? 0 : cr[x];
                    }
                    return _row.get();
                };
                if (!_frame(rows, plain, x0, y0, x1 - x0, y1 - y0, _centiseconds(frames[i].delay), clear ? 2 : 1, true)) {
                    return false;
                }
                // what this frame leaves: itself, its rectangle cleared
                // for disposal 2
                std::swap(left, cur);
                if (clear) {
                    for (uint32_t y = y0; y < y1; ++y) {
                        std::fill_n(left.get() + size_t(y) * _w + x0, x1 - x0, 0u);
                    }
                }
                std::swap(cur, next);
            }
            return _put("\x3B", 1);
        }

    private:
        SGCL_INLINE_HOT bool _put(const void* p, size_t n) noexcept(NothrowSink<Sink>) {
            return _sink.put(static_cast<const uint8_t*>(p), n);
        }

        bool _refuse(const char* what) noexcept {
            _sink.failure = error(errc::invalid_argument, 0, what);
            return false;
        }

        bool _check(uint32_t w, uint32_t h) noexcept {
            if (_o.colors < 2 || _o.colors > 256) {
                return _refuse("gif: options.colors outside 2..256");
            }
            if (w > 65535 || h > 65535) {
                return _refuse("gif: a side past 65535 pixels, more than the logical screen holds");
            }
            return true;
        }

        // A duration in hundredths of a second, rounded to the nearest,
        // within GIF's 16 bits
        SGCL_INLINE_HOT static uint32_t _centiseconds(const duration& d) noexcept {
            const int64_t ns = d.nanoseconds();
            if (ns <= 0) {
                return 0;
            }
            const int64_t cs = (ns / 5'000'000 + 1) / 2;
            return uint32_t(std::min<int64_t>(cs, 65535));
        }

        // A whole frame in the shown form
        void _shown(const image& im, uint32_t* out) noexcept {
            const auto& s = ImageAccess::state(im);
            const uint8_t* pixels = reinterpret_cast<const uint8_t*>(s.pixels.data());
            for (uint32_t y = 0; y < _h; ++y) {
                shown_row(pixels + size_t(y) * s.stride, s.format, out + size_t(y) * _w, _w, _scratch.get());
            }
        }

        // The rectangle of the pixels of cur that differ from left; one
        // pixel at the corner when none does (a frame has one at least)
        void _bounds(const uint32_t* cur, const uint32_t* left, uint32_t& x0, uint32_t& y0, uint32_t& x1, uint32_t& y1) const noexcept {
            x0 = _w;
            y0 = _h;
            x1 = 0;
            y1 = 0;
            for (uint32_t y = 0; y < _h; ++y) {
                const uint32_t* c = cur + size_t(y) * _w;
                const uint32_t* l = left + size_t(y) * _w;
                uint32_t a = 0;
                while (a < _w && c[a] == l[a]) {
                    ++a;
                }
                if (a == _w) {
                    continue;
                }
                uint32_t b = _w;
                while (c[b - 1] == l[b - 1]) {
                    --b;
                }
                x0 = std::min(x0, a);
                x1 = std::max(x1, b);
                y0 = std::min(y0, y);
                y1 = y + 1;
            }
            if (x1 <= x0) {
                x0 = y0 = 0;
                x1 = y1 = 1;
            }
        }

        SGCL_INLINE_HOT static void _le16(uint8_t* p, uint32_t v) noexcept {
            p[0] = uint8_t(v);
            p[1] = uint8_t(v >> 8);
        }

        // The header and the logical screen; the global table comes with
        // the first frame
        bool _header() noexcept(NothrowSink<Sink>) {
            uint8_t h[6] = {'G', 'I', 'F', '8', '9', 'a'};
            return _put(h, 6);
        }

        // The bits of a table of n entries (1..256): 2 entries at least
        SGCL_INLINE_HOT static unsigned _bits(unsigned n) noexcept {
            unsigned k = 1;
            while ((1u << k) < n) {
                ++k;
            }
            return k;
        }

        bool _table(const Palette& p, unsigned bits) noexcept(NothrowSink<Sink>) {
            uint8_t t[768] = {};
            for (unsigned i = 0; i < p.size; ++i) {
                t[3 * i] = p.rgb[i][0];
                t[3 * i + 1] = p.rgb[i][1];
                t[3 * i + 2] = p.rgb[i][2];
            }
            return _put(t, 3u << bits);
        }

        // One frame: its palette, the logical screen and global table for
        // the first, the extensions, the image descriptor and its LZW data
        // plain: the rectangle's own pixels, where rows has the unchanged
        // ones transparent; taken when they fit a palette exactly and rows
        // do not (the transparent entry the unchanged pixels need may be
        // the one too many)
        template<class Rows, class Plain>
        bool _frame(Rows& rows, Plain& plain, uint32_t x0, uint32_t y0, uint32_t w, uint32_t h, uint32_t delay, unsigned disposal,
                    bool animated) noexcept(NothrowSink<Sink>) {
            Quantizer kept(unsigned(_o.colors), _o.dither);
            kept.build(rows, w, h);
            optional<Quantizer> own;
            if (!kept.exact() && static_cast<const void*>(&rows) != static_cast<const void*>(&plain)) {
                own.emplace(unsigned(_o.colors), _o.dither);
                own->build(plain, w, h);
                if (!own->exact()) {
                    own.reset();
                }
            }
            Quantizer& q = own ? *own : kept;
            const Palette& p = q.palette();
            const unsigned bits = _bits(std::max(1u, p.entries()));
            bool local = true;
            if (!_screen_written) {
                _screen_written = true;
                uint8_t s[7];
                _le16(s, _w);
                _le16(s + 2, _h);
                s[4] = uint8_t(0x80 | 0x70 | (bits - 1));   // a global table, 8 bits of color resolution
                s[5] = 0;                                    // the background: entry 0 (shown transparent)
                s[6] = 0;
                if (!_put(s, 7) || !_table(p, bits)) {
                    return false;
                }
                _global = p;
                _global_bits = bits;
                local = false;
                if (animated && _o.loop_count != 1) {
                    // NETSCAPE2.0: loops after the first play, 0 forever
                    const uint32_t loops = _o.loop_count == 0 ? 0 : std::min<uint32_t>(_o.loop_count - 1, 65535);
                    uint8_t a[19] = {0x21, 0xFF, 11, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0', 3, 1, 0, 0, 0};
                    _le16(a + 16, loops);
                    if (!_put(a, 19)) {
                        return false;
                    }
                }
            } else if (bits == _global_bits && p.entries() == _global.entries() && p.transparent == _global.transparent &&
                       std::memcmp(p.rgb, _global.rgb, 3 * size_t(p.size)) == 0) {
                local = false;
            }
            if (animated || p.transparent) {
                uint8_t g[8] = {0x21, 0xF9, 4, 0, 0, 0, 0, 0};
                g[3] = uint8_t((animated ? disposal : 0) << 2 | (p.transparent ? 1 : 0));
                _le16(g + 4, delay);
                g[6] = uint8_t(p.transparent ? p.size : 0);
                if (!_put(g, 8)) {
                    return false;
                }
            }
            uint8_t d[10];
            d[0] = 0x2C;
            _le16(d + 1, x0);
            _le16(d + 3, y0);
            _le16(d + 5, w);
            _le16(d + 7, h);
            d[9] = local ? uint8_t(0x80 | (bits - 1)) : 0;
            if (!_put(d, 10) || (local && !_table(p, bits))) {
                return false;
            }
            const unsigned code = std::max(2u, bits);
            const uint8_t c = uint8_t(code);
            if (!_put(&c, 1)) {
                return false;
            }
            compress::detail::LzwEncoder lzw(false, int(code));
            std::unique_ptr<uint8_t[]> indices(new uint8_t[w]);
            _lzw.clear();
            for (uint32_t y = 0; y < h; ++y) {
                const uint8_t* row = indices.get();
                if (q.exact()) {
                    row = q.stored(y);
                } else {
                    q.map_row(rows(y), indices.get());
                }
                lzw.write(row, w, _lzw);
                if (_lzw.size() >= BlockBatch && !_blocks(false)) {
                    return false;
                }
            }
            lzw.finish(_lzw);
            return _blocks(true);
        }

        // The LZW bytes as sub-blocks of 255 (and the rest, with the block
        // terminator, at the end)
        bool _blocks(bool end) noexcept(NothrowSink<Sink>) {
            _out.clear();
            size_t at = 0;
            const size_t n = _lzw.size();
            while (n - at >= 255 || (end && at < n)) {
                const size_t k = std::min<size_t>(255, n - at);
                _out.push_back(uint8_t(k));
                _out.insert(_out.end(), _lzw.begin() + ptrdiff_t(at), _lzw.begin() + ptrdiff_t(at + k));
                at += k;
            }
            if (end) {
                _out.push_back(0);
            }
            _lzw.erase(_lzw.begin(), _lzw.begin() + ptrdiff_t(at));
            return _out.empty() || _put(_out.data(), _out.size());
        }

        static constexpr size_t BlockBatch = 255 * 256;

        Sink& _sink;
        GifOptions _o;
        uint32_t _w = 0;
        uint32_t _h = 0;
        bool _animated = false;
        bool _screen_written = false;
        Palette _global;
        unsigned _global_bits = 0;
        std::unique_ptr<uint32_t[]> _row;
        std::unique_ptr<uint8_t[]> _scratch;
        std::vector<uint8_t> _lzw;
        std::vector<uint8_t> _out;
    };
}
