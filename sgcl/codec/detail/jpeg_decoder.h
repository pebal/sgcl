//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "exif.h"
#include "input.h"
#include "jpeg_color.h"
#include "jpeg_huffman.h"
#include "jpeg_idct.h"
#include "pixels.h"
#include "../error.h"
#include "../image.h"
#include "../options.h"
#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"
#include "../../core/expected.h"
#include "../../core/string.h"
#include "../../core/vector.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// A hot loop of a few instructions starts on a 64-byte boundary, so that
// it never straddles two: placed by the code before it, the progressive
// decoding's correction loop of an EOB run crossed one in a build where
// only code elsewhere in the program had changed, and the decoding took
// a fifth longer (Apple silicon)
#if defined(__clang__) && defined(__has_cpp_attribute)
#if __has_cpp_attribute(clang::code_align)
#define SGCL_JPEG_ALIGN_LOOP [[clang::code_align(64)]]
#endif
#endif
#ifndef SGCL_JPEG_ALIGN_LOOP
#define SGCL_JPEG_ALIGN_LOOP
#endif

namespace sgcl::codec::detail {
    // The JPEG decoder of the sequential DCT modes with Huffman coding
    // (T.81: baseline, SOF0, and extended sequential of 8 bits, SOF1),
    // over an input of MemoryInput's shape. Markers read in order (Annex
    // B); the scans' entropy-coded data decoded MCU by MCU (Annex F),
    // each block dequantized and transformed (jpeg_idct.h) into the rows
    // of its component; the components upsampled and converted to the
    // image's pixels (jpeg_color.h), row by row as they complete. A file
    // of one scan holding every component (nearly every file) keeps three
    // rows of MCUs of each component, the one being decoded and two for
    // the rows above and below a row being upsampled; a file of one scan
    // per component (or per some) keeps its components whole until the
    // last scan. None of it per MCU: made when the first scan starts.
    //
    // The pixels: gray (one component) gray8; YCbCr (JFIF, or three
    // components without an Adobe marker saying otherwise) and RGB (Adobe
    // transform 0, or components named R, G, B) rgb8; CMYK and YCCK (four
    // components, Adobe transform 0 and 2) cmyk8, the ink of each channel,
    // the values of a file with an Adobe marker inverted as Adobe writes
    // them (as Go reads them). EXIF (APP1 "Exif") and the ICC profile
    // (APP2 "ICC_PROFILE", its chunks joined in their order) kept.
    //
    // Progressive files (SOF2, Annex G) keep the coefficients of the whole
    // image (2 bytes a sample of each component) until EOI, then transform
    // them row by row into the same rings. Annex K's typical Huffman tables
    // stand in for a table 0 or 1 a file never defines (Motion-JPEG).
    // libjpeg-turbo smooths the blocks of a progression that stops before
    // its low coefficients are whole ("block smoothing"); this decoder does
    // not: such a file is its pixels as they are, equal to libjpeg's with
    // do_block_smoothing off.
    //
    // Not read: lossless, hierarchical, arithmetic coding, 12-bit samples,
    // DNL: unsupported. Strict where libjpeg only
    // warns: data that ends early (unexpected_end), a scan whose data ends
    // before its MCUs do, a restart marker out of turn (corrupt); lenient
    // as it is on bytes between a scan's end and the next marker.
    template<class Input>
    class JpegDecoder {
    public:
        SGCL_INLINE_HOT JpegDecoder(Input& in, const decode_options& o) noexcept
        : _in(in), _o(o) {
        }

        expected<image, error> run() noexcept(NothrowInput<Input>) {
            if (_o.want && !valid(*_o.want)) {
                return unexpected(error(errc::invalid_argument, 0, "jpeg: decode_options.want outside the list"));
            }
            uint8_t soi[2];
            if (!_read(soi, 2)) {
                return unexpected(*_err);
            }
            if (soi[0] != 0xFF || soi[1] != 0xD8) {
                return unexpected(error(errc::corrupt, 0, "jpeg: no SOI marker"));
            }
            for (;;) {
                int code;
                const uint64_t at = _in.offset();
                if (!_next_marker(code)) {
                    return unexpected(*_err);
                }
                bool ok = true;
                switch (code) {
                    case 0xC0: case 0xC1:
                        ok = _sof(at, false);
                        break;
                    case 0xC2:
                        ok = _sof(at, true);
                        break;
                    case 0xC3: case 0xC5: case 0xC6: case 0xC7: case 0xCB: case 0xCD: case 0xCE: case 0xCF:
                        return unexpected(error(errc::unsupported, at, "jpeg: lossless or hierarchical"));
                    case 0xC9: case 0xCA: case 0xCC:
                        return unexpected(error(errc::unsupported, at, "jpeg: arithmetic coding"));
                    case 0xC4:
                        ok = _dht(at);
                        break;
                    case 0xDB:
                        ok = _dqt(at);
                        break;
                    case 0xDD:
                        ok = _dri(at);
                        break;
                    case 0xDA:
                        ok = _sos(at);
                        break;
                    case 0xDC:
                        return unexpected(error(errc::unsupported, at, "jpeg: DNL (the height after the scan)"));
                    case 0xD8:
                        return unexpected(error(errc::corrupt, at, "jpeg: a second SOI"));
                    case 0xD9:
                        return _finish(at);
                    case 0x01: case 0xD0: case 0xD1: case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD7:
                        break;   // no segment: a restart marker out of a scan is passed over, as libjpeg does
                    default:
                        if (code >= 0xE0 && code <= 0xEF) {
                            ok = _app(code, at);
                        } else {
                            ok = _skip_segment(at);   // COM, JPGn, anything else with a length
                        }
                        break;
                }
                if (!ok) {
                    return unexpected(*_err);
                }
            }
        }

    private:
        struct Component {
            uint8_t id = 0, h = 1, v = 1, tq = 0;
            uint32_t cw = 0, ch = 0;        // its samples across and down
            size_t pw = 0;                  // a row of its buffer, in samples (whole blocks)
            uint32_t rows = 0;              // the rows of an MCU row
            uint32_t slots = 3;             // the MCU rows its buffer holds
            uint8_t* buffer = nullptr;
            uint8_t* up = nullptr;          // a row upsampled
            uint16_t quant[64] = {};        // natural order, latched at its scan
            int dc = 0;
            uint8_t td = 0, ta = 0;
            bool decoded = false;
            bool latched = false;           // its quantization table taken, at its first scan
            uint32_t bw = 0, bh = 0;        // its blocks across and down (progressive)
            int16_t* coef = nullptr;        // their coefficients, 64 a block (progressive)
        };

