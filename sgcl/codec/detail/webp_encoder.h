//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "output.h"
#include "pixels.h"
#include "vp8_encoder.h"
#include "vp8l_encoder.h"
#include "../error.h"
#include "../frames.h"
#include "../image.h"
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
    // webp::options, outside the class: a default member initializer of a
    // nested struct is not usable in the enclosing class's default arguments
    struct WebpOptions {
        bool lossless = false;     // VP8L, every pixel as it is; else VP8, its alpha lossless in ALPH
        int quality = 85;          // 1..100: the quality of a lossy file, the effort of a lossless one (cwebp's -q); save's default
        uint32_t loop_count = 0;   // an animation's plays, as frames::loop_count says: 0 forever, n times
    };

    // The WebP encoder (RFC 9649): a still image, or an animation of whole
    // canvases, into the RIFF container. A still image is the simple format
    // (one VP8L or VP8 chunk) when it carries no metadata and a lossy one
    // no alpha; otherwise VP8X, ICCP, the image (ALPH before a VP8 chunk
    // whose picture has alpha), EXIF. An animation is VP8X, ICCP, ANIM
    // (the loop count, a transparent background), an ANMF chunk a frame,
    // EXIF: each frame after the first the rectangle of the pixels that
    // differ from the frame before (its corner at even coordinates, as
    // ANMF stores them halved), blended onto the canvas with its unchanged
    // pixels transparent when every changed pixel is opaque (and no
    // unchanged one transparent with a color, which blending would clear),
    // else written over the canvas as it is (which takes a pixel to
    // transparent too); never disposed of. The file is made in memory (RIFF's size comes
    // first), then put into the sink at once.
    template<class Sink>
    class WebpEncoder {
    public:
        SGCL_INLINE_HOT WebpEncoder(Sink& sink, const WebpOptions& o) noexcept
        : _sink(sink), _o(o) {
        }

        // false when the sink failed or for what WebP cannot hold (a side
        // past 16384, quality outside 1..100), the error in the sink's
        // failure
        bool still(const image& im) noexcept(NothrowSink<Sink>) {
            const auto& s = ImageAccess::state(im);
            if (!_check(s.width, s.height)) {
                return false;
            }
            std::vector<uint32_t> argb(size_t(s.width) * s.height);
            _argb(im, argb.data());
            bool alpha = false;
            for (uint32_t v : argb) {
                alpha |= (v >> 24) != 0xff;
            }
            std::vector<uint8_t> body;
            _image_chunks(argb.data(), s.width, s.height, s.width, alpha, body);
            const bool extended = !s.icc.empty() || !s.exif.empty() || (alpha && !_o.lossless);
            std::vector<uint8_t> file;
            _riff(file);
            if (extended) {
                _vp8x(file, s.width, s.height, !s.icc.empty(), alpha, !s.exif.empty(), false);
                if (!s.icc.empty()) {
                    _chunk(file, "ICCP", reinterpret_cast<const uint8_t*>(s.icc.data()), s.icc.size());
                }
            }
            file.insert(file.end(), body.begin(), body.end());
            if (extended && !s.exif.empty()) {
                _chunk(file, "EXIF", reinterpret_cast<const uint8_t*>(s.exif.data()), s.exif.size());
            }
            return _finish(file);
        }

        // An animation of whole canvases; false as still, and for no frame
        // or a frame of another size than the first
        bool animation(const slice<const frame>& frames) noexcept(NothrowSink<Sink>) {
            if (frames.size() == 0) {
                return _refuse("webp: an animation of no frame");
            }
            const auto& first = ImageAccess::state(frames[0].picture);
            if (!_check(first.width, first.height)) {
                return false;
            }
            const uint32_t w = first.width, h = first.height;
            for (size_t i = 1; i < frames.size(); ++i) {
                const auto& s = ImageAccess::state(frames[i].picture);
                if (s.width != w || s.height != h) {
                    return _refuse("webp: a frame of another size than the first");
                }
            }
            const size_t area = size_t(w) * h;
            std::vector<uint32_t> prev(area), cur(area), rect;
            std::vector<uint8_t> anmfs;
            bool any_alpha = false;
            for (size_t i = 0; i < frames.size(); ++i) {
                _argb(frames[i].picture, cur.data());
                uint32_t x0 = 0, y0 = 0, x1 = w, y1 = h;
                if (i > 0) {
                    x0 = w;
                    y0 = h;
                    x1 = 0;
                    y1 = 0;
                    for (uint32_t y = 0; y < h; ++y) {
                        const uint32_t* c = cur.data() + size_t(y) * w;
                        const uint32_t* p = prev.data() + size_t(y) * w;
                        uint32_t a = 0;
                        while (a < w && c[a] == p[a]) {
                            ++a;
                        }
                        if (a == w) {
                            continue;
                        }
                        uint32_t b = w;
                        while (c[b - 1] == p[b - 1]) {
                            --b;
                        }
                        x0 = std::min(x0, a);
                        x1 = std::max(x1, b);
                        y0 = std::min(y0, y);
                        y1 = y + 1;
                    }
                    if (x1 <= x0) {
                        // no change: one pixel, blended as transparent
                        x0 = y0 = 0;
                        x1 = y1 = 1;
                    }
                    x0 &= ~1u;
                    y0 &= ~1u;
                }
                const uint32_t rw = x1 - x0, rh = y1 - y0;
                // blended when every changed pixel is opaque: the unchanged
                // ones transparent then, which leaves them as they are, but
                // for a transparent pixel with a color (blending anything
                // over it gives transparent black, the color lost)
                bool blend = i > 0;
                bool alpha = false;
                for (uint32_t y = y0; y < y1 && blend; ++y) {
                    for (uint32_t x = x0; x < x1; ++x) {
                        const uint32_t c = cur[size_t(y) * w + x];
                        const uint32_t p = prev[size_t(y) * w + x];
                        if (c != p ? (c >> 24) != 0xff : (p >> 24) == 0 && p != 0) {
                            blend = false;
                            break;
                        }
                    }
                }
                rect.resize(size_t(rw) * rh);
                for (uint32_t y = 0; y < rh; ++y) {
                    for (uint32_t x = 0; x < rw; ++x) {
                        const size_t at = size_t(y0 + y) * w + x0 + x;
                        uint32_t v = cur[at];
                        if (blend && v == prev[at]) {
                            v = 0;
                        }
                        rect[size_t(y) * rw + x] = v;
                        alpha |= (v >> 24) != 0xff;
                    }
                }
                any_alpha |= alpha;
                std::vector<uint8_t> body;
                _image_chunks(rect.data(), rw, rh, rw, alpha, body);
                uint8_t a[16];
                _le24(a, x0 / 2);
                _le24(a + 3, y0 / 2);
                _le24(a + 6, rw - 1);
                _le24(a + 9, rh - 1);
                _le24(a + 12, _milliseconds(frames[i].delay));
                a[15] = uint8_t(blend ? 0 : 2);
                std::vector<uint8_t> payload(a, a + 16);
                payload.insert(payload.end(), body.begin(), body.end());
                _chunk(anmfs, "ANMF", payload.data(), payload.size());
                prev.swap(cur);
            }
            const auto& meta = first;
            std::vector<uint8_t> file;
            _riff(file);
            _vp8x(file, w, h, !meta.icc.empty(), any_alpha, !meta.exif.empty(), true);
            if (!meta.icc.empty()) {
                _chunk(file, "ICCP", reinterpret_cast<const uint8_t*>(meta.icc.data()), meta.icc.size());
            }
            uint8_t anim[6] = {0, 0, 0, 0, 0, 0};
            anim[4] = uint8_t(std::min<uint32_t>(_o.loop_count, 65535));
            anim[5] = uint8_t(std::min<uint32_t>(_o.loop_count, 65535) >> 8);
            _chunk(file, "ANIM", anim, 6);
            file.insert(file.end(), anmfs.begin(), anmfs.end());
            if (!meta.exif.empty()) {
                _chunk(file, "EXIF", reinterpret_cast<const uint8_t*>(meta.exif.data()), meta.exif.size());
            }
            return _finish(file);
        }

    private:
        bool _refuse(const char* what) noexcept {
            _sink.failure = error(errc::invalid_argument, 0, what);
            return false;
        }

        bool _check(uint32_t w, uint32_t h) noexcept {
            if (_o.quality < 1 || _o.quality > 100) {
                return _refuse("webp: options.quality outside 1..100");
            }
            if (w > 16384 || h > 16384) {
                return _refuse("webp: a side past 16384 pixels, more than VP8 and VP8L hold");
            }
            return true;
        }

        SGCL_INLINE_HOT static uint32_t _milliseconds(const duration& d) noexcept {
            const int64_t ns = d.nanoseconds();
            if (ns <= 0) {
                return 0;
            }
            return uint32_t(std::min<int64_t>((ns + 500'000) / 1'000'000, 0xFFFFFF));
        }

        // The image as ARGB words (0xAARRGGBB)
        static void _argb(const image& im, uint32_t* out) noexcept {
            const auto& s = ImageAccess::state(im);
            const uint8_t* pixels = reinterpret_cast<const uint8_t*>(s.pixels.data());
            std::unique_ptr<uint8_t[]> row;
            const ConvertRow to_rgba = s.format == pixel_format::rgba8 ? nullptr : converter(s.format, pixel_format::rgba8);
            if (to_rgba) {
                row.reset(new uint8_t[size_t(s.width) * 4]);
            }
            for (uint32_t y = 0; y < s.height; ++y) {
                const uint8_t* src = pixels + size_t(y) * s.stride;
                if (to_rgba) {
                    to_rgba(reinterpret_cast<const std::byte*>(src), reinterpret_cast<std::byte*>(row.get()), s.width);
                    src = row.get();
                }
                uint32_t* dst = out + size_t(y) * s.width;
                for (uint32_t x = 0; x < s.width; ++x) {
                    dst[x] = uint32_t(src[4 * x + 3]) << 24 | uint32_t(src[4 * x]) << 16 | uint32_t(src[4 * x + 1]) << 8 | src[4 * x + 2];
                }
            }
        }

        // The chunks of one picture: VP8L, or ALPH (when it has alpha) and
        // VP8
        void _image_chunks(const uint32_t* argb, uint32_t w, uint32_t h, uint32_t stride, bool alpha, std::vector<uint8_t>& out) {
            (void)stride;
            if (_o.lossless) {
                Vp8lBitWriter b;
                Vp8lEncoder().encode(argb, w, h, _o.quality, b);
                _chunk(out, "VP8L", b.out.data(), b.out.size());
                return;
            }
            if (alpha) {
                std::vector<uint8_t> plane(size_t(w) * h);
                for (size_t i = 0; i < plane.size(); ++i) {
                    plane[i] = uint8_t(argb[i] >> 24);
                }
                const unsigned filter = _alpha_filter(plane.data(), w, h);
                Vp8lBitWriter b;
                b.out.push_back(uint8_t(filter << 2 | 1));   // no preprocessing, the filter, lossless
                Vp8lEncoder().encode_alpha(plane.data(), w, h, 50, b);
                _chunk(out, "ALPH", b.out.data(), b.out.size());
            }
            std::vector<uint8_t> vp8;
            Vp8Encoder().encode(argb, w, h, _o.quality, vp8);
            _chunk(out, "VP8 ", vp8.data(), vp8.size());
        }

        // The ALPH filter of least residual cost, the plane filtered in place
        static unsigned _alpha_filter(uint8_t* a, uint32_t w, uint32_t h) {
            // the cost of each filter: how many residuals are not zero and
            // their magnitudes, roughly the bits they take
            auto residual = [&](unsigned f, uint32_t x, uint32_t y) -> int {
                const uint8_t* row = a + size_t(y) * w;
                if (y == 0) {
                    return x == 0 ? row[0] : row[x] - row[x - 1];
                }
                const uint8_t* up = row - w;
                if (x == 0) {
                    return row[0] - up[0];
                }
                int pred;
                if (f == 1) {
                    pred = row[x - 1];
                } else if (f == 2) {
                    pred = up[x];
                } else {
                    pred = std::clamp(row[x - 1] + up[x] - up[x - 1], 0, 255);
                }
                return row[x] - pred;
            };
            uint64_t best = 0;
            unsigned pick = 0;
            for (unsigned f = 0; f < 4; ++f) {
                uint64_t cost = 0;
                for (uint32_t y = 0; y < h; ++y) {
                    for (uint32_t x = 0; x < w; ++x) {
                        const int r = f == 0 ? a[size_t(y) * w + x] : residual(f, x, y);
                        const unsigned m = unsigned(std::abs(int8_t(uint8_t(r))));
                        cost += m == 0 ? 0 : 2 + (32 - unsigned(__builtin_clz(m)));
                    }
                }
                // the unfiltered plane: its runs and few values cost little
                // to VP8L's own predictor and palette: counted at half
                if (f == 0) {
                    cost /= 2;
                }
                if (f == 0 || cost < best) {
                    best = cost;
                    pick = f;
                }
            }
            if (pick) {
                std::vector<uint8_t> out(size_t(w) * h);
                for (uint32_t y = 0; y < h; ++y) {
                    for (uint32_t x = 0; x < w; ++x) {
                        out[size_t(y) * w + x] = uint8_t(residual(pick, x, y));
                    }
                }
                sgcl::detail::copy_bytes(a, out.data(), out.size());
            }
            return pick;
        }

        SGCL_INLINE_HOT static void _le24(uint8_t* p, uint32_t v) noexcept {
            p[0] = uint8_t(v);
            p[1] = uint8_t(v >> 8);
            p[2] = uint8_t(v >> 16);
        }

        SGCL_INLINE_HOT static void _le32(uint8_t* p, uint32_t v) noexcept {
            p[0] = uint8_t(v);
            p[1] = uint8_t(v >> 8);
            p[2] = uint8_t(v >> 16);
            p[3] = uint8_t(v >> 24);
        }

        // A chunk: its tag, size, data and padding to an even size
        static void _chunk(std::vector<uint8_t>& out, const char* tag, const uint8_t* data, size_t n) {
            uint8_t head[8];
            std::memcpy(head, tag, 4);
            _le32(head + 4, uint32_t(n));
            out.insert(out.end(), head, head + 8);
            out.insert(out.end(), data, data + n);
            if (n & 1) {
                out.push_back(0);
            }
        }

        static void _riff(std::vector<uint8_t>& out) {
            const uint8_t head[12] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P'};
            out.insert(out.end(), head, head + 12);
        }

        static void _vp8x(std::vector<uint8_t>& out, uint32_t w, uint32_t h, bool icc, bool alpha, bool exif, bool animated) {
            uint8_t v[10] = {};
            v[0] = uint8_t((icc ? 0x20 : 0) | (alpha ? 0x10 : 0) | (exif ? 0x08 : 0) | (animated ? 0x02 : 0));
            _le24(v + 4, w - 1);
            _le24(v + 7, h - 1);
            _chunk(out, "VP8X", v, 10);
        }

        // RIFF's size set, the file into the sink; false past 4 GB (RIFF's
        // 32 bits)
        bool _finish(std::vector<uint8_t>& file) noexcept(NothrowSink<Sink>) {
            if (file.size() - 8 > 0xFFFFFFF6u) {
                return _refuse("webp: a file past 4 GB, more than RIFF holds");
            }
            _le32(file.data() + 4, uint32_t(file.size() - 8));
            return _sink.put(file.data(), file.size());
        }

        Sink& _sink;
        WebpOptions _o;
    };
}
