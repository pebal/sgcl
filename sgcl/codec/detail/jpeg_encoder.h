//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "jpeg_color.h"
#include "jpeg_fdct.h"
#include "jpeg_huffman.h"
#include "output.h"
#include "../../core/detail/bytes.h"
#include "pixels.h"
#include "../image.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace sgcl::codec::detail {
    // The quantization tables of T.81 Annex K.1 (luminance) and K.2
    // (chrominance), natural order
    inline constexpr uint8_t LuminanceQuant[64] = {
        16, 11, 10, 16, 24, 40, 51, 61,
        12, 12, 14, 19, 26, 58, 60, 55,
        14, 13, 16, 24, 40, 57, 69, 56,
        14, 17, 22, 29, 51, 87, 80, 62,
        18, 22, 37, 56, 68, 109, 103, 77,
        24, 35, 55, 64, 81, 104, 113, 92,
        49, 64, 78, 87, 103, 121, 120, 101,
        72, 92, 95, 98, 112, 100, 103, 99
    };

    inline constexpr uint8_t ChrominanceQuant[64] = {
        17, 18, 24, 47, 99, 99, 99, 99,
        18, 21, 26, 66, 99, 99, 99, 99,
        24, 26, 56, 99, 99, 99, 99, 99,
        47, 66, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99
    };

    // A quality of 1..100 as the IJG scales the tables of Annex K by it:
    // 5000 / q percent below 50, 200 - 2q from there (50 the tables as
    // printed, 100 all ones); each entry rounded, kept within 1..255 (a
    // baseline file: 8-bit tables)
    inline void scaled_quant(const uint8_t* base, int quality, uint16_t* out) noexcept {
        const int scale = quality < 50 ? 5000 / quality : 200 - quality * 2;
        for (int i = 0; i < 64; ++i) {
            long v = (long(base[i]) * scale + 50) / 100;
            out[i] = uint16_t(std::clamp(v, 1L, 255L));
        }
    }

    // A Huffman code of each symbol, from the counts of each length and
    // the symbols in order (C.2)
    struct HuffmanCodes {
        uint16_t code[256] = {};
        uint8_t size[256] = {};

        void build(const uint8_t* counts, const uint8_t* values) noexcept {
            unsigned k = 0, c = 0;
            for (int l = 1; l <= 16; ++l) {
                for (unsigned i = 0; i < counts[l - 1]; ++i, ++k, ++c) {
                    code[values[k]] = uint16_t(c);
                    size[values[k]] = uint8_t(l);
                }
                c <<= 1;
            }
        }
    };

    // The code lengths of a table optimized for these frequencies (T.81
    // K.2): one code point reserved (symbol 256, frequency 1) so that no
    // code is all ones; the two least frequent merged again and again (of
    // equal frequencies the larger symbol first: Figure K.1); the lengths
    // counted (K.2) and brought to 16 bits at most (Figure K.3), the
    // reserved point then taken out; the symbols sorted by length, then by
    // value (Figure K.4)
    struct OptimalTable {
        uint8_t counts[16] = {};
        uint8_t values[256] = {};

        void build(const uint32_t* frequencies) noexcept {
            int64_t freq[257];
            int codesize[257] = {};
            int others[257];
            for (int i = 0; i < 256; ++i) {
                freq[i] = frequencies[i];
            }
            freq[256] = 1;
            for (int i = 0; i < 257; ++i) {
                others[i] = -1;
            }
            for (;;) {
                int v1 = -1, v2 = -1;
                int64_t least = INT64_MAX;
                for (int i = 0; i <= 256; ++i) {
                    if (freq[i] && freq[i] <= least) {
                        least = freq[i];
                        v1 = i;
                    }
                }
                least = INT64_MAX;
                for (int i = 0; i <= 256; ++i) {
                    if (freq[i] && freq[i] <= least && i != v1) {
                        least = freq[i];
                        v2 = i;
                    }
                }
                if (v2 < 0) {
                    break;
                }
                freq[v1] += freq[v2];
                freq[v2] = 0;
                ++codesize[v1];
                while (others[v1] >= 0) {
                    v1 = others[v1];
                    ++codesize[v1];
                }
                others[v1] = v2;
                ++codesize[v2];
                while (others[v2] >= 0) {
                    v2 = others[v2];
                    ++codesize[v2];
                }
            }
            int bits[33] = {};
            for (int i = 0; i <= 256; ++i) {
                if (codesize[i]) {
                    ++bits[std::min(codesize[i], 32)];
                }
            }
            for (int i = 32; i > 16; --i) {
                while (bits[i] > 0) {
                    int j = i - 2;
                    while (bits[j] == 0) {
                        --j;
                    }
                    bits[i] -= 2;
                    bits[i - 1] += 1;
                    bits[j + 1] += 2;
                    bits[j] -= 1;
                }
            }
            int i = 16;
            while (bits[i] == 0) {
                --i;
            }
            --bits[i];   // the reserved code point
            for (int l = 1; l <= 16; ++l) {
                counts[l - 1] = uint8_t(bits[l]);
            }
            int k = 0;
            for (int l = 1; l <= 32; ++l) {
                for (int s = 0; s <= 255; ++s) {
                    if (codesize[s] == l) {
                        values[k++] = uint8_t(s);
                    }
                }
            }
        }

        unsigned total() const noexcept {
            unsigned n = 0;
            for (int l = 0; l < 16; ++l) {
                n += counts[l];
            }
            return n;
        }
    };

    struct JpegEncodeSettings {
        int quality = 85;
        unsigned h = 2, v = 2;   // the luminance's sampling; chrominance 1×1
        bool optimize = false;
    };

    // The baseline JPEG encoder (T.81, sequential DCT, Huffman), as
    // libjpeg-turbo's cjpeg writes with `-dct int -baseline`: the image's
    // pixels as gray (a gray format) or as YCbCr (any other, through rgb8)
    // with JFIF's conversion in 16-bit fixed point; chrominance subsampled
    // 2× across (4:2:2) or both ways (4:2:0) by averaging, the rounding
    // alternated across a row; each MCU's blocks transformed (jpeg_fdct.h)
    // and quantized by the tables of Annex K scaled for the quality (to the
    // nearest, halves away from zero); the typical Huffman tables of Annex
    // K, or tables made for the image's own counts (optimize: two passes
    // over the image, no coefficient buffer). The edges as libjpeg makes
    // them: the last column and the last row repeated to whole blocks; the
    // blocks an MCU has past the component's edge all zero but for the DC
    // of the block before. Markers: SOI, JFIF, EXIF (APP1) and the ICC
    // profile (APP2 chunks) of the image, DQT, SOF0, DHT, SOS, EOI. Memory:
    // one row of MCUs and a block of output, none per MCU.
    template<class Sink>
    class JpegEncoder {
    public:
        JpegEncoder(const image& im, const JpegEncodeSettings& s, Sink& sink) noexcept
        : _im(im), _set(s), _sink(sink) {
        }

        // false when the sink failed, or for an image JPEG cannot hold (a
        // side past 65 535, SOF's 16 bits), refused before anything is
        // written: the error in the sink's failure
        bool run() noexcept(NothrowSink<Sink>) {
            if (const auto& s = ImageAccess::state(_im); s.width > 65535 || s.height > 65535) {
                _sink.failure = error(errc::invalid_argument, 0, "jpeg: a side past 65535 pixels, more than SOF holds");
                return false;
            }
            _setup();
            // headers
            _marker(0xD8);
            static constexpr uint8_t Jfif[14] = {'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0};
            _segment(0xE0, Jfif, sizeof(Jfif));
            const auto& st = ImageAccess::state(_im);
            if (!st.exif.empty() && st.exif.size() <= 65527) {
                std::vector<uint8_t> body(6 + st.exif.size());
                std::memcpy(body.data(), "Exif\0\0", 6);
                sgcl::detail::copy_bytes(body.data() + 6, st.exif.data(), st.exif.size());
                _segment(0xE1, body.data(), body.size());
            }
            if (!st.icc.empty()) {
                // chunks of at most 65 519 bytes, numbered from 1 (at most 255 of them)
                constexpr size_t Chunk = 65519;
                const size_t n = (st.icc.size() + Chunk - 1) / Chunk;
                if (n <= 255) {
                    for (size_t i = 0; i < n; ++i) {
                        const size_t len = std::min(Chunk, st.icc.size() - i * Chunk);
                        std::vector<uint8_t> body(14 + len);
                        std::memcpy(body.data(), "ICC_PROFILE\0", 12);
                        body[12] = uint8_t(i + 1);
                        body[13] = uint8_t(n);
                        sgcl::detail::copy_bytes(body.data() + 14, st.icc.data() + i * Chunk, len);
                        _segment(0xE2, body.data(), body.size());
                    }
                }
            }
            for (unsigned t = 0; t < (_gray ? 1u : 2u); ++t) {
                uint8_t body[65];
                body[0] = uint8_t(t);
                for (int k = 0; k < 64; ++k) {
                    body[1 + k] = uint8_t(_quant[t][ZigZag[k]]);
                }
                _segment(0xDB, body, sizeof(body));
            }
            {
                uint8_t body[6 + 9];
                body[0] = 8;
                body[1] = uint8_t(_height >> 8);
                body[2] = uint8_t(_height);
                body[3] = uint8_t(_width >> 8);
                body[4] = uint8_t(_width);
                body[5] = uint8_t(_ncomp);
                for (unsigned c = 0; c < _ncomp; ++c) {
                    body[6 + 3 * c] = uint8_t(c + 1);
                    body[7 + 3 * c] = uint8_t(_comp[c].h << 4 | _comp[c].v);
                    body[8 + 3 * c] = uint8_t(c ? 1 : 0);
                }
                _segment(0xC0, body, 6 + 3 * _ncomp);
            }
            // the tables: typical, or made from a first pass's counts
            if (_set.optimize) {
                _counting = true;
                _pass();
                _counting = false;
                for (unsigned t = 0; t < (_gray ? 1u : 2u); ++t) {
                    _dc_opt[t].build(_dc_count[t]);
                    _ac_opt[t].build(_ac_count[t]);
                    _dc_codes[t].build(_dc_opt[t].counts, _dc_opt[t].values);
                    _ac_codes[t].build(_ac_opt[t].counts, _ac_opt[t].values);
                }
            } else {
                for (unsigned t = 0; t < (_gray ? 1u : 2u); ++t) {
                    _dc_codes[t].build(t ? annex_k::DcChrominanceBits : annex_k::DcLuminanceBits, annex_k::DcValues);
                    _ac_codes[t].build(t ? annex_k::AcChrominanceBits : annex_k::AcLuminanceBits,
                                       t ? annex_k::AcChrominanceValues : annex_k::AcLuminanceValues);
                }
            }
            for (unsigned t = 0; t < (_gray ? 1u : 2u); ++t) {
                _dht(0x00 | t, t, true);
                _dht(0x10 | t, t, false);
            }
            {
                uint8_t body[1 + 6 + 3];
                body[0] = uint8_t(_ncomp);
                for (unsigned c = 0; c < _ncomp; ++c) {
                    body[1 + 2 * c] = uint8_t(c + 1);
                    body[2 + 2 * c] = uint8_t(c ? 0x11 : 0x00);
                }
                body[1 + 2 * _ncomp] = 0;
                body[2 + 2 * _ncomp] = 63;
                body[3 + 2 * _ncomp] = 0;
                _segment(0xDA, body, 4 + 2 * _ncomp);
            }
            if (!_ok) {
                return false;
            }
            _pass();
            _flush_bits();
            _marker(0xD9);
            _drain();
            return _ok;
        }

    private:
        struct Component {
            unsigned h = 1, v = 1;
            uint32_t cw = 0, ch = 0;   // samples across and down
            uint32_t wb = 0, hb = 0;   // real blocks across and down
            size_t pw = 0;             // a row of its MCU row, in samples
            uint8_t* rows = nullptr;   // its MCU row: v × 8 rows
            uint8_t* full = nullptr;   // the full-resolution rows of the MCU row, before subsampling
            int dc = 0;                // the last DC, for the difference
        };

        void _setup() noexcept {
            const auto& st = ImageAccess::state(_im);
            _width = st.width;
            _height = st.height;
            const pixel_format f = st.format;
            _gray = f == pixel_format::gray8 || f == pixel_format::gray16 || f == pixel_format::gray_alpha8 || f == pixel_format::gray_alpha16;
            _ncomp = _gray ? 1 : 3;
            const pixel_format source = _gray ? pixel_format::gray8 : pixel_format::rgb8;
            _to_source = f == source ? nullptr : converter(f, source);
            scaled_quant(LuminanceQuant, _set.quality, _quant[0]);
            scaled_quant(ChrominanceQuant, _set.quality, _quant[1]);
            _steps[0] = QuantSteps(_quant[0]);
            _steps[1] = QuantSteps(_quant[1]);
            if (_gray) {
                _comp[0].h = _comp[0].v = 1;
            } else {
                _comp[0].h = _set.h;
                _comp[0].v = _set.v;
                _comp[1].h = _comp[1].v = _comp[2].h = _comp[2].v = 1;
            }
            _hmax = _comp[0].h;
            _vmax = _comp[0].v;
            _mcux = (_width + 8 * _hmax - 1) / (8 * _hmax);
            _mcuy = (_height + 8 * _vmax - 1) / (8 * _vmax);
            _full_width = size_t(_mcux) * _hmax * 8;
            size_t total = size_t(_width) * 3 + 16;   // a row of the source
            for (unsigned c = 0; c < _ncomp; ++c) {
                Component& k = _comp[c];
                k.cw = uint32_t((uint64_t(_width) * k.h + _hmax - 1) / _hmax);
                k.ch = uint32_t((uint64_t(_height) * k.v + _vmax - 1) / _vmax);
                k.wb = (k.cw + 7) / 8;
                k.hb = (k.ch + 7) / 8;
                k.pw = size_t(_mcux) * k.h * 8;
                total += k.pw * k.v * 8 + _full_width * _vmax * 8;
            }
            _memory.reset(new uint8_t[total]);
            uint8_t* m = _memory.get();
            _source_row = m;
            m += size_t(_width) * 3 + 16;
            for (unsigned c = 0; c < _ncomp; ++c) {
                _comp[c].rows = m;
                m += _comp[c].pw * _comp[c].v * 8;
                _comp[c].full = m;
                m += _full_width * _vmax * 8;
            }
            _out.resize(OutputBlock + 16);
        }

        // ---- output ----------------------------------------------------------

        // The bytes go to a buffer of OutputBlock (and a margin of 16 for
        // a word's bytes and their stuffing), to the sink when it is full
        static constexpr size_t OutputBlock = 65536;

        void _drain() noexcept(NothrowSink<Sink>) {
            if (_ok && _olen) {
                _ok = _sink.put(_out.data(), _olen);
            }
            _olen = 0;
        }

        void _byte(uint8_t b) noexcept(NothrowSink<Sink>) {
            _out[_olen++] = b;
            if (_olen >= OutputBlock) {
                _drain();
            }
        }

        void _marker(uint8_t code) noexcept(NothrowSink<Sink>) {
            _byte(0xFF);
            _byte(code);
        }

        void _segment(uint8_t code, const uint8_t* body, size_t n) noexcept(NothrowSink<Sink>) {
            _marker(code);
            _byte(uint8_t((n + 2) >> 8));
            _byte(uint8_t(n + 2));
            for (size_t i = 0; i < n; ++i) {
                _byte(body[i]);
            }
        }

        void _dht(uint8_t tc_th, unsigned t, bool dc) noexcept(NothrowSink<Sink>) {
            const uint8_t* counts;
            const uint8_t* values;
            unsigned n;
            if (_set.optimize) {
                const OptimalTable& o = dc ? _dc_opt[t] : _ac_opt[t];
                counts = o.counts;
                values = o.values;
                n = o.total();
            } else if (dc) {
                counts = t ? annex_k::DcChrominanceBits : annex_k::DcLuminanceBits;
                values = annex_k::DcValues;
                n = 12;
            } else {
                counts = t ? annex_k::AcChrominanceBits : annex_k::AcLuminanceBits;
                values = t ? annex_k::AcChrominanceValues : annex_k::AcLuminanceValues;
                n = 162;
            }
            uint8_t body[1 + 16 + 256];
            body[0] = tc_th;
            std::memcpy(body + 1, counts, 16);
            sgcl::detail::copy_bytes(body + 17, values, n);
            _segment(0xC4, body, 17 + n);
        }

        // The entropy-coded bits, most significant first, a 0 stuffed after
        // each 0xFF (F.1.2.3). They gather in a 64-bit word, its low
        // 64 - _free bits the pending ones (anything above them already
        // written), and go out when the word is full: eight bytes in one
        // store when none of them is 0xFF (one test of the eight at once),
        // else a byte at a time with the zeros.
        // bits: the value in its low n bits and nothing above (n <= 32)
        void _put(uint32_t bits, int n) noexcept(NothrowSink<Sink>) {
            if (n < _free) {
                _acc = _acc << n | bits;
                _free -= n;
                return;
            }
            // the word's last _free bits from the top of bits, the rest of
            // bits (r of them) the next word's first
            const int r = n - _free;
            _word(_acc << _free | uint64_t(bits) >> r);
            _acc = bits;
            _free = 64 - r;
        }

        void _word(uint64_t w) noexcept(NothrowSink<Sink>) {
            if (_olen + 16 > OutputBlock) {
                _drain();
            }
            // a byte of w is 0xFF where its complement has a zero byte
            const uint64_t c = ~w;
            if (((c - 0x0101010101010101u) & ~c & 0x8080808080808080u) == 0) {
                uint8_t bytes[8];
                for (int i = 0; i < 8; ++i) {
                    bytes[i] = uint8_t(w >> (56 - 8 * i));
                }
                std::memcpy(_out.data() + _olen, bytes, 8);
                _olen += 8;
                return;
            }
            for (int s = 56; s >= 0; s -= 8) {
                const uint8_t b = uint8_t(w >> s);
                _out[_olen++] = b;
                if (b == 0xFF) {
                    _out[_olen++] = 0;
                }
            }
        }

        // The pending bits, the last byte filled with ones
        void _flush_bits() noexcept(NothrowSink<Sink>) {
            int pending = 64 - _free;
            if (pending == 0) {
                return;
            }
            const int pad = (8 - (pending & 7)) & 7;
            uint64_t acc = _acc << pad | ((uint64_t(1) << pad) - 1);
            pending += pad;
            while (pending) {
                pending -= 8;
                const uint8_t b = uint8_t(acc >> pending);
                _byte(b);
                if (b == 0xFF) {
                    _byte(0);
                }
            }
            _free = 64;
        }

        // ---- the image, MCU row by MCU row ----------------------------------

        // Row y of the image (the last one for rows past it) as gray8 or rgb8
        const uint8_t* _source(uint32_t y) noexcept {
            const auto& st = ImageAccess::state(_im);
            y = std::min(y, _height - 1);
            const auto* p = reinterpret_cast<const uint8_t*>(st.pixels.data()) + size_t(y) * st.stride;
            if (!_to_source) {
                return p;
            }
            _to_source(reinterpret_cast<const std::byte*>(p), reinterpret_cast<std::byte*>(_source_row), _width);
            return _source_row;
        }

        // The full-resolution rows of MCU row m: each component's (RGB to
        // YCbCr by jpeg_color.h), its last column repeated to its whole
        // blocks (at its subsampled width)
        void _convert_rows(uint32_t m) noexcept {
            for (unsigned r = 0; r < _vmax * 8; ++r) {
                const uint8_t* s = _source(m * _vmax * 8 + r);
                if (_gray) {
                    uint8_t* y = _comp[0].full + size_t(r) * _full_width;
                    sgcl::detail::copy_bytes(y, s, _width);
                } else {
                    uint8_t* y = _comp[0].full + size_t(r) * _full_width;
                    uint8_t* cb = _comp[1].full + size_t(r) * _full_width;
                    uint8_t* cr = _comp[2].full + size_t(r) * _full_width;
                    rgb_to_ycc(s, y, cb, cr, _width);
                }
                for (unsigned c = 0; c < _ncomp; ++c) {
                    Component& k = _comp[c];
                    const size_t want = size_t(k.wb) * 8 * (_hmax / k.h);
                    uint8_t* row = k.full + size_t(r) * _full_width;
                    for (size_t x = _width; x < want; ++x) {
                        row[x] = row[_width - 1];
                    }
                }
            }
        }

        // Each component's rows at its sampling: a copy, or the average of
        // two samples across or of four (jpeg_color.h's downsample)
        void _subsample(uint32_t m) noexcept {
            for (unsigned c = 0; c < _ncomp; ++c) {
                Component& k = _comp[c];
                const unsigned ex = _hmax / k.h, ey = _vmax / k.v;
                const size_t n = size_t(k.wb) * 8;
                // the rows made of the image's (its last row repeated to a
                // whole group of vmax); past them, the last of them repeated
                const uint32_t made = (_height + _vmax - 1) / _vmax * k.v;
                for (unsigned r = 0; r < k.v * 8; ++r) {
                    uint8_t* out = k.rows + size_t(r) * k.pw;
                    const uint32_t gr = m * k.v * 8 + r;
                    if (gr >= made) {
                        sgcl::detail::copy_bytes(out, k.rows + size_t(made - 1 - m * k.v * 8) * k.pw, n);
                        continue;
                    }
                    const uint8_t* a = k.full + size_t(r * ey) * _full_width;
                    if (ex == 1 && ey == 1) {
                        sgcl::detail::copy_bytes(out, a, n);
                    } else if (ey == 1) {
                        downsample::h2v1(a, out, n);
                    } else {
                        downsample::h2v2(a, a + _full_width, out, n);
                    }
                }
            }
        }

        // A value's category and its bits (F.1.2.1): the magnitude's length,
        // and the value itself if positive or less one if negative, in the
        // category's low bits
        struct Category {
            unsigned size;
            uint32_t bits;
        };

        static Category _category(int v) noexcept {
            const int sign = v >> 31;
            const unsigned size = unsigned(std::bit_width(unsigned((v ^ sign) - sign)));
            return {size, uint32_t(v + sign) & ((1u << size) - 1)};
        }

        // A block's symbols (F.1.2): counted, or coded. The block made ready
        // by coded_block (jpeg_fdct.h: zigzag order, categories, value bits,
        // the mask of the nonzero ones): a run is the distance to the next
        // set bit, 16 zeros a ZRL, the rest after the last one an EOB
        template<bool Counting>
        void _code_block(const int16_t* b, Component& k, unsigned t) noexcept(NothrowSink<Sink>) {
            CodedBlock c;
            coded_block(b, c);
            const Category dc = _category(c.z[0] - k.dc);
            k.dc = c.z[0];
            if constexpr (Counting) {
                ++_dc_count[t][dc.size];
            } else {
                _put(uint32_t(_dc_codes[t].code[dc.size]) << dc.size | dc.bits, _dc_codes[t].size[dc.size] + int(dc.size));
            }
            const HuffmanCodes& ac = _ac_codes[t];
            uint64_t mask = c.mask & ~uint64_t(1);
            int last = 0;
            while (mask) {
                const int i = std::countr_zero(mask);
                mask &= mask - 1;
                int run = i - last - 1;
                last = i;
                for (; run > 15; run -= 16) {
                    if constexpr (Counting) {
                        ++_ac_count[t][0xF0];
                    } else {
                        _put(ac.code[0xF0], ac.size[0xF0]);
                    }
                }
                const unsigned size = c.size[i];
                const unsigned sym = unsigned(run) << 4 | size;
                if constexpr (Counting) {
                    ++_ac_count[t][sym];
                } else {
                    _put(uint32_t(ac.code[sym]) << size | c.bits[i], ac.size[sym] + int(size));
                }
            }
            if (last != 63) {
                if constexpr (Counting) {
                    ++_ac_count[t][0];
                } else {
                    _put(ac.code[0], ac.size[0]);
                }
            }
        }

        // Every MCU of the image, once: counted or coded
        void _pass() noexcept(NothrowSink<Sink>) {

            for (unsigned c = 0; c < _ncomp; ++c) {
                _comp[c].dc = 0;
            }
            int32_t dct[64];
            int16_t blocks[10][64];
            for (uint32_t m = 0; m < _mcuy && _ok; ++m) {
                _convert_rows(m);
                _subsample(m);
                for (uint32_t mx = 0; mx < _mcux; ++mx) {
                    for (unsigned c = 0; c < _ncomp; ++c) {
                        Component& k = _comp[c];
                        const unsigned t = c ? 1 : 0;
                        unsigned n = 0;
                        for (unsigned by = 0; by < k.v; ++by) {
                            const uint32_t gy = m * k.v + by;
                            for (unsigned bx = 0; bx < k.h; ++bx, ++n) {
                                const uint32_t gx = mx * k.h + bx;
                                int16_t* b = blocks[n];
                                if (gy < k.hb && gx < k.wb) {
                                    fdct_islow(k.rows + size_t(by) * 8 * k.pw + size_t(gx) * 8, k.pw, dct);
                                    quantize(dct, _steps[t], b);
                                } else {
                                    // past the edge: zero but for the DC of the
                                    // block before in the MCU (for a row wholly
                                    // past the bottom, the last of the row above)
                                    std::memset(b, 0, sizeof(blocks[0]));
                                    b[0] = blocks[n - 1][0];
                                }
                            }
                        }
                        for (unsigned i = 0; i < n; ++i) {
                            if (_counting) {
                                _code_block<true>(blocks[i], k, t);
                            } else {
                                _code_block<false>(blocks[i], k, t);
                            }
                        }
                    }
                }
            }
        }

        const image& _im;
        JpegEncodeSettings _set;
        Sink& _sink;
        bool _ok = true;

        uint32_t _width = 0, _height = 0;
        bool _gray = false;
        unsigned _ncomp = 1, _hmax = 1, _vmax = 1;
        uint32_t _mcux = 0, _mcuy = 0;
        size_t _full_width = 0;
        Component _comp[3];
        ConvertRow _to_source = nullptr;
        uint16_t _quant[2][64] = {};
        QuantSteps _steps[2];

        HuffmanCodes _dc_codes[2], _ac_codes[2];
        OptimalTable _dc_opt[2], _ac_opt[2];
        uint32_t _dc_count[2][256] = {};
        uint32_t _ac_count[2][256] = {};
        bool _counting = false;

        std::unique_ptr<uint8_t[]> _memory;
        uint8_t* _source_row = nullptr;
        std::vector<uint8_t> _out;
        size_t _olen = 0;
        uint64_t _acc = 0;
        int _free = 64;   // the bits of _acc not yet holding pending ones
    };
}