        enum class Space : uint8_t { gray, ycc, rgb, cmyk, ycck };

        // ---- input ---------------------------------------------------------

        bool _fail(errc code, uint64_t at, const std::string& what) noexcept {
            _err = error(code, at, string(what));
            return false;
        }

        bool _fail_input() noexcept {
            _err = *_in.failure;
            return false;
        }

        bool _read(uint8_t* dst, size_t n) noexcept(NothrowInput<Input>) {
            while (n) {
                const uint8_t* p;
                size_t got;
                if (!_in.peek(n, p, got)) {
                    return _fail_input();
                }
                if (!got) {
                    return _fail(errc::unexpected_end, _in.offset(), "jpeg: the data ends in the middle");
                }
                sgcl::detail::copy_bytes(dst, p, got);
                _in.consume(got);
                dst += got;
                n -= got;
            }
            return true;
        }

        bool _skip(size_t n) noexcept(NothrowInput<Input>) {
            while (n) {
                const uint8_t* p;
                size_t got;
                if (!_in.peek(n, p, got)) {
                    return _fail_input();
                }
                if (!got) {
                    return _fail(errc::unexpected_end, _in.offset(), "jpeg: the data ends in the middle");
                }
                _in.consume(got);
                n -= got;
            }
            return true;
        }

        // The next byte, -1 at the end; false when the source failed
        bool _byte(int& b) noexcept(NothrowInput<Input>) {
            const uint8_t* p;
            size_t got;
            if (!_in.peek(1, p, got)) {
                return _fail_input();
            }
            if (!got) {
                b = -1;
                return true;
            }
            b = *p;
            _in.consume(1);
            return true;
        }

        // The code of the next marker: 0xFF, any fill bytes of 0xFF, the
        // code. Bytes before the 0xFF are passed over (after a scan's data,
        // as libjpeg does; elsewhere the segments are whole and there are none)
        bool _next_marker(int& code) noexcept(NothrowInput<Input>) {
            if (_pending) {
                // the scan's data ended at it: its 0xFF and code already read
                _pending = false;
                code = _pending_code;
                return true;
            }
            for (;;) {
                int b;
                do {
                    if (!_byte(b)) {
                        return false;
                    }
                    if (b < 0) {
                        return _fail(errc::unexpected_end, _in.offset(), "jpeg: the data ends before EOI");
                    }
                } while (b != 0xFF);
                do {
                    if (!_byte(b)) {
                        return false;
                    }
                    if (b < 0) {
                        return _fail(errc::unexpected_end, _in.offset(), "jpeg: the data ends before EOI");
                    }
                } while (b == 0xFF);
                if (b != 0) {
                    code = b;
                    return true;
                }
                // a stuffed 0xFF of data left after a scan: passed over
            }
        }

        // A segment's length, less its own two bytes
        bool _length(uint64_t at, size_t& n) noexcept(NothrowInput<Input>) {
            uint8_t b[2];
            if (!_read(b, 2)) {
                return false;
            }
            const size_t l = size_t(b[0]) << 8 | b[1];
            if (l < 2) {
                return _fail(errc::corrupt, at, "jpeg: a segment length below 2");
            }
            n = l - 2;
            return true;
        }

        SGCL_INLINE_HOT bool _skip_segment(uint64_t at) noexcept(NothrowInput<Input>) {
            size_t n;
            return _length(at, n) && _skip(n);
        }

        SGCL_INLINE_HOT bool _segment(uint64_t at, std::vector<uint8_t>& body) noexcept(NothrowInput<Input>) {
            size_t n;
            if (!_length(at, n)) {
                return false;
            }
            body.resize(n);
            return _read(body.data(), n);
        }

        // ---- segments ------------------------------------------------------

        bool _app(int code, uint64_t at) noexcept(NothrowInput<Input>) {
            std::vector<uint8_t> b;
            if (!_segment(at, b)) {
                return false;
            }
            auto starts = [&](const char* tag, size_t n) { return b.size() >= n && std::memcmp(b.data(), tag, n) == 0; };
            if (code == 0xE0 && starts("JFIF\0", 5)) {
                _jfif = true;
            } else if (code == 0xEE && starts("Adobe", 5) && b.size() >= 12) {
                _adobe = true;
                _adobe_transform = b[11];
            } else if (code == 0xE1 && starts("Exif\0\0", 6) && _o.metadata && _exif.empty()) {
                if (b.size() - 6 > _o.limits.max_metadata) {
                    return _fail(errc::too_large, at, "jpeg: EXIF past limits.max_metadata");
                }
                _exif.insert(_exif.end(), reinterpret_cast<const byte*>(b.data() + 6), reinterpret_cast<const byte*>(b.data() + b.size()));
            } else if (code == 0xE2 && starts("ICC_PROFILE\0", 12) && b.size() >= 14 && _o.metadata) {
                // a chunk: its number from 1, the count, the bytes
                const unsigned seq = b[12], count = b[13];
                if (seq == 0 || count == 0 || seq > count || (_icc_count && _icc_count != count)) {
                    _icc_broken = true;
                    return true;
                }
                _icc_count = count;
                _icc_chunks.resize(count);
                _icc_seen.resize(count);
                if (_icc_seen[seq - 1]) {
                    _icc_broken = true;   // a chunk twice: the profile is not whole
                    return true;
                }
                _icc_total += b.size() - 14;
                if (_icc_total > _o.limits.max_metadata) {
                    return _fail(errc::too_large, at, "jpeg: ICC profile past limits.max_metadata");
                }
                _icc_chunks[seq - 1].assign(b.begin() + 14, b.end());
                _icc_seen[seq - 1] = true;
            }
            return true;
        }

        bool _dqt(uint64_t at) noexcept(NothrowInput<Input>) {
            std::vector<uint8_t> b;
            if (!_segment(at, b)) {
                return false;
            }
            size_t i = 0;
            while (i < b.size()) {
                const unsigned pq = b[i] >> 4, tq = b[i] & 15;
                ++i;
                if (pq > 1 || tq > 3) {
                    return _fail(errc::corrupt, at, "jpeg: DQT table " + std::to_string(tq) + " of precision " + std::to_string(pq));
                }
                const size_t need = pq ? 128 : 64;
                if (b.size() - i < need) {
                    return _fail(errc::corrupt, at, "jpeg: DQT shorter than its tables");
                }
                for (unsigned k = 0; k < 64; ++k) {
                    const unsigned v = pq ? unsigned(b[i + 2 * k]) << 8 | b[i + 2 * k + 1] : b[i + k];
                    _quant[tq][ZigZag[k]] = uint16_t(v);
                }
                _quant_defined[tq] = true;
                i += need;
            }
            return true;
        }

