//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "vp8l_decoder.h"
#include "vp8l_simd.h"
#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace sgcl::codec::detail {
    // The bits of a VP8L bitstream, least significant first, into bytes
    class Vp8lBitWriter {
    public:
        std::vector<uint8_t> out;

        // n bits of v (n up to 32)
        SGCL_INLINE_HOT void put(uint32_t v, unsigned n) noexcept {
            _bits |= uint64_t(v) << _count;
            _count += n;
            if (_count >= 32) {
                const uint32_t w = uint32_t(_bits);
                const uint8_t b[4] = {uint8_t(w), uint8_t(w >> 8), uint8_t(w >> 16), uint8_t(w >> 24)};
                out.insert(out.end(), b, b + 4);
                _bits >>= 32;
                _count -= 32;
            }
        }

        // The last bits to a byte
        void finish() noexcept {
            while (_count > 0) {
                out.push_back(uint8_t(_bits));
                _bits >>= 8;
                _count = _count > 8 ? _count - 8 : 0;
            }
            _bits = 0;
        }

        // The bits written so far
        SGCL_INLINE_HOT uint64_t size() const noexcept {
            return uint64_t(out.size()) * 8 + _count;
        }

    private:
        uint64_t _bits = 0;
        unsigned _count = 0;
    };

    namespace vp8l {
        // log2 of n and n · log2(n), from a table for the small n met most
        inline float log2_of(uint32_t n) noexcept {
            struct Table {
                float v[4096];
                Table() noexcept {
                    v[0] = 0;
                    for (unsigned i = 1; i < 4096; ++i) {
                        v[i] = float(std::log2(double(i)));
                    }
                }
            };
            static const Table t;
            return n < 4096 ? t.v[n] : float(std::log2(double(n)));
        }

        SGCL_INLINE_HOT float nlog2(uint32_t n) noexcept {
            return float(n) * log2_of(n);
        }

        // The bits of the symbols counted in h[0..n) under their own best
        // code (Shannon's), plus a rough cost of the code itself
        // (a symbol of a prefix code takes a whole bit at least: a symbol
        // more likely than a half costs its count in bits)
        inline float entropy_bits(const uint32_t* h, unsigned n) noexcept {
            uint32_t total = 0;
            unsigned used = 0;
            for (unsigned i = 0; i < n; ++i) {
                total += h[i];
                used += h[i] != 0;
            }
            if (used <= 1) {
                return 0;
            }
            const float lt = log2_of(total);
            float bits = 0;
            for (unsigned i = 0; i < n; ++i) {
                if (const uint32_t c = h[i]) {
                    bits += float(c) * std::max(1.0f, lt - log2_of(c));
                }
            }
            return bits + float(used) * 3.5f;
        }

        // The prefix code of a length (1..4096) or a distance code: the code,
        // its extra bits and their value
        SGCL_INLINE_HOT void prefix_of(uint32_t value, uint32_t& code, unsigned& extra, uint32_t& bits) noexcept {
            const uint32_t v = value - 1;
            if (v < 4) {
                code = v;
                extra = 0;
                bits = 0;
                return;
            }
            const unsigned high = 31u - unsigned(__builtin_clz(v));   // v >= 4: high >= 2
            const unsigned second = (v >> (high - 1)) & 1;
            extra = high - 1;
            code = 2 * high + second;
            bits = v & ((uint32_t(1) << extra) - 1);
        }

        // The cost in bits of a prefix value's extra bits
        SGCL_INLINE_HOT unsigned prefix_extra(uint32_t value) noexcept {
            const uint32_t v = value - 1;
            return v < 4 ? 0u : 30u - unsigned(__builtin_clz(v));
        }

        // A distance in pixels as the code VP8L writes: one of the 120
        // places near the pixel (1..120) when it is one, else distance + 120
        struct DistanceCodes {
            uint8_t code[16][17];   // [yi][xi + 8]: the code of (xi, yi) for yi < 16, 0 for none
            DistanceCodes() noexcept {
                std::memset(code, 0, sizeof(code));
                for (unsigned i = 0; i < 120; ++i) {
                    const int xi = DistanceMap[i][0], yi = DistanceMap[i][1];
                    code[yi][xi + 8] = uint8_t(i + 1);
                }
            }
        };

        inline uint32_t distance_code(uint32_t distance, uint32_t xsize) noexcept {
            static const DistanceCodes t;
            const uint32_t yi = distance / xsize;
            const uint32_t xi = distance - yi * xsize;
            if (yi < 8 && xi <= 8) {
                if (const uint8_t c = t.code[yi][xi + 8]) {
                    return c;
                }
            }
            if (yi + 1 < 8 && xsize - xi <= 8) {
                if (const uint8_t c = t.code[yi + 1][8 - (xsize - xi)]) {
                    return c;
                }
            }
            return distance + 120;
        }

        // The hash of the color cache
        SGCL_INLINE_HOT uint32_t cache_slot(uint32_t argb, unsigned bits) noexcept {
            return (0x1e35a7bdu * argb) >> (32 - bits);
        }
    }

    // A prefix code made for counted symbols: lengths of at most `limit`
    // bits (the Huffman lengths, the counts flattened until they fit), the
    // canonical codes of RFC 9649 as the decoder builds them, bits reversed
    // for writing least significant first
    struct Vp8lCode {
        std::vector<uint8_t> length;
        std::vector<uint16_t> code;

        void make(const uint32_t* counts, unsigned n, unsigned limit) {
            length.assign(n, 0);
            code.assign(n, 0);
            unsigned used = 0;
            for (unsigned i = 0; i < n; ++i) {
                used += counts[i] != 0;
            }
            if (used == 0) {
                return;
            }
            if (used == 1) {
                for (unsigned i = 0; i < n; ++i) {
                    if (counts[i]) {
                        length[i] = 1;   // a code of one symbol: no bits written for it
                    }
                }
                return;
            }
            std::vector<uint32_t> c(counts, counts + n);
            for (uint32_t floor = 1;; floor *= 2) {
                if (_huffman(c.data(), n, limit)) {
                    break;
                }
                for (unsigned i = 0; i < n; ++i) {
                    if (c[i] && c[i] < floor) {
                        c[i] = floor;
                    }
                }
            }
            _canonical(n);
        }

        // The symbols with a length: 0, 1 or more
        SGCL_INLINE_HOT unsigned used() const noexcept {
            unsigned k = 0;
            for (uint8_t l : length) {
                k += l != 0;
            }
            return k;
        }

    private:
        // Huffman's lengths of the counts; false when one passes limit
        bool _huffman(const uint32_t* counts, unsigned n, unsigned limit) {
            struct Node {
                uint64_t weight;
                int left, right;
            };
            std::vector<Node> nodes;
            nodes.reserve(2 * n);
            // a min-heap of node indices by weight, ties by index
            std::vector<int> heap;
            auto less = [&](int a, int b) {
                return nodes[size_t(a)].weight != nodes[size_t(b)].weight ? nodes[size_t(a)].weight > nodes[size_t(b)].weight : a > b;
            };
            std::vector<int> leaf_of(n, -1);
            for (unsigned i = 0; i < n; ++i) {
                if (counts[i]) {
                    leaf_of[i] = int(nodes.size());
                    heap.push_back(int(nodes.size()));
                    nodes.push_back({counts[i], -1, -1});
                }
            }
            std::make_heap(heap.begin(), heap.end(), less);
            while (heap.size() > 1) {
                std::pop_heap(heap.begin(), heap.end(), less);
                const int a = heap.back();
                heap.pop_back();
                std::pop_heap(heap.begin(), heap.end(), less);
                const int b = heap.back();
                heap.pop_back();
                nodes.push_back({nodes[size_t(a)].weight + nodes[size_t(b)].weight, a, b});
                heap.push_back(int(nodes.size()) - 1);
                std::push_heap(heap.begin(), heap.end(), less);
            }
            // the depth of each node from the root, the last made
            std::vector<uint8_t> depth(nodes.size(), 0);
            for (size_t k = nodes.size(); k-- > 0;) {
                const Node& nd = nodes[k];
                if (nd.left >= 0) {
                    depth[size_t(nd.left)] = uint8_t(std::min(255, depth[k] + 1));
                    depth[size_t(nd.right)] = uint8_t(std::min(255, depth[k] + 1));
                }
            }
            for (unsigned i = 0; i < n; ++i) {
                if (leaf_of[i] >= 0) {
                    const unsigned d = depth[size_t(leaf_of[i])];
                    if (d > limit) {
                        return false;
                    }
                    length[i] = uint8_t(d);
                }
            }
            return true;
        }

        void _canonical(unsigned n) noexcept {
            unsigned count[16] = {};
            for (unsigned i = 0; i < n; ++i) {
                ++count[length[i]];
            }
            count[0] = 0;
            uint32_t next[16];
            uint32_t c = 0;
            for (unsigned l = 1; l < 16; ++l) {
                c = (c + count[l - 1]) << 1;
                next[l] = c;
            }
            for (unsigned i = 0; i < n; ++i) {
                const unsigned l = length[i];
                if (l) {
                    uint32_t v = next[l]++;
                    uint32_t r = 0;
                    for (unsigned k = 0; k < l; ++k) {
                        r = r << 1 | (v & 1);
                        v >>= 1;
                    }
                    code[i] = uint16_t(r);
                }
            }
        }
    };

    // A symbol of an entropy-coded image: a literal pixel, a cache index or
    // a back reference
    struct Vp8lToken {
        uint32_t value;    // literal: ARGB; cache: the index; copy: the distance code
        uint16_t length;   // copy: the length (1..4096); 0: literal; 0xFFFF: cache
    };

    // The VP8L encoder (RFC 9649 §3): an image of ARGB words into a
    // bitstream, header and all. What it does by effort (0..100, cwebp's
    // -q of a lossless file):
    //   - an image of at most 256 colors: the color indexing transform, its
    //     palette sorted, the indices bundled 8, 4 or 2 to a pixel when the
    //     palette has at most 2, 4 or 16 entries;
    //   - any other: subtract green, the predictor transform (a mode per
    //     tile of 2^bits pixels a side, the one whose residuals cost least),
    //     the cross-color transform (multipliers per tile by least cost);
    //   - then the entropy-coded image: LZ77 over a hash chain of pairs of
    //     pixels (the chain as long as the effort asks), the pixel to the
    //     left and above tried first; the color cache of the size whose
    //     symbols cost least; Huffman codes of at most 15 bits, one group,
    //     or at the higher efforts groups per tile (the entropy image) made
    //     by merging tiles whose histograms cost least together.
    // The pixels go out exactly as given, the RGB of transparent ones too.
    class Vp8lEncoder {
    public:
        // The bitstream of w × h ARGB pixels (w, h up to 16384), its header
        // first; alpha says whether any pixel is not opaque
        void encode(const uint32_t* argb, uint32_t w, uint32_t h, int effort, Vp8lBitWriter& b) {
            _effort = std::clamp(effort, 0, 100);
            bool alpha = false;
            const size_t n = size_t(w) * h;
            for (size_t i = 0; i < n; ++i) {
                alpha |= (argb[i] >> 24) != 0xff;
            }
            b.put(0x2F, 8);
            b.put((w - 1) | (h - 1) << 14, 28);
            b.put(alpha ? 1 : 0, 1);
            b.put(0, 3);
            _image(argb, w, h, b, true);
            b.finish();
        }

        // The pixels of an alpha plane (ALPH, RFC 9649 §2.7.1.6): the alpha
        // values in the green channel, no header
        void encode_alpha(const uint8_t* alpha, uint32_t w, uint32_t h, int effort, Vp8lBitWriter& b) {
            _effort = std::clamp(effort, 0, 100);
            std::vector<uint32_t> px(size_t(w) * h);
            for (size_t i = 0; i < px.size(); ++i) {
                px[i] = 0xff000000u | uint32_t(alpha[i]) << 8;
            }
            _image(px.data(), w, h, b, true);
            b.finish();
        }

    private:
        // The transforms and the main image
        void _image(const uint32_t* argb, uint32_t w, uint32_t h, Vp8lBitWriter& b, bool main) {
            (void)main;
            const size_t n = size_t(w) * h;
            std::vector<uint32_t> px(argb, argb + n);
            uint32_t xsize = w;
            uint32_t palette[256];
            unsigned colors = 0;
            if (_palette(px.data(), n, palette, colors)) {
                xsize = _index(px, w, h, palette, colors, b);
            } else {
                // subtract green
                b.put(1, 1);
                b.put(2, 2);
                for (uint32_t& v : px) {
                    const uint32_t g = (v >> 8) & 0xff;
                    v = (v & 0xff00ff00u) | ((((v >> 16) - g) & 0xff) << 16) | ((v - g) & 0xff);
                }
                if (w > 1 || h > 1) {
                    _predict(px, w, h, b);
                    // the cross-color transform: at the highest efforts both
                    // ways, the smaller kept (what it gains in the channels'
                    // spread it may lose in the copies and the cache, which
                    // see whole pixels; no estimate tells which beforehand)
                    if (_effort >= 90) {
                        Vp8lBitWriter with = b;
                        std::vector<uint32_t> crossed = px;
                        _cross_color(crossed, w, h, with);
                        with.put(0, 1);
                        _stream(crossed.data(), xsize, h, with, true);
                        b.put(0, 1);
                        _stream(px.data(), xsize, h, b, true);
                        if (with.size() < b.size()) {
                            b = std::move(with);
                        }
                        return;
                    }
                }
            }
            b.put(0, 1);   // no more transforms
            _stream(px.data(), xsize, h, b, true);
        }

        // The colors of the image when there are at most 256, sorted
        static bool _palette(const uint32_t* px, size_t n, uint32_t* palette, unsigned& colors) noexcept {
            uint32_t keys[1024];
            bool used[1024] = {};
            colors = 0;
            uint32_t last = ~px[0];
            for (size_t i = 0; i < n; ++i) {
                const uint32_t v = px[i];
                if (v == last) {
                    continue;
                }
                last = v;
                uint32_t s = (v * 0x9E3779B1u) >> 22;
                while (used[s] && keys[s] != v) {
                    s = (s + 1) & 1023;
                }
                if (!used[s]) {
                    if (colors == 256) {
                        return false;
                    }
                    used[s] = true;
                    keys[s] = v;
                    palette[colors++] = v;
                }
            }
            std::sort(palette, palette + colors);
            return true;
        }

        // The color indexing transform: the palette (deltas, as an image of
        // colors × 1), the pixels replaced by their indices, bundled; the
        // width of the packed image
        uint32_t _index(std::vector<uint32_t>& px, uint32_t w, uint32_t h, const uint32_t* palette, unsigned colors, Vp8lBitWriter& b) {
            b.put(1, 1);
            b.put(3, 2);
            b.put(colors - 1, 8);
            uint32_t deltas[256];
            for (unsigned i = 0; i < colors; ++i) {
                deltas[i] = i == 0 ? palette[0] : _sub_pixels(palette[i], palette[i - 1]);
            }
            _stream(deltas, colors, 1, b, false);
            const unsigned bits = colors > 16 ? 0 : colors > 4 ? 1 : colors > 2 ? 2 : 3;
            const uint32_t packed = vp8l::div_round_up(w, bits);
            const unsigned width = 8 >> bits;
            // the index of each color: palette sorted, a binary search
            auto index_of = [&](uint32_t v) {
                return uint32_t(std::lower_bound(palette, palette + colors, v) - palette);
            };
            std::vector<uint32_t> out(size_t(packed) * h);
            for (uint32_t y = 0; y < h; ++y) {
                const uint32_t* src = px.data() + size_t(y) * w;
                uint32_t* dst = out.data() + size_t(y) * packed;
                uint32_t last = src[0], last_index = index_of(src[0]);
                for (uint32_t x = 0; x < w; ++x) {
                    if (src[x] != last) {
                        last = src[x];
                        last_index = index_of(last);
                    }
                    dst[x >> bits] |= last_index << ((x & ((1u << bits) - 1)) * width);
                }
                for (uint32_t x = 0; x < packed; ++x) {
                    dst[x] = 0xff000000u | (dst[x] & 0xff) << 8;
                }
            }
            px.swap(out);
            return packed;
        }

        SGCL_INLINE_HOT static uint32_t _sub_pixels(uint32_t a, uint32_t b) noexcept {
            const uint32_t ag = 0x00ff00ffu + (a & 0xff00ff00u) - (b & 0xff00ff00u);
            const uint32_t rb = 0xff00ff00u + (a & 0x00ff00ffu) - (b & 0x00ff00ffu);
            return (ag & 0xff00ff00u) | (rb & 0x00ff00ffu);
        }

        // The prediction of mode m for pixel (x, y) of the image p (x, y >
        // 0), from the pixels as they are (the decoder has them so)
        SGCL_INLINE_HOT static uint32_t _predict_at(unsigned m, const uint32_t* p, uint32_t x, uint32_t y, uint32_t w) noexcept {
            const uint32_t* cur = p + size_t(y) * w;
            const uint32_t* top = cur - w;
            const uint32_t L = cur[x - 1];
            const uint32_t T = top[x];
            const uint32_t TL = top[x - 1];
            const uint32_t TR = top[x + 1];   // the row's first pixel for the last column: top + w is cur
            switch (m) {
                case 0: return 0xff000000u;
                case 1: return L;
                case 2: return T;
                case 3: return TR;
                case 4: return TL;
                case 5: return vp8l::average2(vp8l::average2(L, TR), T);
                case 6: return vp8l::average2(L, TL);
                case 7: return vp8l::average2(L, T);
                case 8: return vp8l::average2(TL, T);
                case 9: return vp8l::average2(T, TR);
                case 10: return vp8l::average2(vp8l::average2(L, TL), vp8l::average2(T, TR));
                case 11: return vp8l::select_plain(L, T, TL);
                case 12: return vp8l::clamp_full_plain(L, T, TL);
                default: return vp8l::clamp_half_plain(vp8l::average2(L, T), TL);
            }
        }

        // The tile bits of a transform by the image and the effort
        unsigned _tile_bits(uint32_t w, uint32_t h, unsigned base) const noexcept {
            unsigned bits = base;
            if (_effort < 50) {
                ++bits;
            }
            const uint64_t pixels = uint64_t(w) * h;
            if (pixels < 64 * 64) {
                bits = std::min(bits, 3u);
            }
            return std::clamp(bits, 2u, 9u);
        }

        // What the residuals chosen so far are, channel by channel, as the
        // bits a byte of each channel would cost under their distribution:
        // the cost of a tile's choice measured against the code the whole
        // image will have rather than the tile's own
        struct Adaptive {
            uint32_t count[4][256];
            float cost[4][256];
            uint64_t total[4];

            Adaptive() noexcept {
                // a prior: small residuals likely, as after any good predictor
                for (unsigned c = 0; c < 4; ++c) {
                    total[c] = 0;
                    for (unsigned v = 0; v < 256; ++v) {
                        const unsigned m = v < 128 ? v : 256 - v;
                        count[c][v] = 1 + 64 / (1 + m);
                        total[c] += count[c][v];
                    }
                }
                refresh();
            }

            void refresh() noexcept {
                for (unsigned c = 0; c < 4; ++c) {
                    const float lt = float(std::log2(double(total[c])));
                    for (unsigned v = 0; v < 256; ++v) {
                        cost[c][v] = lt - vp8l::log2_of(count[c][v]);
                    }
                }
            }

            SGCL_INLINE_HOT float of(uint32_t r) const noexcept {
                return cost[0][r & 0xff] + cost[1][(r >> 8) & 0xff] + cost[2][(r >> 16) & 0xff] + cost[3][r >> 24];
            }

            SGCL_INLINE_HOT void add(uint32_t r) noexcept {
                ++count[0][r & 0xff];
                ++count[1][(r >> 8) & 0xff];
                ++count[2][(r >> 16) & 0xff];
                ++count[3][r >> 24];
                total[0] += 1;
                total[1] += 1;
                total[2] += 1;
                total[3] += 1;
            }
        };

        // The predictor transform: each tile's mode the one whose residuals
        // cost least under the residuals chosen so far (and a little more
        // for a mode other than its neighbors', which the mode image codes
        // cheaply); the residuals replace the pixels
        void _predict(std::vector<uint32_t>& px, uint32_t w, uint32_t h, Vp8lBitWriter& b) {
            const unsigned bits = _tile_bits(w, h, 3);
            const uint32_t tw = vp8l::div_round_up(w, bits), th = vp8l::div_round_up(h, bits);
            std::vector<uint32_t> modes(size_t(tw) * th);
            const uint32_t* p = px.data();
            static const unsigned all[14] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
            static const unsigned few[6] = {1, 2, 7, 11, 12, 13};
            const unsigned* tried = _effort >= 25 ? all : few;
            const unsigned count = _effort >= 25 ? 14 : 6;
            Adaptive a;
            std::vector<uint32_t> res;
            for (uint32_t ty = 0; ty < th; ++ty) {
                for (uint32_t tx = 0; tx < tw; ++tx) {
                    const uint32_t x0 = tx << bits, y0 = ty << bits;
                    const uint32_t x1 = std::min(w, x0 + (1u << bits)), y1 = std::min(h, y0 + (1u << bits));
                    const unsigned left = tx > 0 ? (modes[size_t(ty) * tw + tx - 1] >> 8) & 0xf : 99;
                    const unsigned up = ty > 0 ? (modes[size_t(ty - 1) * tw + tx] >> 8) & 0xf : 99;
                    float best = 0;
                    unsigned pick = 11;
                    for (unsigned k = 0; k < count; ++k) {
                        const unsigned m = tried[k];
                        float cost = (m != left ? 1.5f : 0.0f) + (m != up ? 1.5f : 0.0f);
                        for (uint32_t y = std::max(y0, 1u); y < y1; ++y) {
                            for (uint32_t x = std::max(x0, 1u); x < x1; ++x) {
                                cost += a.of(_sub_pixels(p[size_t(y) * w + x], _predict_at(m, p, x, y, w)));
                            }
                        }
                        if (k == 0 || cost < best) {
                            best = cost;
                            pick = m;
                        }
                    }
                    modes[size_t(ty) * tw + tx] = 0xff000000u | pick << 8;
                    for (uint32_t y = std::max(y0, 1u); y < y1; ++y) {
                        for (uint32_t x = std::max(x0, 1u); x < x1; ++x) {
                            a.add(_sub_pixels(p[size_t(y) * w + x], _predict_at(pick, p, x, y, w)));
                        }
                    }
                    a.refresh();
                }
            }
            // the residuals
            res.resize(px.size());
            for (uint32_t y = 0; y < h; ++y) {
                for (uint32_t x = 0; x < w; ++x) {
                    uint32_t pred;
                    if (y == 0) {
                        pred = x == 0 ? 0xff000000u : p[x - 1];
                    } else if (x == 0) {
                        pred = p[size_t(y - 1) * w];
                    } else {
                        pred = _predict_at((modes[size_t(y >> bits) * tw + (x >> bits)] >> 8) & 0xf, p, x, y, w);
                    }
                    res[size_t(y) * w + x] = _sub_pixels(p[size_t(y) * w + x], pred);
                }
            }
            px.swap(res);
            b.put(1, 1);
            b.put(0, 2);
            b.put(bits - 2, 3);
            _stream(modes.data(), tw, th, b, false);
        }

        // The cross-color transform: per tile, green_to_red, green_to_blue
        // and red_to_blue whose channels cost least under what the tiles
        // before chose (the multipliers of the tile before and above tried
        // first, kept on a tie: the element image codes repeats cheaply)
        void _cross_color(std::vector<uint32_t>& px, uint32_t w, uint32_t h, Vp8lBitWriter& b) {
            const unsigned bits = _tile_bits(w, h, 5);
            const uint32_t tw = vp8l::div_round_up(w, bits), th = vp8l::div_round_up(h, bits);
            std::vector<uint32_t> elements(size_t(tw) * th);
            Adaptive a;
            std::vector<uint8_t> green, red, blue;
            for (uint32_t ty = 0; ty < th; ++ty) {
                for (uint32_t tx = 0; tx < tw; ++tx) {
                    const uint32_t x0 = tx << bits, y0 = ty << bits;
                    const uint32_t x1 = std::min(w, x0 + (1u << bits)), y1 = std::min(h, y0 + (1u << bits));
                    // the tile's channels, gathered once
                    green.clear();
                    red.clear();
                    blue.clear();
                    for (uint32_t y = y0; y < y1; ++y) {
                        const uint32_t* row = px.data() + size_t(y) * w;
                        for (uint32_t x = x0; x < x1; ++x) {
                            green.push_back(uint8_t(row[x] >> 8));
                            red.push_back(uint8_t(row[x] >> 16));
                            blue.push_back(uint8_t(row[x]));
                        }
                    }
                    const size_t n = green.size();
                    const uint32_t left = tx > 0 ? elements[size_t(ty) * tw + tx - 1] : 0xff000000u;
                    const uint32_t up = ty > 0 ? elements[size_t(ty - 1) * tw + tx] : 0xff000000u;
                    // the multiplier of one channel: of least cost among
                    // the neighbors', a coarse sweep and its refinement
                    auto search = [&](auto&& cost_of, int from_left, int from_up) {
                        int pick = from_left;
                        float best = cost_of(from_left) - 0.5f;
                        auto tryone = [&](int m) {
                            if (m < -128 || m > 127) {
                                return;
                            }
                            const float c = cost_of(m);
                            if (c < best) {
                                best = c;
                                pick = m;
                            }
                        };
                        if (from_up != from_left) {
                            tryone(from_up);
                        }
                        tryone(0);
                        const int step = _effort >= 75 ? 8 : 16;
                        for (int m = -128 + step / 2; m < 128; m += step) {
                            tryone(m);
                        }
                        for (int d = step / 2; d >= 1; d /= 2) {
                            const int c = pick;
                            tryone(c - d);
                            tryone(c + d);
                        }
                        return pick;
                    };
                    auto red_cost = [&](int m) {
                        float c = 0;
                        for (size_t i = 0; i < n; ++i) {
                            c += a.cost[2][uint8_t(red[i] - vp8l::color_delta(int8_t(m), int8_t(green[i])))];
                        }
                        return c;
                    };
                    const int g2r = search(red_cost, int8_t(left & 0xff), int8_t(up & 0xff));
                    auto blue_cost_g = [&](int m) {
                        float c = 0;
                        for (size_t i = 0; i < n; ++i) {
                            c += a.cost[0][uint8_t(blue[i] - vp8l::color_delta(int8_t(m), int8_t(green[i])))];
                        }
                        return c;
                    };
                    const int g2b = search(blue_cost_g, int8_t((left >> 8) & 0xff), int8_t((up >> 8) & 0xff));
                    for (size_t i = 0; i < n; ++i) {
                        blue[i] = uint8_t(blue[i] - vp8l::color_delta(int8_t(g2b), int8_t(green[i])));
                    }
                    auto blue_cost_r = [&](int m) {
                        float c = 0;
                        for (size_t i = 0; i < n; ++i) {
                            c += a.cost[0][uint8_t(blue[i] - vp8l::color_delta(int8_t(m), int8_t(red[i])))];
                        }
                        return c;
                    };
                    const int r2b = search(blue_cost_r, int8_t((left >> 16) & 0xff), int8_t((up >> 16) & 0xff));
                    elements[size_t(ty) * tw + tx] = 0xff000000u | uint32_t(uint8_t(r2b)) << 16 | uint32_t(uint8_t(g2b)) << 8 | uint8_t(g2r);
                    // the tile transformed: red by the green, blue by the
                    // green and the red as they were
                    for (uint32_t y = y0; y < y1; ++y) {
                        uint32_t* row = px.data() + size_t(y) * w;
                        for (uint32_t x = x0; x < x1; ++x) {
                            const uint32_t v = row[x];
                            const int8_t gr = int8_t((v >> 8) & 0xff);
                            const int8_t rd = int8_t((v >> 16) & 0xff);
                            const uint32_t nr = ((v >> 16) - uint32_t(vp8l::color_delta(int8_t(g2r), gr))) & 0xff;
                            const uint32_t nb = (v - uint32_t(vp8l::color_delta(int8_t(g2b), gr)) - uint32_t(vp8l::color_delta(int8_t(r2b), rd))) & 0xff;
                            row[x] = (v & 0xff00ff00u) | nr << 16 | nb;
                            a.add(row[x]);
                        }
                    }
                    a.refresh();
                }
            }
            b.put(1, 1);
            b.put(1, 2);
            b.put(bits - 2, 3);
            _stream(elements.data(), tw, th, b, false);
        }

        // The bits each symbol is expected to take: literals by channel,
        // the prefix codes of lengths and distances (their extra bits
        // apart), from a histogram, or flat before there is one
        struct SymbolCosts {
            float green[256], red[256], blue[256], alpha[256];
            float length[24], dist[40];
            float cache[2048];

            // a prefix code gives a symbol a whole bit at least, and none
            // when it is the code's only one
            static void from(const uint32_t* h, unsigned n, float* out) noexcept {
                uint64_t total = 0;
                unsigned used = 0;
                for (unsigned i = 0; i < n; ++i) {
                    total += h[i];
                    used += h[i] != 0;
                }
                const float lt = total ? float(std::log2(double(total))) : 0.0f;
                for (unsigned i = 0; i < n; ++i) {
                    out[i] = h[i] ? (used == 1 ? 0.0f : std::max(1.0f, lt - vp8l::log2_of(h[i]))) : lt + 2.0f;
                }
            }

            // the costs of the symbols a histogram counted (the green
            // alphabet shared by the literals' green, the lengths and the
            // cache), with flat guesses for the copies it has none of
            template<class H>
            void of(const H& h, unsigned cache_bits) noexcept {
                from(h.red, 256, red);
                from(h.blue, 256, blue);
                from(h.alpha, 256, alpha);
                from(h.dist, 40, dist);
                const unsigned n = unsigned(h.green.size());
                std::vector<float> g(n);
                from(h.green.data(), n, g.data());
                for (unsigned i = 0; i < 256; ++i) {
                    green[i] = g[i];
                }
                uint32_t copies = 0, dists = 0;
                for (unsigned i = 0; i < 24; ++i) {
                    length[i] = g[256 + i];
                    copies += h.green[256 + i];
                }
                for (unsigned i = 0; i < 40; ++i) {
                    dists += h.dist[i];
                }
                if (copies == 0) {
                    for (unsigned i = 0; i < 24; ++i) {
                        length[i] = 4.0f + 0.25f * float(i);
                    }
                }
                if (dists == 0) {
                    for (unsigned i = 0; i < 40; ++i) {
                        dist[i] = 5.0f + 0.15f * float(i);
                    }
                }
                for (unsigned i = 0; i < (cache_bits ? 1u << cache_bits : 0u); ++i) {
                    cache[i] = g[280 + i];
                }
            }

            SGCL_INLINE_HOT float literal(uint32_t v) const noexcept {
                return green[(v >> 8) & 0xff] + red[(v >> 16) & 0xff] + blue[v & 0xff] + alpha[v >> 24];
            }

            SGCL_INLINE_HOT float copy(uint32_t len, uint32_t dcode) const noexcept {
                uint32_t code, bits;
                unsigned extra;
                vp8l::prefix_of(len, code, extra, bits);
                float c = length[code] + float(extra);
                vp8l::prefix_of(dcode, code, extra, bits);
                return c + dist[code] + float(extra);
            }
        };

        // LZ77 over the pixels, in two steps: the longest copy at each pixel
        // (_matches), found once; then the cheapest path over literals and
        // copies (_backward_refs), once or twice as the cache asks.
        //
        // The longest copy at each pixel and its distance code: from a hash
        // chain over pairs of pixels, the near places VP8L codes cheaply
        // tried first; inside a long copy the pixels after its first take
        // what is left of it
        struct Matches {
            std::vector<uint16_t> lens;
            std::vector<uint32_t> dists;
            std::vector<uint32_t> dcodes;
        };

        void _matches(const uint32_t* px, uint32_t xsize, uint32_t ysize, Matches& mt) const {
            const size_t n = size_t(xsize) * ysize;
            if (n < 4) {
                return;
            }
            const unsigned hash_bits = n > (1u << 20) ? 18 : n > (1u << 16) ? 16 : 14;
            std::vector<int32_t> head(size_t(1) << hash_bits, -1);
            std::vector<int32_t> chain(n, -1);
            const unsigned depth = _effort >= 90 ? 64 : _effort >= 75 ? 48 : _effort >= 50 ? 24 : _effort >= 25 ? 10 : 3;
            const size_t window = 1048576 - 120;
            const uint32_t max_len = 4096;
            auto hash = [&](size_t i) {
                const uint64_t k = uint64_t(px[i]) << 32 | px[i + 1];
                return uint32_t((k * 0x9E3779B97F4A7C15ull) >> (64 - hash_bits));
            };
            auto insert = [&](size_t i) {
                if (i + 1 < n) {
                    const uint32_t hv = hash(i);
                    chain[i] = head[hv];
                    head[hv] = int32_t(i);
                }
            };
            auto match_len = [&](size_t i, size_t j, uint32_t limit) {
                uint32_t len = 0;
                while (len < limit && px[i + len] == px[j + len]) {
                    ++len;
                }
                return len;
            };
            std::vector<uint16_t>& lens = mt.lens;
            std::vector<uint32_t>& dists = mt.dists;
            lens.assign(n, 0);
            dists.assign(n, 0);
            for (size_t i = 0; i < n;) {
                uint32_t best_len = 0;
                size_t best_dist = 0;
                const uint32_t limit = uint32_t(std::min<size_t>(max_len, n - i));
                if (limit >= 3) {
                    // the near places VP8L codes cheaply first: left, up,
                    // two up, the diagonals (a hash chain of a run of equal
                    // pixels holds only the run's own recent positions)
                    const size_t near[6] = {1, size_t(xsize), 2 * size_t(xsize), size_t(xsize) - 1, size_t(xsize) + 1, 3 * size_t(xsize)};
                    for (size_t d : near) {
                        if (d >= 1 && d <= i) {
                            const uint32_t l = match_len(i, i - d, limit);
                            if (l > best_len) {
                                best_len = l;
                                best_dist = d;
                            }
                        }
                    }
                    if (best_len < limit && i + 1 < n) {
                        int32_t j = head[hash(i)];
                        unsigned left = depth;
                        while (j >= 0 && left-- > 0) {
                            const size_t d = i - size_t(j);
                            if (d > window) {
                                break;
                            }
                            if (px[size_t(j) + best_len] == px[i + best_len]) {
                                const uint32_t l = match_len(i, size_t(j), limit);
                                if (l > best_len) {
                                    best_len = l;
                                    best_dist = d;
                                    if (l == limit) {
                                        break;
                                    }
                                }
                            }
                            j = chain[size_t(j)];
                        }
                    }
                }
                insert(i);
                lens[i] = uint16_t(best_len);
                dists[i] = uint32_t(best_dist);
                if (best_len >= 32) {
                    // the rest of a long copy: what is left of it
                    for (uint32_t k = 1; k < best_len; ++k) {
                        lens[i + k] = uint16_t(best_len - k);
                        dists[i + k] = uint32_t(best_dist);
                        insert(i + k);
                    }
                    i += best_len;
                } else {
                    ++i;
                }
            }
            // the distance codes, once
            mt.dcodes.assign(n, 0);
            for (size_t i = 0; i < n; ++i) {
                if (lens[i] >= 1) {
                    mt.dcodes[i] = vp8l::distance_code(dists[i], xsize);
                }
            }
        }

        // The tokens: the cheapest path over literals (or cache hits) and
        // copies of the lengths the effort allows, by the costs of the
        // symbols (taken from the pixels coded one by one first, from the
        // path found after); the cache applied
        void _backward_refs(const uint32_t* px, uint32_t xsize, uint32_t ysize, const Matches& mt, unsigned cache_bits, std::vector<Vp8lToken>& tokens) const {
            const size_t n = size_t(xsize) * ysize;
            tokens.clear();
            tokens.reserve(n / 2 + 16);
            if (n < 4) {
                for (size_t i = 0; i < n; ++i) {
                    tokens.push_back({px[i], 0});
                }
                _apply_cache(px, tokens, cache_bits);
                return;
            }
            const std::vector<uint16_t>& lens = mt.lens;
            const std::vector<uint32_t>& dists = mt.dists;
            const std::vector<uint32_t>& dcodes = mt.dcodes;
            // whether each pixel is in the cache when it comes (every pixel
            // before it goes in, however it is coded) and where
            std::vector<int16_t> slot(n, -1);
            if (cache_bits) {
                std::vector<uint32_t> cache(size_t(1) << cache_bits, 0);
                std::vector<uint8_t> filled(size_t(1) << cache_bits, 0);
                for (size_t i = 0; i < n; ++i) {
                    const uint32_t s = vp8l::cache_slot(px[i], cache_bits);
                    if (filled[s] && cache[s] == px[i]) {
                        slot[i] = int16_t(s);
                    }
                    cache[s] = px[i];
                    filled[s] = 1;
                }
            }
            SymbolCosts costs;
            {
                // the first costs: every pixel a literal or a cache hit
                Histogram h(cache_bits);
                for (size_t i = 0; i < n; ++i) {
                    h.add(slot[i] >= 0 ? Vp8lToken{uint32_t(slot[i]), 0xFFFF} : Vp8lToken{px[i], 0}, xsize);
                }
                costs.of(h, cache_bits);
            }
            auto single = [&](size_t i) {
                return slot[i] >= 0 ? costs.cache[slot[i]] : costs.literal(px[i]);
            };
            const unsigned passes = _effort >= 75 ? 2 : 1;
            const uint32_t short_lengths = _effort >= 90 ? 32 : _effort >= 50 ? 16 : 0;
            std::vector<float> cost(n + 1);
            std::vector<uint16_t> step(n + 1);   // the length that reached i: 1 a literal
            // each pass's tokens kept when they cost less than the best so
            // far (the costs of a pass come from the one before, which may
            // lead away as well as closer)
            std::vector<Vp8lToken> best;
            float best_bits = 0;
            for (unsigned pass = 0; pass < passes; ++pass) {
                if (pass > 0) {
                    Histogram h(cache_bits);
                    _apply_cache(px, tokens, cache_bits);
                    for (const auto& t : tokens) {
                        h.add(t, xsize);
                    }
                    const float bits = h.bits();
                    if (pass == 1 || bits < best_bits) {
                        best_bits = bits;
                        best = tokens;
                    }
                    costs.of(h, cache_bits);
                    tokens.clear();
                }
                std::fill(cost.begin(), cost.end(), 3.0e38f);
                cost[0] = 0;
                for (size_t i = 0; i < n; ++i) {
                    const float base = cost[i];
                    const float lit = base + single(i);
                    if (lit < cost[i + 1]) {
                        cost[i + 1] = lit;
                        step[i + 1] = 1;
                    }
                    const uint32_t L = lens[i];
                    if (L < 3) {
                        continue;
                    }
                    // the whole copy, and the shorter ones the effort tries
                    const float full = base + costs.copy(L, dcodes[i]);
                    if (full < cost[i + L]) {
                        cost[i + L] = full;
                        step[i + L] = uint16_t(L);
                    }
                    const uint32_t upto = std::min(L - 1, short_lengths);
                    for (uint32_t k = 3; k <= upto; ++k) {
                        const float c = base + costs.copy(k, dcodes[i]);
                        if (c < cost[i + k]) {
                            cost[i + k] = c;
                            step[i + k] = uint16_t(k);
                        }
                    }
                }
                // the path back from the end, then forward
                std::vector<uint16_t> path;
                for (size_t i = n; i > 0;) {
                    path.push_back(step[i]);
                    i -= step[i];
                }
                size_t i = 0;
                for (size_t k = path.size(); k-- > 0;) {
                    const uint16_t len = path[k];
                    if (len == 1) {
                        tokens.push_back({px[i], 0});
                    } else {
                        tokens.push_back({dists[i], len});
                    }
                    i += len;
                }
            }
            _apply_cache(px, tokens, cache_bits);
            if (passes > 1) {
                Histogram h(cache_bits);
                for (const auto& t : tokens) {
                    h.add(t, xsize);
                }
                if (h.bits() > best_bits) {
                    tokens.swap(best);
                }
            }
        }

        // The tokens with literals in the cache made cache indices
        static void _apply_cache(const uint32_t* px, std::vector<Vp8lToken>& tokens, unsigned bits) {
            if (bits == 0) {
                return;
            }
            std::vector<uint32_t> cache(size_t(1) << bits, 0);
            std::vector<uint8_t> filled(size_t(1) << bits, 0);
            size_t pos = 0;
            for (auto& t : tokens) {
                if (t.length == 0) {
                    const uint32_t v = t.value;
                    const uint32_t s = vp8l::cache_slot(v, bits);
                    if (filled[s] && cache[s] == v) {
                        t.value = s;
                        t.length = 0xFFFF;
                    }
                    cache[s] = v;
                    filled[s] = 1;
                    ++pos;
                } else {
                    for (uint32_t k = 0; k < t.length; ++k) {
                        const uint32_t v = px[pos + k];
                        const uint32_t s = vp8l::cache_slot(v, bits);
                        cache[s] = v;
                        filled[s] = 1;
                    }
                    pos += t.length;
                }
            }
        }

        // The histograms of the five codes over tokens
        struct Histogram {
            std::vector<uint32_t> green;   // 256 + 24 + cache
            uint32_t red[256];
            uint32_t blue[256];
            uint32_t alpha[256];
            uint32_t dist[40];
            uint64_t extra = 0;   // the extra bits of lengths and distances

            explicit Histogram(unsigned cache_bits)
            : green(256 + 24 + (cache_bits ? 1u << cache_bits : 0u), 0) {
                std::memset(red, 0, sizeof(red));
                std::memset(blue, 0, sizeof(blue));
                std::memset(alpha, 0, sizeof(alpha));
                std::memset(dist, 0, sizeof(dist));
            }

            void add(const Vp8lToken& t, uint32_t xsize) noexcept {
                if (t.length == 0) {
                    ++green[(t.value >> 8) & 0xff];
                    ++red[(t.value >> 16) & 0xff];
                    ++blue[t.value & 0xff];
                    ++alpha[t.value >> 24];
                } else if (t.length == 0xFFFF) {
                    ++green[280 + t.value];
                } else {
                    uint32_t code, bits;
                    unsigned extra;
                    vp8l::prefix_of(t.length, code, extra, bits);
                    ++green[256 + code];
                    this->extra += extra;
                    const uint32_t dcode = vp8l::distance_code(t.value, xsize);
                    vp8l::prefix_of(dcode, code, extra, bits);
                    ++dist[code];
                    this->extra += extra;
                }
            }

            float bits() const noexcept {
                return vp8l::entropy_bits(green.data(), unsigned(green.size())) + vp8l::entropy_bits(red, 256) + vp8l::entropy_bits(blue, 256) +
                       vp8l::entropy_bits(alpha, 256) + vp8l::entropy_bits(dist, 40) + float(extra);
            }

            void merge(const Histogram& o) noexcept {
                for (size_t i = 0; i < green.size(); ++i) {
                    green[i] += o.green[i];
                }
                for (unsigned i = 0; i < 256; ++i) {
                    red[i] += o.red[i];
                    blue[i] += o.blue[i];
                    alpha[i] += o.alpha[i];
                }
                for (unsigned i = 0; i < 40; ++i) {
                    dist[i] += o.dist[i];
                }
                extra += o.extra;
            }

            void clear() noexcept {
                std::fill(green.begin(), green.end(), 0u);
                std::memset(red, 0, sizeof(red));
                std::memset(blue, 0, sizeof(blue));
                std::memset(alpha, 0, sizeof(alpha));
                std::memset(dist, 0, sizeof(dist));
                extra = 0;
            }

            uint32_t symbols() const noexcept {
                uint32_t n = 0;
                for (uint32_t c : green) {
                    n += c;
                }
                return n;
            }
        };

        // The groups of codes of the main image (the entropy image, RFC 9649
        // §3.7.2.2): its tiles' histograms put into bins by what their
        // literals cost a symbol, then the bins merged two at a time while a
        // merge saves bits (the codes' cost counted); the group of each
        // tile, the number of groups (1: no entropy image is worth it)
        unsigned _groups(const std::vector<Vp8lToken>& tokens, uint32_t xsize, uint32_t ysize, unsigned cache_bits, unsigned bits,
                         std::vector<uint16_t>& group_of) const {
            const uint32_t tw = vp8l::div_round_up(xsize, bits), th = vp8l::div_round_up(ysize, bits);
            constexpr unsigned Bins = 64;
            std::vector<Histogram> bins(Bins, Histogram(cache_bits));
            std::vector<Histogram> band(tw, Histogram(cache_bits));
            std::vector<uint8_t> bin_of(size_t(tw) * th, 0);
            Histogram all(cache_bits);
            uint32_t x = 0, y = 0;
            size_t k = 0;
            auto close_band = [&](uint32_t ty) {
                for (uint32_t tx = 0; tx < tw; ++tx) {
                    Histogram& h = band[tx];
                    const uint32_t n = h.symbols();
                    unsigned key = 0;
                    if (n) {
                        auto per = [&](const uint32_t* c, unsigned m) {
                            const float e = vp8l::entropy_bits(c, m) / float(n);
                            return std::min(3u, unsigned(e * 0.5f));
                        };
                        key = per(h.green.data(), 256) * 16 + per(h.red, 256) * 4 + per(h.blue, 256);
                    }
                    bin_of[size_t(ty) * tw + tx] = uint8_t(key);
                    bins[key].merge(h);
                    h.clear();
                }
            };
            uint32_t band_row = 0;
            while (k < tokens.size()) {
                const Vp8lToken& t = tokens[k++];
                const uint32_t ty = y >> bits;
                if (ty != band_row) {
                    close_band(band_row);
                    band_row = ty;
                }
                band[x >> bits].add(t, xsize);
                all.add(t, xsize);
                const size_t len = t.length == 0 || t.length == 0xFFFF ? 1 : t.length;
                x += uint32_t(len);
                while (x >= xsize) {
                    x -= xsize;
                    ++y;
                }
            }
            close_band(band_row);
            // the bins in use, merged by the best saving first
            std::vector<int> live;
            std::vector<float> cost(Bins, 0);
            for (unsigned i = 0; i < Bins; ++i) {
                if (bins[i].symbols()) {
                    live.push_back(int(i));
                    cost[i] = bins[i].bits();
                }
            }
            std::vector<int> parent(Bins);
            for (unsigned i = 0; i < Bins; ++i) {
                parent[i] = int(i);
            }
            // the saving of merging a and b (positive: fewer bits)
            auto saving = [&](int a, int b) {
                Histogram m = bins[size_t(a)];
                m.merge(bins[size_t(b)]);
                return cost[size_t(a)] + cost[size_t(b)] - m.bits();
            };
            std::vector<float> gain(size_t(Bins) * Bins, 0);
            for (size_t i = 0; i < live.size(); ++i) {
                for (size_t j = i + 1; j < live.size(); ++j) {
                    gain[size_t(live[i]) * Bins + size_t(live[j])] = saving(live[i], live[j]);
                }
            }
            while (live.size() > 1) {
                float best = 0;
                size_t bi = 0, bj = 0;
                for (size_t i = 0; i < live.size(); ++i) {
                    for (size_t j = i + 1; j < live.size(); ++j) {
                        const float g = gain[size_t(live[i]) * Bins + size_t(live[j])];
                        if (g > best) {
                            best = g;
                            bi = i;
                            bj = j;
                        }
                    }
                }
                if (best <= 0) {
                    break;
                }
                const int a = live[bi], b = live[bj];
                bins[size_t(a)].merge(bins[size_t(b)]);
                cost[size_t(a)] = bins[size_t(a)].bits();
                parent[size_t(b)] = a;
                live.erase(live.begin() + ptrdiff_t(bj));
                for (size_t i = 0; i < live.size(); ++i) {
                    const int o = live[i];
                    if (o == a) {
                        continue;
                    }
                    const int lo = std::min(a, o), hi = std::max(a, o);
                    gain[size_t(lo) * Bins + size_t(hi)] = saving(lo, hi);
                }
            }
            // worth it: the groups' bits and their codes against one group
            float grouped = 0;
            for (int i : live) {
                grouped += cost[size_t(i)];
            }
            if (live.size() <= 1 || grouped + float(tw * th) * 0.5f >= all.bits()) {
                return 1;
            }
            std::vector<int> index(Bins, -1);
            for (size_t i = 0; i < live.size(); ++i) {
                index[size_t(live[i])] = int(i);
            }
            auto root = [&](int i) {
                while (parent[size_t(i)] != i) {
                    i = parent[size_t(i)];
                }
                return i;
            };
            group_of.resize(size_t(tw) * th);
            for (size_t t = 0; t < group_of.size(); ++t) {
                const int r = root(bin_of[t]);
                group_of[t] = uint16_t(index[size_t(r)] < 0 ? 0 : index[size_t(r)]);
            }
            return unsigned(live.size());
        }

        // An entropy-coded image: the cache, the meta codes (none here),
        // the five codes and the tokens
        void _stream(const uint32_t* px, uint32_t xsize, uint32_t ysize, Vp8lBitWriter& b, bool main) {
            // the copies found with no cache; the cache size whose symbols
            // cost least over them; then the copies found again knowing
            // which pixels the cache holds, kept when they cost less
            std::vector<Vp8lToken> tokens;
            Matches mt;
            _matches(px, xsize, ysize, mt);
            _backward_refs(px, xsize, ysize, mt, 0, tokens);
            unsigned cache_bits = 0;
            if (size_t(xsize) * ysize >= 64) {
                Histogram h0(0);
                for (const auto& t : tokens) {
                    h0.add(t, xsize);
                }
                float best = h0.bits();
                const unsigned from = _effort >= 75 ? 1 : _effort >= 25 ? 4 : 6;
                const unsigned step = _effort >= 75 ? 1 : 2;
                std::vector<Vp8lToken> trial;
                for (unsigned bits = from; bits <= 10; bits += step) {
                    trial = tokens;
                    _apply_cache(px, trial, bits);
                    Histogram h(bits);
                    for (const auto& t : trial) {
                        h.add(t, xsize);
                    }
                    const float c = h.bits();
                    if (c < best) {
                        best = c;
                        cache_bits = bits;
                    }
                }
                if (cache_bits) {
                    _backward_refs(px, xsize, ysize, mt, cache_bits, trial);
                    Histogram h(cache_bits);
                    for (const auto& t : trial) {
                        h.add(t, xsize);
                    }
                    if (h.bits() < best) {
                        tokens.swap(trial);
                    } else {
                        _apply_cache(px, tokens, cache_bits);
                    }
                }
            }
            if (cache_bits) {
                b.put(1, 1);
                b.put(cache_bits, 4);
            } else {
                b.put(0, 1);
            }
            // the groups of codes: one, or an entropy image of the main
            // image's tiles at the higher efforts
            std::vector<uint16_t> group_of;
            unsigned groups = 1;
            unsigned group_bits = 0;
            if (main && _effort >= 25 && size_t(xsize) * ysize >= 16384) {
                group_bits = _effort >= 75 ? 4 : 5;
                while (vp8l::div_round_up(xsize, group_bits) * vp8l::div_round_up(ysize, group_bits) > 16384) {
                    ++group_bits;
                }
                groups = _groups(tokens, xsize, ysize, cache_bits, group_bits, group_of);
            }
            const uint32_t gw = groups > 1 ? vp8l::div_round_up(xsize, group_bits) : 1;
            if (main) {
                if (groups > 1) {
                    b.put(1, 1);
                    b.put(group_bits - 2, 3);
                    const uint32_t gh = vp8l::div_round_up(ysize, group_bits);
                    std::vector<uint32_t> image(size_t(gw) * gh);
                    for (size_t i = 0; i < image.size(); ++i) {
                        image[i] = 0xff000000u | uint32_t(group_of[i] >> 8) << 16 | uint32_t(group_of[i] & 0xff) << 8;
                    }
                    _stream(image.data(), gw, gh, b, false);
                } else {
                    b.put(0, 1);
                }
            }
            std::vector<Histogram> hs(groups, Histogram(cache_bits));
            {
                uint32_t x = 0, y = 0;
                for (const auto& t : tokens) {
                    const unsigned g = groups > 1 ? group_of[size_t(y >> group_bits) * gw + (x >> group_bits)] : 0;
                    hs[g].add(t, xsize);
                    const uint32_t len = t.length == 0 || t.length == 0xFFFF ? 1u : t.length;
                    x += len;
                    while (x >= xsize) {
                        x -= xsize;
                        ++y;
                    }
                }
            }
            std::vector<std::array<Vp8lCode, 5>> codes(groups);
            std::vector<std::array<bool, 5>> single(groups);
            for (unsigned g = 0; g < groups; ++g) {
                const Histogram& h = hs[g];
                codes[g][0].make(h.green.data(), unsigned(h.green.size()), 15);
                codes[g][1].make(h.red, 256, 15);
                codes[g][2].make(h.blue, 256, 15);
                codes[g][3].make(h.alpha, 256, 15);
                codes[g][4].make(h.dist, 40, 15);
                for (unsigned k = 0; k < 5; ++k) {
                    _write_code(codes[g][k], b);
                    // a code of one symbol takes no bits
                    single[g][k] = codes[g][k].used() <= 1;
                }
            }
            uint32_t x = 0, y = 0;
            for (const auto& t : tokens) {
                const unsigned g = groups > 1 ? group_of[size_t(y >> group_bits) * gw + (x >> group_bits)] : 0;
                const auto& c = codes[g];
                auto sym = [&](unsigned k, uint32_t s) {
                    if (!single[g][k]) {
                        b.put(c[k].code[s], c[k].length[s]);
                    }
                };
                uint32_t len = 1;
                if (t.length == 0) {
                    sym(0, (t.value >> 8) & 0xff);
                    sym(1, (t.value >> 16) & 0xff);
                    sym(2, t.value & 0xff);
                    sym(3, t.value >> 24);
                } else if (t.length == 0xFFFF) {
                    sym(0, 280 + t.value);
                } else {
                    uint32_t code, bits;
                    unsigned extra;
                    vp8l::prefix_of(t.length, code, extra, bits);
                    sym(0, 256 + code);
                    if (extra) {
                        b.put(bits, extra);
                    }
                    vp8l::prefix_of(vp8l::distance_code(t.value, xsize), code, extra, bits);
                    sym(4, code);
                    if (extra) {
                        b.put(bits, extra);
                    }
                    len = t.length;
                }
                x += len;
                while (x >= xsize) {
                    x -= xsize;
                    ++y;
                }
            }
        }

        // A prefix code: the simple form for one or two symbols below 256,
        // else the code lengths through the code of code lengths
        static void _write_code(const Vp8lCode& c, Vp8lBitWriter& b) {
            const unsigned n = unsigned(c.length.size());
            unsigned symbols[2] = {0, 0};
            unsigned used = 0;
            for (unsigned i = 0; i < n; ++i) {
                if (c.length[i]) {
                    if (used < 2) {
                        symbols[used] = i;
                    }
                    ++used;
                }
            }
            if (used <= 2 && symbols[0] < 256 && symbols[1] < 256) {
                b.put(1, 1);
                b.put(used == 2 ? 1 : 0, 1);
                if (symbols[0] < 2) {
                    b.put(0, 1);
                    b.put(symbols[0], 1);
                } else {
                    b.put(1, 1);
                    b.put(symbols[0], 8);
                }
                if (used == 2) {
                    b.put(symbols[1], 8);
                }
                return;
            }
            b.put(0, 1);
            // the lengths as tokens: 0..15, 16 (repeat the last nonzero 3..6
            // times), 17 (zeros 3..10), 18 (zeros 11..138)
            struct Token {
                uint8_t symbol;
                uint8_t extra;
            };
            std::vector<Token> tokens;
            unsigned prev = 8;
            for (unsigned i = 0; i < n;) {
                const unsigned v = c.length[i];
                unsigned run = 1;
                while (i + run < n && c.length[i + run] == v) {
                    ++run;
                }
                i += run;
                if (v == 0) {
                    while (run >= 11) {
                        const unsigned k = std::min(run, 138u);
                        tokens.push_back({18, uint8_t(k - 11)});
                        run -= k;
                    }
                    if (run >= 3) {
                        tokens.push_back({17, uint8_t(run - 3)});
                        run = 0;
                    }
                    while (run-- > 0) {
                        tokens.push_back({0, 0});
                    }
                    continue;
                }
                if (v != prev) {
                    tokens.push_back({uint8_t(v), 0});
                    prev = v;
                    --run;
                }
                while (run >= 3) {
                    const unsigned k = std::min(run, 6u);
                    tokens.push_back({16, uint8_t(k - 3)});
                    run -= k;
                }
                while (run-- > 0) {
                    tokens.push_back({uint8_t(v), 0});
                }
            }
            uint32_t counts[19] = {};
            for (const auto& t : tokens) {
                ++counts[t.symbol];
            }
            Vp8lCode cl;
            cl.make(counts, 19, 7);
            unsigned last = 4;
            for (unsigned i = 0; i < 19; ++i) {
                if (cl.length[vp8l::CodeLengthOrder[i]]) {
                    last = std::max(last, i + 1);
                }
            }
            b.put(last - 4, 4);
            for (unsigned i = 0; i < last; ++i) {
                b.put(cl.length[vp8l::CodeLengthOrder[i]], 3);
            }
            b.put(0, 1);   // max_symbol: the whole alphabet
            const bool one = cl.used() <= 1;
            for (const auto& t : tokens) {
                if (!one) {
                    b.put(cl.code[t.symbol], cl.length[t.symbol]);
                }
                if (t.symbol == 16) {
                    b.put(t.extra, 2);
                } else if (t.symbol == 17) {
                    b.put(t.extra, 3);
                } else if (t.symbol == 18) {
                    b.put(t.extra, 7);
                }
            }
        }

        int _effort = 75;
    };
}
