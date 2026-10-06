//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "pixels.h"
#include "simd.h"
#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace sgcl::codec::detail {
    // Color quantization for the formats of a palette (GIF; later others):
    // rows of "shown" pixels, each a word 0xFFBBGGRR for an opaque color
    // and 0 for a pixel the palette leaves transparent, reduced to at most
    // `colors` palette entries (the transparent one among them when a
    // pixel needs it) and mapped to indices.
    //
    // An image of no more colors than the palette holds is written as it
    // is: its colors in the order they first come, found through a small
    // hash table in one pass that keeps every pixel's index (a byte a
    // pixel). Any other goes through median cut (Heckbert 1982): a
    // histogram of 5 bits a channel, the box of the most pixels split at
    // its median along its longest side for the first three quarters of
    // the palette, then the box of the most pixels times its volume (the
    // order of Leptonica's modified median cut, which keeps the large
    // smooth areas and then the spread-out colors), each box shrunk to the
    // cells it holds after a split; an entry is the mean of the colors of
    // its box. A pixel maps to the entry nearest to it (squared RGB
    // distance), searched among the few entries that can be nearest in its
    // cell of 5 bits a channel and kept per color met. With dithering,
    // Floyd–Steinberg from left to right, the errors in sixteenths, a
    // transparent pixel taking and passing none; each color with its error
    // goes to the entry nearest to the center of its cell of 6 bits a
    // channel (the error carried on makes up for the distance).

    // The shown form of a pixel: alpha below 128 transparent (0), else
    // opaque with its color
    SGCL_INLINE_HOT uint32_t shown(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept {
        const uint32_t mask = 0u - uint32_t(a >> 7);
        return (0xFF000000u | uint32_t(b) << 16 | uint32_t(g) << 8 | r) & mask;
    }

    // A row of any pixel format in the shown form; scratch holds a row of
    // rgba8 (4 bytes a pixel) for the formats converted through it
    inline void shown_row(const uint8_t* src, pixel_format f, uint32_t* out, size_t n, uint8_t* scratch) noexcept {
        if (f == pixel_format::rgb8) {
            for (size_t x = 0; x < n; ++x) {
                out[x] = 0xFF000000u | uint32_t(src[3 * x + 2]) << 16 | uint32_t(src[3 * x + 1]) << 8 | src[3 * x];
            }
            return;
        }
        const uint8_t* rgba = src;
        if (f != pixel_format::rgba8) {
            converter(f, pixel_format::rgba8)(reinterpret_cast<const std::byte*>(src), reinterpret_cast<std::byte*>(scratch), n);
            rgba = scratch;
        }
        for (size_t x = 0; x < n; ++x) {
            out[x] = shown(rgba[4 * x], rgba[4 * x + 1], rgba[4 * x + 2], rgba[4 * x + 3]);
        }
    }

    struct Palette {
        uint8_t rgb[256][3] = {};
        unsigned size = 0;          // the colors, not counting the transparent entry
        bool transparent = false;   // entry `size` is the transparent one

        // Every entry, the transparent one with them
        SGCL_INLINE_HOT unsigned entries() const noexcept {
            return size + (transparent ? 1u : 0u);
        }
    };

    class Quantizer {
    public:
        // colors: 2..256, the entries of the palette, the transparent one
        // counted
        SGCL_INLINE_HOT Quantizer(unsigned colors, bool dither) noexcept
        : _colors(colors), _dither(dither) {
        }

        // The palette of the rows: rows(y) gives row y of width w in the
        // shown form, read once or twice
        template<class Rows>
        void build(Rows&& rows, uint32_t w, uint32_t h) noexcept {
            _p = Palette();
            _width = w;
            if (_exact(rows, w, h)) {
                _mode = Mode::exact;
                return;
            }
            _median_cut(rows, w, h);
            _cells.reset(new uint32_t[Cells]);
            std::fill_n(_cells.get(), Cells, Unmet);
            _used = 0;
            _seen_key.reset(new uint32_t[1u << SeenBits]());
            _seen_index.reset(new uint8_t[1u << SeenBits]);
            if (_dither) {
                _mode = Mode::dither;
                _fine.reset(new uint16_t[FineCells]);
                std::fill_n(_fine.get(), FineCells, uint16_t(0xFFFF));
                _err.reset(new int16_t[2 * 3 * (size_t(w) + 2)]());
                _cur = _err.get();
                _next = _cur + 3 * (size_t(w) + 2);
            } else {
                _mode = Mode::nearest;
            }
        }

        SGCL_INLINE_HOT const Palette& palette() const noexcept {
            return _p;
        }

        // Whether the palette holds every color of the image as it is
        SGCL_INLINE_HOT bool exact() const noexcept {
            return _mode == Mode::exact;
        }

        // The indices of row y of an exact palette, kept from the pass
        // that found its colors
        SGCL_INLINE_HOT const uint8_t* stored(uint32_t y) const noexcept {
            return _indices.get() + size_t(y) * _width;
        }

        // The indices of the next row of a palette not exact (rows in
        // order, from the first)
        void map_row(const uint32_t* in, uint8_t* out) noexcept {
            const uint32_t w = _width;
            const uint8_t clear = uint8_t(_p.size);
            switch (_mode) {
                case Mode::exact:
                    break;
                case Mode::nearest: {
                    for (uint32_t x = 0; x < w; ++x) {
                        const uint32_t v = in[x];
                        out[x] = v ? _nearest(uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16)) : clear;
                    }
                    break;
                }
                case Mode::dither:
                    _dither_row(in, out);
                    break;
            }
        }

    private:
        enum class Mode : uint8_t { exact, nearest, dither };

        static constexpr uint32_t TableSize = 1024;   // the exact colors' table: 256 at most, a quarter full
        static constexpr uint32_t Cells = 32768;      // the histogram: 5 bits a channel
        static constexpr uint32_t Unmet = UINT32_MAX; // a cell whose candidates are not found yet
        static constexpr unsigned SeenBits = 17;      // the colors met: 128 Ki slots, one color each
        static constexpr uint32_t FineCells = 262144; // dithering's cells: 6 bits a channel

        SGCL_INLINE_HOT static uint32_t _slot(uint32_t v) noexcept {
            return (v * 2654435761u) >> 22;
        }

        // The slot of color v in the exact table: where it is, or the
        // empty one where it goes
        SGCL_INLINE_HOT uint32_t _find(uint32_t v) const noexcept {
            uint32_t s = _slot(v);
            while (_table[s] != 0 && _table[s] != v) {
                s = (s + 1) & (TableSize - 1);
            }
            return s;
        }

        // The image's own colors, when they fit the palette, and the index
        // of every pixel, kept (a byte a pixel): the transparent pixels
        // first as 255, which no color is then (256 colors leave no room
        // for transparency), made the index after the colors at the end
        template<class Rows>
        bool _exact(Rows& rows, uint32_t w, uint32_t h) noexcept {
            std::memset(_table, 0, sizeof(_table));
            _indices.reset(new uint8_t[size_t(w) * h]);
            unsigned count = 0;
            bool transparent = false;
            for (uint32_t y = 0; y < h; ++y) {
                const uint32_t* row = rows(y);
                uint8_t* out = _indices.get() + size_t(y) * w;
                for (uint32_t x = 0; x < w; ++x) {
                    // every pixel through the table, no test of a run: the
                    // runs of a dithered image are too short to predict
                    const uint32_t v = row[x];
                    uint32_t s = _slot(v);
                    uint32_t k;
                    while ((k = _table[s]) != v && k != 0) {
                        s = (s + 1) & (TableSize - 1);
                    }
                    if (k == 0) [[unlikely]] {
                        if (v == 0) {
                            transparent = true;
                            out[x] = 255;
                            continue;
                        }
                        if (count == _colors) {
                            _indices.reset();
                            return false;
                        }
                        _table[s] = v;
                        _table_index[s] = uint8_t(count);
                        _p.rgb[count][0] = uint8_t(v);
                        _p.rgb[count][1] = uint8_t(v >> 8);
                        _p.rgb[count][2] = uint8_t(v >> 16);
                        ++count;
                    }
                    out[x] = _table_index[s];
                }
            }
            if (count + (transparent ? 1u : 0u) > _colors) {
                _indices.reset();
                return false;
            }
            _p.size = count;
            _p.transparent = transparent;
            if (transparent && count != 255) {
                const uint8_t clear = uint8_t(count);
                uint8_t* p = _indices.get();
                const size_t n = size_t(w) * h;
                for (size_t i = 0; i < n; ++i) {
                    p[i] = p[i] == 255 ? clear : p[i];
                }
            }
            return true;
        }

        struct Candidate {
            uint32_t near;   // the least squared distance from the entry to the cell
            uint8_t index;
        };

        struct Box {
            uint8_t lo[3];
            uint8_t hi[3];
            uint64_t count;
            uint64_t volume() const noexcept {
                return uint64_t(hi[0] - lo[0] + 1) * (hi[1] - lo[1] + 1) * (hi[2] - lo[2] + 1);
            }
            bool single() const noexcept {
                return lo[0] == hi[0] && lo[1] == hi[1] && lo[2] == hi[2];
            }
        };

        SGCL_INLINE_HOT static uint32_t _cell(unsigned r, unsigned g, unsigned b) noexcept {
            return r << 10 | g << 5 | b;
        }

        // The box shrunk to the cells of the histogram it holds, its count
        // summed; false for a box of none
        bool _shrink(Box& b) const noexcept {
            uint8_t lo[3] = {31, 31, 31}, hi[3] = {0, 0, 0};
            uint64_t count = 0;
            for (unsigned r = b.lo[0]; r <= b.hi[0]; ++r) {
                for (unsigned g = b.lo[1]; g <= b.hi[1]; ++g) {
                    const uint32_t* c = _hist.get() + _cell(r, g, 0);
                    for (unsigned bl = b.lo[2]; bl <= b.hi[2]; ++bl) {
                        if (const uint32_t n = c[bl]) {
                            count += n;
                            lo[0] = std::min<uint8_t>(lo[0], uint8_t(r));
                            hi[0] = std::max<uint8_t>(hi[0], uint8_t(r));
                            lo[1] = std::min<uint8_t>(lo[1], uint8_t(g));
                            hi[1] = std::max<uint8_t>(hi[1], uint8_t(g));
                            lo[2] = std::min<uint8_t>(lo[2], uint8_t(bl));
                            hi[2] = std::max<uint8_t>(hi[2], uint8_t(bl));
                        }
                    }
                }
            }
            if (count == 0) {
                return false;
            }
            std::memcpy(b.lo, lo, 3);
            std::memcpy(b.hi, hi, 3);
            b.count = count;
            return true;
        }

        // Splits box b at the median of its longest side into b and c;
        // false when it cannot be split (one cell)
        bool _split(Box& b, Box& c) const noexcept {
            if (b.single()) {
                return false;
            }
            unsigned axis = 0;
            for (unsigned a = 1; a < 3; ++a) {
                if (b.hi[a] - b.lo[a] > b.hi[axis] - b.lo[axis]) {
                    axis = a;
                }
            }
            // the pixels of each plane across the axis
            uint64_t plane[32] = {};
            for (unsigned r = b.lo[0]; r <= b.hi[0]; ++r) {
                for (unsigned g = b.lo[1]; g <= b.hi[1]; ++g) {
                    const uint32_t* cells = _hist.get() + _cell(r, g, 0);
                    for (unsigned bl = b.lo[2]; bl <= b.hi[2]; ++bl) {
                        const unsigned at = axis == 0 ? r : axis == 1 ? g : bl;
                        plane[at] += cells[bl];
                    }
                }
            }
            // the last plane of the lower half: where half the pixels are
            // reached, short of the last plane so that both halves hold some
            uint64_t sum = 0;
            unsigned cut = b.lo[axis];
            for (unsigned at = b.lo[axis]; at < b.hi[axis]; ++at) {
                sum += plane[at];
                cut = at;
                if (2 * sum >= b.count) {
                    break;
                }
            }
            c = b;
            b.hi[axis] = uint8_t(cut);
            c.lo[axis] = uint8_t(cut + 1);
            const bool lower = _shrink(b);
            const bool upper = _shrink(c);
            if (!lower) {
                b = c;
                return false;
            }
            return upper;
        }

        template<class Rows>
        void _median_cut(Rows& rows, uint32_t w, uint32_t h) noexcept {
            // the histogram, one more cell for the transparent pixels, and
            // the sums of each cell's colors
            _hist.reset(new uint32_t[Cells + 1]());
            std::unique_ptr<uint32_t[]> sums(new uint32_t[3 * size_t(Cells + 1)]());
            // a cell's sum may pass 32 bits past 2^24 pixels of one cell:
            // such images fold the sums into 64 bits a strip at a time
            std::unique_ptr<uint64_t[]> wide;
            const bool big = uint64_t(w) * h > (1u << 24);
            if (big) {
                wide.reset(new uint64_t[3 * size_t(Cells + 1)]());
            }
            uint64_t since = 0;
            for (uint32_t y = 0; y < h; ++y) {
                const uint32_t* row = rows(y);
                for (uint32_t x = 0; x < w; ++x) {
                    const uint32_t v = row[x];
                    const uint32_t r = v & 0xFF, g = v >> 8 & 0xFF, b = v >> 16 & 0xFF;
                    const uint32_t opaque = 0u - (v >> 31);
                    const uint32_t cell = (_cell(r >> 3, g >> 3, b >> 3) & opaque) | (Cells & ~opaque);
                    ++_hist[cell];
                    uint32_t* s = sums.get() + 3 * size_t(cell);
                    s[0] += r;
                    s[1] += g;
                    s[2] += b;
                }
                since += w;
                if (big && since >= (1u << 23)) {
                    for (size_t i = 0; i < 3 * size_t(Cells + 1); ++i) {
                        wide[i] += sums[i];
                    }
                    std::memset(sums.get(), 0, 3 * size_t(Cells + 1) * sizeof(uint32_t));
                    since = 0;
                }
            }
            auto sum_of = [&](uint32_t cell, unsigned ch) {
                return uint64_t(sums[3 * size_t(cell) + ch]) + (big ? wide[3 * size_t(cell) + ch] : 0);
            };
            _p.transparent = _hist[Cells] != 0;
            const unsigned limit = _colors - (_p.transparent ? 1u : 0u);

            Box boxes[256];
            unsigned n = 0;
            Box all{{0, 0, 0}, {31, 31, 31}, 0};
            if (_shrink(all)) {
                boxes[n++] = all;
            }
            const unsigned by_count = std::max(1u, limit * 3 / 4);
            while (n > 0 && n < limit) {
                // the box to split: the most pixels, then the most pixels
                // times volume; boxes of one cell never
                int pick = -1;
                uint64_t best = 0;
                for (unsigned i = 0; i < n; ++i) {
                    if (boxes[i].single()) {
                        continue;
                    }
                    const uint64_t score = n < by_count ? boxes[i].count : boxes[i].count * boxes[i].volume();
                    if (pick < 0 || score > best) {
                        pick = int(i);
                        best = score;
                    }
                }
                if (pick < 0) {
                    break;
                }
                Box c;
                if (_split(boxes[pick], c)) {
                    boxes[n++] = c;
                }
            }
            for (unsigned i = 0; i < n; ++i) {
                const Box& b = boxes[i];
                uint64_t total[3] = {0, 0, 0};
                for (unsigned r = b.lo[0]; r <= b.hi[0]; ++r) {
                    for (unsigned g = b.lo[1]; g <= b.hi[1]; ++g) {
                        for (unsigned bl = b.lo[2]; bl <= b.hi[2]; ++bl) {
                            const uint32_t cell = _cell(r, g, bl);
                            for (unsigned ch = 0; ch < 3; ++ch) {
                                total[ch] += sum_of(cell, ch);
                            }
                        }
                    }
                }
                for (unsigned ch = 0; ch < 3; ++ch) {
                    _p.rgb[i][ch] = uint8_t((total[ch] + b.count / 2) / b.count);
                }
            }
            _p.size = n;
            _hist.reset();
        }

        // The entry nearest to a color (least squared RGB distance, the
        // lowest index of equals): among the candidates of its cell of 5
        // bits a channel, the entries that can be nearest to some color of
        // the cell (an entry is one when its least distance to the cell is
        // within the least of the entries' greatest distances), found the
        // first time the cell is met and kept in the order of that least
        // distance, so that the search stops at the first candidate that
        // cannot come closer than the best so far
        SGCL_INLINE_HOT uint8_t _nearest(uint8_t r, uint8_t g, uint8_t b) noexcept {
            // the colors met before, in a table of their own: a photo has
            // tens of thousands of colors, its pixels millions
            const uint32_t key = 0x01000000u | uint32_t(b) << 16 | uint32_t(g) << 8 | r;
            const uint32_t slot = (key * 0x9E3779B1u) >> (32 - SeenBits);
            if (_seen_key[slot] == key) {
                return _seen_index[slot];
            }
            const uint8_t pick = _search(r, g, b);
            _seen_key[slot] = key;
            _seen_index[slot] = pick;
            return pick;
        }

        // For dithering, whose colors are the pixels' with errors added,
        // seldom the same twice: the entry nearest to the center of the
        // color's cell of 6 bits a channel, found once per cell (the error
        // carried to the next pixels makes up for the distance to the
        // center)
        SGCL_INLINE_HOT uint8_t _nearest_cell(uint8_t r, uint8_t g, uint8_t b) noexcept {
            const uint32_t cell = uint32_t(r >> 2) << 12 | uint32_t(g >> 2) << 6 | uint32_t(b >> 2);
            uint16_t m = _fine[cell];
            if (m == 0xFFFF) [[unlikely]] {
                m = _search(uint8_t((r & 0xFC) + 2), uint8_t((g & 0xFC) + 2), uint8_t((b & 0xFC) + 2));
                _fine[cell] = m;
            }
            return uint8_t(m);
        }

        uint8_t _search(uint8_t r, uint8_t g, uint8_t b) noexcept {
            const uint32_t cell = _cell(r >> 3, g >> 3, b >> 3);
            uint32_t at = _cells[cell];
            if (at == Unmet) [[unlikely]] {
                at = _candidates(cell);
                _cells[cell] = at;
            }
            const Candidate* c = _pool.get() + (at >> 9);
            const unsigned n = at & 511;
            uint32_t best = UINT32_MAX;
            uint8_t pick = c[0].index;
            for (unsigned i = 0; i < n && c[i].near <= best; ++i) {
                const uint8_t* e = _p.rgb[c[i].index];
                const int dr = int(r) - e[0], dg = int(g) - e[1], db = int(b) - e[2];
                const uint32_t d = uint32_t(dr * dr + dg * dg + db * db);
                if (d < best || (d == best && c[i].index < pick)) {
                    best = d;
                    pick = c[i].index;
                }
            }
            return pick;
        }

        // The candidates of a cell into the pool: its offset above 9 bits
        // of their count
        uint32_t _candidates(uint32_t cell) noexcept {
            const int lo[3] = {int(cell >> 10) << 3, int(cell >> 5 & 31) << 3, int(cell & 31) << 3};
            uint32_t near[256];
            uint32_t bound = UINT32_MAX;
            for (unsigned i = 0; i < _p.size; ++i) {
                uint32_t dn = 0, df = 0;
                for (unsigned ch = 0; ch < 3; ++ch) {
                    const int v = _p.rgb[i][ch];
                    const int a = lo[ch], b = lo[ch] + 7;
                    const int dl = v < a ? a - v : v > b ? v - b : 0;
                    const int dh = std::max(v - a, b - v);
                    dn += uint32_t(dl * dl);
                    df += uint32_t(dh * dh);
                }
                near[i] = dn;
                bound = std::min(bound, df);
            }
            if (_used + 256 > _room) {
                // a new block: the cells met before keep theirs
                _room = std::max<size_t>(_room * 2, 16384);
                std::unique_ptr<Candidate[]> grown(new Candidate[_room]);
                std::copy_n(_pool.get(), _used, grown.get());
                _pool = std::move(grown);
            }
            const size_t start = _used;
            Candidate* c = _pool.get();
            for (unsigned i = 0; i < _p.size; ++i) {
                if (near[i] <= bound) {
                    // in the order of the least distance (insertion: a few
                    // dozen at most)
                    size_t k = _used++;
                    while (k > start && c[k - 1].near > near[i]) {
                        c[k] = c[k - 1];
                        --k;
                    }
                    c[k] = Candidate{near[i], uint8_t(i)};
                }
            }
            return uint32_t(start << 9 | (_used - start));
        }

        SGCL_INLINE_HOT static int _clamp(int v) noexcept {
            return v < 0 ? 0 : v > 255 ? 255 : v;
        }

        // Floyd–Steinberg: 7/16 to the right, 3/16, 5/16 and 1/16 below
        // left, below and below right; the errors of the row below in
        // sixteenths, index x + 1 for pixel x. The error to the right and
        // the two last errors are carried in registers, so that each pixel
        // stores the sum below its left neighbor once, whole.
        void _dither_row(const uint32_t* in, uint8_t* out) noexcept {
            const uint32_t w = _width;
            const uint8_t clear = uint8_t(_p.size);
            const int16_t* cur = _cur;
            int16_t* next = _next;
            int right[3] = {0, 0, 0};    // 7/16 of the error before, in sixteenths
            int p1[3] = {0, 0, 0};       // the error of the pixel before
            int p2[3] = {0, 0, 0};       // and of the one before it
            for (uint32_t x = 0; x < w; ++x) {
                const uint32_t v = in[x];
                const int16_t* e = cur + 3 * (size_t(x) + 1);
                int d[3] = {0, 0, 0};
                if (v != 0) {
                    const int r = _clamp(int(v & 0xFF) + ((e[0] + right[0] + 8) >> 4));
                    const int g = _clamp(int(v >> 8 & 0xFF) + ((e[1] + right[1] + 8) >> 4));
                    const int b = _clamp(int(v >> 16 & 0xFF) + ((e[2] + right[2] + 8) >> 4));
                    const uint8_t i = _nearest_cell(uint8_t(r), uint8_t(g), uint8_t(b));
                    out[x] = i;
                    d[0] = r - _p.rgb[i][0];
                    d[1] = g - _p.rgb[i][1];
                    d[2] = b - _p.rgb[i][2];
                } else {
                    out[x] = clear;
                }
                // below left of this pixel: 1/16 of the one two before, 5/16
                // of the one before, 3/16 of this one
                int16_t* n = next + 3 * size_t(x);
                for (unsigned c = 0; c < 3; ++c) {
                    n[c] = int16_t(p2[c] + 5 * p1[c] + 3 * d[c]);
                    right[c] = 7 * d[c];
                    p2[c] = p1[c];
                    p1[c] = d[c];
                }
            }
            // below the last pixel and below right of it
            int16_t* n = next + 3 * size_t(w);
            for (unsigned c = 0; c < 3; ++c) {
                n[c] = int16_t(p2[c] + 5 * p1[c]);
                n[3 + c] = int16_t(p1[c]);
            }
            std::swap(_cur, _next);
        }

        unsigned _colors;
        bool _dither;
        Mode _mode = Mode::exact;
        uint32_t _width = 0;
        Palette _p;
        uint32_t _table[TableSize];
        uint8_t _table_index[TableSize];
        std::unique_ptr<uint8_t[]> _indices;      // an exact palette's indices, every row
        std::unique_ptr<uint32_t[]> _hist;
        std::unique_ptr<uint32_t[]> _cells;     // each cell's candidates: their offset in the pool, their count
        std::unique_ptr<Candidate[]> _pool;
        std::unique_ptr<uint32_t[]> _seen_key;    // a color met, its bit 24 set; 0 for none
        std::unique_ptr<uint8_t[]> _seen_index;
        std::unique_ptr<uint16_t[]> _fine;        // dithering's cells: the entry nearest to the center, 0xFFFF before

        size_t _used = 0;
        size_t _room = 0;
        std::unique_ptr<int16_t[]> _err;
        int16_t* _cur = nullptr;
        int16_t* _next = nullptr;
    };
}