        bool _dht(uint64_t at) noexcept(NothrowInput<Input>) {
            std::vector<uint8_t> b;
            if (!_segment(at, b)) {
                return false;
            }
            size_t i = 0;
            while (i < b.size()) {
                if (b.size() - i < 17) {
                    return _fail(errc::corrupt, at, "jpeg: DHT shorter than its tables");
                }
                const unsigned tc = b[i] >> 4, th = b[i] & 15;
                if (tc > 1 || th > 3) {
                    return _fail(errc::corrupt, at, "jpeg: DHT table class " + std::to_string(tc) + " number " + std::to_string(th));
                }
                const uint8_t* counts = b.data() + i + 1;
                size_t total = 0;
                for (int l = 0; l < 16; ++l) {
                    total += counts[l];
                }
                i += 17;
                if (total > 256 || b.size() - i < total) {
                    return _fail(errc::corrupt, at, "jpeg: DHT of more symbols than it holds");
                }
                HuffmanTable& t = tc ? _ac[th] : _dc[th];
                if (!t.build(counts, b.data() + i, tc == 0)) {
                    return _fail(errc::corrupt, at, "jpeg: a Huffman table no code can be made of");
                }
                i += total;
            }
            return true;
        }

        SGCL_INLINE_HOT bool _dri(uint64_t at) noexcept(NothrowInput<Input>) {
            std::vector<uint8_t> b;
            if (!_segment(at, b)) {
                return false;
            }
            if (b.size() != 2) {
                return _fail(errc::corrupt, at, "jpeg: DRI of " + std::to_string(b.size()) + " bytes");
            }
            _restart_interval = unsigned(b[0]) << 8 | b[1];
            return true;
        }

        bool _sof(uint64_t at, bool progressive) noexcept(NothrowInput<Input>) {
            _progressive = progressive;
            if (_have_frame) {
                return _fail(errc::corrupt, at, "jpeg: a second frame");
            }
            std::vector<uint8_t> b;
            if (!_segment(at, b)) {
                return false;
            }
            if (b.size() < 6) {
                return _fail(errc::corrupt, at, "jpeg: SOF too short");
            }
            if (b[0] != 8) {
                return _fail(errc::unsupported, at, "jpeg: " + std::to_string(b[0]) + "-bit samples");
            }
            _height = uint32_t(b[1]) << 8 | b[2];
            _width = uint32_t(b[3]) << 8 | b[4];
            _ncomp = b[5];
            if (_height == 0) {
                return _fail(errc::unsupported, at, "jpeg: DNL (the height after the scan)");
            }
            if (_width == 0) {
                return _fail(errc::corrupt, at, "jpeg: width 0");
            }
            if (_ncomp != 1 && _ncomp != 3 && _ncomp != 4) {
                return _fail(_ncomp == 0 ? errc::corrupt : errc::unsupported, at, "jpeg: " + std::to_string(_ncomp) + " components");
            }
            if (b.size() != 6 + 3 * size_t(_ncomp)) {
                return _fail(errc::corrupt, at, "jpeg: SOF of the wrong length");
            }
            for (unsigned c = 0; c < _ncomp; ++c) {
                Component& k = _comp[c];
                k.id = b[6 + 3 * c];
                k.h = b[7 + 3 * c] >> 4;
                k.v = b[7 + 3 * c] & 15;
                k.tq = b[8 + 3 * c];
                if (k.h < 1 || k.h > 4 || k.v < 1 || k.v > 4 || k.tq > 3) {
                    return _fail(errc::corrupt, at, "jpeg: component sampling or table out of range");
                }
                for (unsigned d = 0; d < c; ++d) {
                    if (_comp[d].id == k.id) {
                        return _fail(errc::corrupt, at, "jpeg: two components of one id");
                    }
                }
                _hmax = std::max<unsigned>(_hmax, k.h);
                _vmax = std::max<unsigned>(_vmax, k.v);
            }
            for (unsigned c = 0; c < _ncomp; ++c) {
                if (_hmax % _comp[c].h || _vmax % _comp[c].v) {
                    return _fail(errc::unsupported, at, "jpeg: a sampling factor that does not divide the largest");
                }
            }
            if (auto e = check_size(_width, _height, _o.limits, at)) {
                _err = *e;
                return false;
            }
            _have_frame = true;
            return true;
        }

        // ---- scans ---------------------------------------------------------

        bool _sos(uint64_t at) noexcept(NothrowInput<Input>) {
            if (!_have_frame) {
                return _fail(errc::corrupt, at, "jpeg: SOS before SOF");
            }
            std::vector<uint8_t> b;
            if (!_segment(at, b)) {
                return false;
            }
            if (b.empty()) {
                return _fail(errc::corrupt, at, "jpeg: SOS too short");
            }
            const unsigned ns = b[0];
            if (ns < 1 || ns > _ncomp || b.size() != 4 + 2 * size_t(ns)) {
                return _fail(errc::corrupt, at, "jpeg: SOS of " + std::to_string(ns) + " components");
            }
            Component* scan[4];
            for (unsigned i = 0; i < ns; ++i) {
                const uint8_t id = b[1 + 2 * i];
                Component* k = nullptr;
                for (unsigned c = 0; c < _ncomp; ++c) {
                    if (_comp[c].id == id) {
                        k = &_comp[c];
                    }
                }
                if (!k) {
                    return _fail(errc::corrupt, at, "jpeg: SOS names a component the frame has not");
                }
                for (unsigned j = 0; j < i; ++j) {
                    if (scan[j] == k) {
                        return _fail(errc::corrupt, at, "jpeg: SOS names a component twice");
                    }
                }
                k->td = b[2 + 2 * i] >> 4;
                k->ta = b[2 + 2 * i] & 15;
                if (k->td > 3 || k->ta > 3) {
                    return _fail(errc::corrupt, at, "jpeg: SOS table number past 3");
                }
                if (!k->latched) {
                    if (!_quant_defined[k->tq]) {
                        return _fail(errc::corrupt, at, "jpeg: a quantization table the scan uses is not defined");
                    }
                    std::memcpy(k->quant, _quant[k->tq], sizeof(k->quant));
                    k->latched = true;
                }
                k->dc = 0;
                scan[i] = k;
            }
            const size_t p = 1 + 2 * size_t(ns);
            const unsigned ss = b[p], se = b[p + 1], ah = b[p + 2] >> 4, al = b[p + 2] & 15;
            if (!_progressive) {
                if (ss != 0 || se != 63 || ah != 0 || al != 0) {
                    return _fail(errc::corrupt, at, "jpeg: spectral selection or approximation of a sequential scan");
                }
            } else if (ss > se || se > 63 || (ss == 0 && se != 0) || (ss != 0 && ns != 1) || ah > 13 || al > 13) {
                // G.1.1.1: a band within 0..63, DC alone, AC of one component
                return _fail(errc::corrupt, at, "jpeg: progressive scan of Ss " + std::to_string(ss) + ", Se " + std::to_string(se) +
                                                    ", Ah " + std::to_string(ah) + ", Al " + std::to_string(al) + " over " + std::to_string(ns) + " components");
            }
            // the tables the scan's kind uses: DC codes for DC first, AC codes
            // for AC, both for a sequential scan; a table 0 or 1 never
            // defined is Annex K's
            const bool uses_dc = !_progressive || (ss == 0 && ah == 0);
            const bool uses_ac = !_progressive || ss != 0;
            for (unsigned i = 0; i < ns; ++i) {
                if ((uses_dc && !_table(true, scan[i]->td)) || (uses_ac && !_table(false, scan[i]->ta))) {
                    return _fail(errc::corrupt, at, "jpeg: a Huffman table the scan uses is not defined");
                }
            }
            if (ns > 1) {
                unsigned blocks = 0;
                for (unsigned i = 0; i < ns; ++i) {
                    blocks += scan[i]->h * scan[i]->v;
                }
                if (blocks > 10) {
                    return _fail(errc::corrupt, at, "jpeg: an MCU of more than 10 blocks");
                }
            }
            if (!_started) {
                if (!_start(ns == _ncomp && !_progressive)) {
                    return false;
                }
            } else if (_streaming) {
                // every component was in the first scan: the image is made
                return true;
            }
            if (_progressive) {
                return _scan_progressive(scan, ns, ss, se, ah, al);
            }
            return _scan(scan, ns);
        }

        // The table of that class and number, Annex K's for 0 and 1 when
        // the file defines none (Motion-JPEG), as libjpeg-turbo does
        SGCL_INLINE_HOT bool _table(bool dc, unsigned number) noexcept {
            HuffmanTable& t = dc ? _dc[number] : _ac[number];
            if (t.defined) {
                return true;
            }
            return number <= 1 && annex_k::build(t, dc, number);
        }

        // At the first scan: the color space, the image, the buffers
        bool _start(bool one_scan) noexcept {
            _started = true;
            _streaming = one_scan;
            if (_ncomp == 1) {
                _space = Space::gray;
                _native = pixel_format::gray8;
                _hmax = _vmax = 1;
                _comp[0].h = _comp[0].v = 1;
            } else if (_ncomp == 3) {
                const bool named_rgb = _comp[0].id == 'R' && _comp[1].id == 'G' && _comp[2].id == 'B';
                if (_jfif) {
                    _space = Space::ycc;
                } else if (_adobe) {
                    _space = _adobe_transform == 0 ? Space::rgb : Space::ycc;
                } else {
                    _space = named_rgb ? Space::rgb : Space::ycc;
                }
                _native = pixel_format::rgb8;
            } else {
                _space = _adobe && _adobe_transform == 2 ? Space::ycck : Space::cmyk;
                _native = pixel_format::cmyk8;
            }
            _out = _o.want.value_or(_native);
            _mcux = (_width + 8 * _hmax - 1) / (8 * _hmax);
            _mcuy = (_height + 8 * _vmax - 1) / (8 * _vmax);
            size_t total = 0;
            size_t up_max = 0;
            size_t coefs = 0;
            for (unsigned c = 0; c < _ncomp; ++c) {
                Component& k = _comp[c];
                k.cw = uint32_t((uint64_t(_width) * k.h + _hmax - 1) / _hmax);
                k.ch = uint32_t((uint64_t(_height) * k.v + _vmax - 1) / _vmax);
                k.pw = size_t(_mcux) * k.h * 8;
                k.rows = k.v * 8u;
                k.slots = _streaming || _progressive ? 3u : _mcuy;
                total += k.pw * k.rows * k.slots;
                if (_ncomp == 1) {
                    k.bw = (k.cw + 7) / 8;
                    k.bh = (k.ch + 7) / 8;
                } else {
                    k.bw = _mcux * k.h;
                    k.bh = _mcuy * k.v;
                }
                coefs += size_t(k.bw) * k.bh * 64;
                up_max = std::max(up_max, size_t(k.cw) * (_hmax / k.h) + 16);
            }
            _image.emplace(_width, _height, _out);
            _pixels = reinterpret_cast<uint8_t*>(ImageAccess::state(*_image).pixels.data());
            _stride = ImageAccess::state(*_image).stride;
            _convert = _out == _native ? nullptr : converter(_native, _out);
            const size_t native_row = size_t(_width) * bytes_per_pixel(_native) + 16;
            _memory.reset(new uint8_t[total + _ncomp * up_max + native_row]);
            uint8_t* m = _memory.get();
            for (unsigned c = 0; c < _ncomp; ++c) {
                _comp[c].buffer = m;
                m += _comp[c].pw * _comp[c].rows * _comp[c].slots;
            }
            for (unsigned c = 0; c < _ncomp; ++c) {
                _comp[c].up = m;
                m += up_max;
            }
            _native_row = m;
            if (_progressive) {
                // every coefficient of the image, zero until a scan sets it
                _coefs.reset(new int16_t[coefs]());
                int16_t* q = _coefs.get();
                for (unsigned c = 0; c < _ncomp; ++c) {
                    _comp[c].coef = q;
                    q += size_t(_comp[c].bw) * _comp[c].bh * 64;
                }
            }
            return true;
        }

        // Row r of component k's buffer (r whole, not clamped)
        SGCL_INLINE_HOT uint8_t* _raw_row(Component& k, size_t r) noexcept {
            const size_t slot = (r / k.rows) % k.slots;
            return k.buffer + (slot * k.rows + r % k.rows) * k.pw;
        }

        // Row r clamped to the component's real rows: the edge row
        // again above the first and below the last
        SGCL_INLINE_HOT const uint8_t* _row(Component& k, int64_t r) noexcept {
            if (r < 0) {
                r = 0;
            }
            if (r > int64_t(k.ch) - 1) {
                r = int64_t(k.ch) - 1;
            }
            return _raw_row(k, size_t(r));
        }

        // ---- entropy-coded data --------------------------------------------

        // The bits of the scan, most significant first, the stuffed zero
        // after each 0xFF taken out. At a marker (or the end) the bits run
        // on as zeros, counted, so that decoding only reads past what is
        // there when the data really ends early (checked after each MCU)
        //
        // The common case first: the next eight bytes there and none of them
        // 0xFF (no stuffing, no marker), the whole bytes that fit taken at
        // once, as the loop below would take them one by one
        bool _fill() noexcept(NothrowInput<Input>) {
            if (!_marker && !_eod) {
                const uint8_t* p;
                size_t got;
                if (_in.peek(8, p, got) && got == 8) {
                    uint64_t w = 0;
                    for (int i = 0; i < 8; ++i) {
                        w = w << 8 | p[i];
                    }
                    // a byte of w is 0xFF where its complement has a zero byte
                    const uint64_t c = ~w;
                    if (((c - 0x0101010101010101u) & ~c & 0x8080808080808080u) == 0) {
                        const int n = (64 - _bits) >> 3;
                        if (n > 0) {
                            const uint64_t whole = n == 8 ? w : w & ~(~uint64_t(0) >> (8 * n));
                            _buf |= whole >> _bits;
                            _bits += 8 * n;
                            _in.consume(size_t(n));
                        }
                        return true;
                    }
                }
            }
            while (_bits <= 56) {
                int b = 0;
                if (!_marker && !_eod) {
                    if (!_byte(b)) {
                        _in_failed = true;
                        b = -1;
                    }
                    if (b < 0) {
                        _eod = true;
                    } else if (b == 0xFF) {
                        int c;
                        if (!_byte(c)) {
                            _in_failed = true;
                            c = -1;
                        }
                        if (c == 0) {
                            b = 0xFF;
                        } else if (c < 0) {
                            _eod = true;
                        } else {
                            _marker = true;
                            _marker_code = c;
                        }
                    }
                }
                if (_marker || _eod) {
                    _phantom += 8;
                    b = 0;
                }
                _buf |= uint64_t(uint8_t(b)) << (56 - _bits);
                _bits += 8;
            }
            return true;
        }

        SGCL_INLINE_HOT uint32_t _get(int n) noexcept(NothrowInput<Input>) {
            if (_bits < n) {
                _fill();
            }
            const uint32_t v = uint32_t(_buf >> (64 - n));
            _buf <<= n;
            _bits -= n;
            return v;
        }

        // The symbol of the next code; -1 for bits that are no code
        int _decode(const HuffmanTable& t) noexcept(NothrowInput<Input>) {
            if (_bits < 16) {
                _fill();
            }
            const unsigned look = unsigned(_buf >> (64 - HuffmanTable::FastBits));
            const uint16_t e = t.fast[look];
            if (e) {
                const int l = e >> 8;
                _buf <<= l;
                _bits -= l;
                return e & 0xFF;
            }
            for (int l = HuffmanTable::FastBits + 1; l <= 16; ++l) {
                const int32_t code = int32_t(_buf >> (64 - l));
                if (code <= t.maxcode[l]) {
                    _buf <<= l;
                    _bits -= l;
                    return t.symbols[code + t.offset[l]];
                }
            }
            return -1;
        }

        // s bits as a signed value (F.2.2.1, EXTEND)
        SGCL_INLINE_HOT int _receive_extend(int s) noexcept(NothrowInput<Input>) {
            if (s == 0) {
                return 0;
            }
            const int v = int(_get(s));
            return v < (1 << (s - 1)) ? v - (1 << s) + 1 : v;
        }

        bool _block(Component& k, uint8_t* out, size_t stride) noexcept(NothrowInput<Input>) {
            int16_t coef[64] = {};
            const int s = _decode(_dc[k.td]);
            if (s < 0 || s > 15) {
                return _data_error("jpeg: a DC code with no symbol");
            }
            k.dc += _receive_extend(s);
            coef[0] = int16_t(k.dc);
            const HuffmanTable& ac = _ac[k.ta];
            // whether an AC coefficient came (EXTEND never gives 0 for a
            // size above 0): without one the block is its DC alone
            bool any_ac = false;
            for (int i = 1; i < 64;) {
                // the common case: code and value in one lookup
                if (_bits < 16) {
                    _fill();
                }
                const HuffmanTable::AcFast f = ac.ac_fast[unsigned(_buf >> (64 - HuffmanTable::FastBits))];
                if (f.length) {
                    _buf <<= f.length;
                    _bits -= f.length;
                    i += f.run;
                    if (i > 63) {
                        return _data_error("jpeg: a coefficient past the 64th");
                    }
                    coef[ZigZag[i]] = f.value;
                    any_ac = true;
                    ++i;
                    continue;
                }
                const int rs = _decode(ac);
                if (rs < 0) {
                    return _data_error("jpeg: an AC code with no symbol");
                }
                const int r = rs >> 4, sz = rs & 15;
                if (sz) {
                    i += r;
                    if (i > 63) {
                        return _data_error("jpeg: a coefficient past the 64th");
                    }
                    coef[ZigZag[i]] = int16_t(_receive_extend(sz));
                    any_ac = true;
                    ++i;
                } else if (r == 15) {
                    i += 16;
                } else {
                    break;   // EOB
                }
            }
            if (!any_ac) {
                idct_dc_only_fill(coef[0], k.quant[0], out, stride);
                return true;
            }
            idct_islow(coef, k.quant, out, stride);
            return true;
        }

        // Bits that are no code, or a value out of place: corrupt data; but
        // when the source failed or the data ended within the scan, what the
        // zeros after the end decode to is not the file's, and the error is
        // the source's or the end's
        bool _data_error(const std::string& what) noexcept {
            if (_in_failed) {
                return _fail_input();
            }
            if (_eod) {
                return _fail(errc::unexpected_end, _in.offset(), "jpeg: the data ends in a scan");
            }
            return _fail(errc::corrupt, _in.offset(), what);
        }

        // After an MCU: the data read past its end is an error, of the
        // source's, of an end or of a marker too early
        SGCL_INLINE_HOT bool _check_bits() noexcept {
            if (_in_failed) {
                return _fail_input();
            }
            if (_bits < _phantom) {
                if (_eod) {
                    return _fail(errc::unexpected_end, _in.offset(), "jpeg: the data ends in a scan");
                }
                return _fail(errc::corrupt, _in.offset(), "jpeg: a scan's data ends before its MCUs");
            }
            return true;
        }

        // A restart: the bits to the byte dropped, the marker RSTn next
        // (the bytes before it passed over), the DC predictions from 0
        bool _restart(Component* const* scan, unsigned ns) noexcept(NothrowInput<Input>) {
            if (!_marker) {
                _buf = 0;
                _bits = 0;
                _phantom = 0;
                for (;;) {
                    int b;
                    if (!_byte(b)) {
                        return false;
                    }
                    if (b < 0) {
                        return _fail(errc::unexpected_end, _in.offset(), "jpeg: the data ends before a restart marker");
                    }
                    if (b != 0xFF) {
                        continue;
                    }
                    int c;
                    do {
                        if (!_byte(c)) {
                            return false;
                        }
                    } while (c == 0xFF);
                    if (c < 0) {
                        return _fail(errc::unexpected_end, _in.offset(), "jpeg: the data ends before a restart marker");
                    }
                    if (c != 0) {
                        _marker_code = c;
                        break;
                    }
                }
            }
            if (_marker_code != 0xD0 + int(_next_restart)) {
                return _fail(errc::corrupt, _in.offset(), "jpeg: restart marker out of turn");
            }
            _next_restart = (_next_restart + 1) & 7;
            _marker = false;
            _eod = false;
            _buf = 0;
            _bits = 0;
            _phantom = 0;
            for (unsigned i = 0; i < ns; ++i) {
                scan[i]->dc = 0;
            }
            _eobrun = 0;
            return true;
        }

        bool _scan(Component* const* scan, unsigned ns) noexcept(NothrowInput<Input>) {
            _buf = 0;
            _bits = 0;
            _phantom = 0;
            _marker = false;
            _eod = false;
            _next_restart = 0;
            uint32_t mcux, mcuy;
            if (ns == 1) {
                // one component: an MCU is one block, over the component's own extent
                mcux = (scan[0]->cw + 7) / 8;
                mcuy = (scan[0]->ch + 7) / 8;
            } else {
                mcux = _mcux;
                mcuy = _mcuy;
            }
            uint64_t count = 0;
            for (uint32_t my = 0; my < mcuy; ++my) {
                for (uint32_t mx = 0; mx < mcux; ++mx) {
                    if (_restart_interval && count && count % _restart_interval == 0) {
                        if (!_restart(scan, ns)) {
                            return false;
                        }
                    }
                    if (ns == 1) {
                        Component& k = *scan[0];
                        if (!_block(k, _raw_row(k, size_t(my) * 8) + size_t(mx) * 8, k.pw)) {
                            return false;
                        }
                    } else {
                        for (unsigned i = 0; i < ns; ++i) {
                            Component& k = *scan[i];
                            for (unsigned by = 0; by < k.v; ++by) {
                                uint8_t* row = _raw_row(k, (size_t(my) * k.v + by) * 8);
                                for (unsigned bx = 0; bx < k.h; ++bx) {
                                    if (!_block(k, row + (size_t(mx) * k.h + bx) * 8, k.pw)) {
                                        return false;
                                    }
                                }
                            }
                        }
                    }
                    if (!_check_bits()) {
                        return false;
                    }
                    ++count;
                }
                if (_streaming && my >= 1) {
                    _emit(my - 1);
                }
            }
            if (_streaming) {
                _emit(mcuy - 1);
            }
            for (unsigned i = 0; i < ns; ++i) {
                scan[i]->decoded = true;
            }
            // the marker that ended the data is the next one read
            if (_marker) {
                _pending = true;
                _pending_code = _marker_code;
            }
            return true;
        }

        // ---- progressive (T.81 Annex G) ------------------------------------

        // DC, first scan (G.1.2.1): the difference as in F.2.2.1, the value
        // scaled up by the point transform
        SGCL_INLINE_HOT bool _dc_first(Component& k, int16_t* block, unsigned al) noexcept(NothrowInput<Input>) {
            const int s = _decode(_dc[k.td]);
            if (s < 0 || s > 15) {
                return _data_error("jpeg: a DC code with no symbol");
            }
            k.dc += _receive_extend(s);
            block[0] = int16_t(k.dc * (1 << al));
            return true;
        }

        // DC, a later scan: the next bit of the value, as it is
        SGCL_INLINE_HOT void _dc_refine(int16_t* block, unsigned al) noexcept(NothrowInput<Input>) {
            if (_get(1)) {
                block[0] = int16_t(block[0] | (1 << al));
            }
        }

        // AC, first scan of the band (G.1.2.2): the run and size of each
        // value as in the sequential mode, and EOBn: this block and the
        // next 2^n - 1 + (n bits) blocks end their band here (Table G.1)
        bool _ac_first(Component& k, int16_t* block, unsigned ss, unsigned se, unsigned al) noexcept(NothrowInput<Input>) {
            if (_eobrun) {
                --_eobrun;
                return true;
            }
            for (unsigned i = ss; i <= se; ++i) {
                const int rs = _decode(_ac[k.ta]);
                if (rs < 0) {
                    return _data_error("jpeg: an AC code with no symbol");
                }
                const unsigned r = unsigned(rs) >> 4, sz = unsigned(rs) & 15;
                if (sz) {
                    i += r;
                    if (i > se) {
                        return _data_error("jpeg: a coefficient past the band");
                    }
                    block[ZigZag[i]] = int16_t(_receive_extend(int(sz)) * (1 << al));
                } else if (r == 15) {
                    i += 15;   // ZRL: sixteen zeros, the loop's step the last
                } else {
                    _eobrun = (1u << r) - 1;
                    if (r) {
                        _eobrun += _get(int(r));
                    }
                    break;
                }
            }
            return true;
        }

        // The correction bit of a coefficient already nonzero: its next
        // bit (G.1.2.3 b), added away from zero when set
        SGCL_INLINE_HOT void _correct(int16_t& c, int p1) noexcept(NothrowInput<Input>) {
            if (_get(1) && (c & p1) == 0) {
                c = int16_t(c >= 0 ? c + p1 : c - p1);
            }
        }

        // AC, a later scan of the band (G.1.2.3): each value new to it is
        // ±1 at the bit (a sign bit after its code); the zeros of its run
        // count only coefficients still zero, and every coefficient
        // already nonzero passed on the way takes a correction bit, those
        // after the last value too, and all of them in the blocks of an
        // EOB run
        bool _ac_refine(Component& k, int16_t* block, unsigned ss, unsigned se, unsigned al) noexcept(NothrowInput<Input>) {
            const int p1 = 1 << al;
            unsigned i = ss;
            if (_eobrun == 0) {
                for (; i <= se; ++i) {
                    const int rs = _decode(_ac[k.ta]);
                    if (rs < 0) {
                        return _data_error("jpeg: an AC code with no symbol");
                    }
                    unsigned r = unsigned(rs) >> 4;
                    const unsigned sz = unsigned(rs) & 15;
                    int value = 0;
                    if (sz) {
                        if (sz != 1) {
                            return _data_error("jpeg: a refinement of size " + std::to_string(sz));
                        }
                        value = _get(1) ? p1 : -p1;
                    } else if (r != 15) {
                        _eobrun = 1u << r;
                        if (r) {
                            _eobrun += _get(int(r));
                        }
                        break;
                    }
                    // r coefficients still zero passed, the corrections of
                    // the nonzero ones among them; i stops on the next zero
                    for (; i <= se; ++i) {
                        int16_t& c = block[ZigZag[i]];
                        if (c != 0) {
                            _correct(c, p1);
                        } else {
                            if (r == 0) {
                                break;
                            }
                            --r;
                        }
                    }
                    if (value) {
                        if (i > se) {
                            return _data_error("jpeg: a coefficient past the band");
                        }
                        block[ZigZag[i]] = int16_t(value);
                    }
                }
            }
            if (_eobrun > 0) {
                SGCL_JPEG_ALIGN_LOOP
                for (; i <= se; ++i) {
                    int16_t& c = block[ZigZag[i]];
                    if (c != 0) {
                        _correct(c, p1);
                    }
                }
                --_eobrun;
            }
            return true;
        }

        SGCL_INLINE_HOT bool _progressive_block(Component& k, int16_t* block, unsigned ss, unsigned se, unsigned ah, unsigned al) noexcept(NothrowInput<Input>) {
            if (ss == 0) {
                if (ah == 0) {
                    return _dc_first(k, block, al);
                }
                _dc_refine(block, al);
                return true;
            }
            return ah == 0 ? _ac_first(k, block, ss, se, al) : _ac_refine(k, block, ss, se, al);
        }

        bool _scan_progressive(Component* const* scan, unsigned ns, unsigned ss, unsigned se, unsigned ah, unsigned al) noexcept(NothrowInput<Input>) {
            _buf = 0;
            _bits = 0;
            _phantom = 0;
            _marker = false;
            _eod = false;
            _next_restart = 0;
            _eobrun = 0;
            uint32_t mcux, mcuy;
            if (ns == 1) {
                mcux = (scan[0]->cw + 7) / 8;
                mcuy = (scan[0]->ch + 7) / 8;
            } else {
                mcux = _mcux;
                mcuy = _mcuy;
            }
            uint64_t count = 0;
            for (uint32_t my = 0; my < mcuy; ++my) {
                for (uint32_t mx = 0; mx < mcux; ++mx) {
                    if (_restart_interval && count && count % _restart_interval == 0) {
                        if (!_restart(scan, ns)) {
                            return false;
                        }
                    }
                    if (ns == 1) {
                        Component& k = *scan[0];
                        if (!_progressive_block(k, k.coef + (size_t(my) * k.bw + mx) * 64, ss, se, ah, al)) {
                            return false;
                        }
                    } else {
                        for (unsigned i = 0; i < ns; ++i) {
                            Component& k = *scan[i];
                            for (unsigned by = 0; by < k.v; ++by) {
                                for (unsigned bx = 0; bx < k.h; ++bx) {
                                    int16_t* block = k.coef + ((size_t(my) * k.v + by) * k.bw + size_t(mx) * k.h + bx) * 64;
                                    if (!_progressive_block(k, block, ss, se, ah, al)) {
                                        return false;
                                    }
                                }
                            }
                        }
                    }
                    if (!_check_bits()) {
                        return false;
                    }
                    ++count;
                }
            }
            for (unsigned i = 0; i < ns; ++i) {
                scan[i]->decoded = true;
            }
            if (_marker) {
                _pending = true;
                _pending_code = _marker_code;
            }
            return true;
        }

        // The blocks of MCU row m of every component, transformed into
        // its ring of rows
        void _transform_row(uint32_t m) noexcept {
            for (unsigned c = 0; c < _ncomp; ++c) {
                Component& k = _comp[c];
                const uint32_t per = _ncomp == 1 ? 1u : k.v;
                for (uint32_t br = m * per; br < (m + 1) * per && br < k.bh; ++br) {
                    uint8_t* row = _raw_row(k, size_t(br) * 8);
                    const int16_t* q = k.coef + size_t(br) * k.bw * 64;
                    for (uint32_t bx = 0; bx < k.bw; ++bx) {
                        idct_islow(q + size_t(bx) * 64, k.quant, row + size_t(bx) * 8, k.pw);
                    }
                }
            }
        }

        // ---- rows ----------------------------------------------------------

        // The image rows of MCU row m (in the scan's MCU rows): each
        // component upsampled to the row, the row converted
        void _emit(uint32_t m) noexcept {
            const uint32_t per = (_ncomp == 1 ? 1u : _vmax) * 8;
            const uint32_t y0 = m * per;
            const uint32_t y1 = std::min(_height, y0 + per);
            for (uint32_t y = y0; y < y1; ++y) {
                _emit_row(y);
            }
        }

        const uint8_t* _upsampled(Component& k, uint32_t y) noexcept {
            const unsigned rv = _vmax / k.v, rh = _hmax / k.h;
            const size_t n = k.cw;
            if (rv == 1) {
                const uint8_t* in = _row(k, y);
                if (rh == 1) {
                    return in;
                }
                if (rh == 2 && n > 2) {
                    upsample::h2(in, k.up, n);
                } else {
                    upsample::box(in, k.up, n, rh);
                }
                return k.up;
            }
            if (rv == 2) {
                const int64_t r = y / 2;
                const bool lower = y & 1;
                const uint8_t* in = _row(k, r);
                const uint8_t* near = _row(k, lower ? r + 1 : r - 1);
                if (rh == 2 && n > 2) {
                    upsample::h2v2(in, near, k.up, n);
                } else if (rh == 1) {
                    upsample::v2(in, near, lower, k.up, n);
                } else {
                    upsample::box(in, k.up, n, rh);
                }
                return k.up;
            }
            const uint8_t* in = _row(k, y / rv);
            if (rh == 1) {
                return in;
            }
            upsample::box(in, k.up, n, rh);
            return k.up;
        }

        void _emit_row(uint32_t y) noexcept {
            const size_t w = _width;
            uint8_t* image_row = _pixels + size_t(y) * _stride;
            uint8_t* dst = _convert ? _native_row : image_row;
            switch (_space) {
                case Space::gray:
                    sgcl::detail::copy_bytes(dst, _upsampled(_comp[0], y), w);
                    break;
                case Space::ycc:
                    ycc_to_rgb(_upsampled(_comp[0], y), _upsampled(_comp[1], y), _upsampled(_comp[2], y), dst, w);
                    break;
                case Space::rgb: {
                    const uint8_t* r = _upsampled(_comp[0], y);
                    const uint8_t* g = _upsampled(_comp[1], y);
                    const uint8_t* b = _upsampled(_comp[2], y);
                    for (size_t i = 0; i < w; ++i) {
                        dst[3 * i] = r[i];
                        dst[3 * i + 1] = g[i];
                        dst[3 * i + 2] = b[i];
                    }
                    break;
                }
                case Space::cmyk:
                case Space::ycck: {
                    const uint8_t* c0 = _upsampled(_comp[0], y);
                    const uint8_t* c1 = _upsampled(_comp[1], y);
                    const uint8_t* c2 = _upsampled(_comp[2], y);
                    const uint8_t* c3 = _upsampled(_comp[3], y);
                    // as the file has them; YCCK to CMYK by the complement of
                    // YCbCr's RGB (K as it is); then Adobe's inversion undone
                    const uint8_t flip = _adobe ? 255 : 0;
                    if (_space == Space::cmyk) {
                        for (size_t i = 0; i < w; ++i) {
                            dst[4 * i] = uint8_t(c0[i] ^ flip);
                            dst[4 * i + 1] = uint8_t(c1[i] ^ flip);
                            dst[4 * i + 2] = uint8_t(c2[i] ^ flip);
                            dst[4 * i + 3] = uint8_t(c3[i] ^ flip);
                        }
                    } else {
                        const auto& t = ycc_tables();
                        for (size_t i = 0; i < w; ++i) {
                            const int Y = c0[i];
                            dst[4 * i] = uint8_t(uint8_t(255 - clamp255(Y + t.cr_r[c2[i]])) ^ flip);
                            dst[4 * i + 1] = uint8_t(uint8_t(255 - clamp255(Y + ((t.cb_g[c1[i]] + t.cr_g[c2[i]]) >> 16))) ^ flip);
                            dst[4 * i + 2] = uint8_t(uint8_t(255 - clamp255(Y + t.cb_b[c1[i]])) ^ flip);
                            dst[4 * i + 3] = uint8_t(c3[i] ^ flip);
                        }
                    }
                    break;
                }
            }
            if (_convert) {
                _convert(reinterpret_cast<const std::byte*>(_native_row), reinterpret_cast<std::byte*>(image_row), w);
            }
        }

        // ---- the end -------------------------------------------------------

        expected<image, error> _finish(uint64_t at) noexcept {

            if (!_started) {
                return unexpected(error(errc::corrupt, at, "jpeg: no scan before EOI"));
            }
            if (!_streaming) {
                for (unsigned c = 0; c < _ncomp; ++c) {
                    if (!_comp[c].decoded) {
                        return unexpected(error(errc::corrupt, at, "jpeg: a component no scan holds"));
                    }
                }
                if (_progressive) {
                    // the coefficients whole: transformed MCU row by MCU row
                    // into the rings, each row out when the next is in
                    const uint32_t rows = _ncomp == 1 ? _comp[0].bh : _mcuy;
                    for (uint32_t m = 0; m < rows; ++m) {
                        _transform_row(m);
                        if (m >= 1) {
                            _emit(m - 1);
                        }
                    }
                    _emit(rows - 1);
                } else {
                    for (uint32_t m = 0; m < _mcuy; ++m) {
                        _emit(m);
                    }
                }
            }
            auto& s = ImageAccess::state(*_image);
            if (!_exif.empty()) {
                ImageAccess::set_orientation(*_image, exif_orientation(reinterpret_cast<const uint8_t*>(_exif.data()), _exif.size()));
                s.exif = std::move(_exif);
            }
            if (_icc_count && !_icc_broken) {
                bool whole = _icc_seen.size() == _icc_count;
                for (bool seen : _icc_seen) {
                    whole = whole && seen;
                }
                if (whole) {
                    for (const auto& chunk : _icc_chunks) {
                        s.icc.insert(s.icc.end(), reinterpret_cast<const byte*>(chunk.data()), reinterpret_cast<const byte*>(chunk.data() + chunk.size()));
                    }
                }
            }
            return std::move(*_image);
        }

        Input& _in;
        const decode_options& _o;
        optional<error> _err;

        // the marker a scan's data ended at, read with it
        bool _pending = false;
        int _pending_code = 0;

        // tables
        uint16_t _quant[4][64] = {};
        bool _quant_defined[4] = {};
        HuffmanTable _dc[4];
        HuffmanTable _ac[4];
        unsigned _restart_interval = 0;

        // the frame
        bool _have_frame = false;
        uint32_t _width = 0, _height = 0;
        unsigned _ncomp = 0, _hmax = 1, _vmax = 1;
        Component _comp[4];
        uint32_t _mcux = 0, _mcuy = 0;
        bool _jfif = false, _adobe = false;
        uint8_t _adobe_transform = 1;
        Space _space = Space::ycc;

        // metadata
        vector<byte> _exif;
        std::vector<std::vector<uint8_t>> _icc_chunks;
        std::vector<bool> _icc_seen;
        unsigned _icc_count = 0;
        size_t _icc_total = 0;
        bool _icc_broken = false;

        // the image and its rows
        bool _started = false, _streaming = true, _progressive = false;
        std::unique_ptr<int16_t[]> _coefs;
        uint32_t _eobrun = 0;
        optional<image> _image;
        uint8_t* _pixels = nullptr;
        size_t _stride = 0;
        pixel_format _native = pixel_format::rgb8, _out = pixel_format::rgb8;
        ConvertRow _convert = nullptr;
        std::unique_ptr<uint8_t[]> _memory;
        uint8_t* _native_row = nullptr;

        // the bits of a scan
        uint64_t _buf = 0;
        int _bits = 0, _phantom = 0;
        bool _marker = false, _eod = false, _in_failed = false;
        int _marker_code = 0;
        unsigned _next_restart = 0;
    };
}
