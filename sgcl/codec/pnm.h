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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace sgcl::codec {
    namespace detail {
        // pnm::kind and pnm::options, outside the class (a nested struct with
        // member initializers cannot be a default argument inside its class)
        enum class PnmKind : uint8_t {
            automatic,   // PGM for gray, PPM for color, PAM for an image with alpha
            pbm,         // P1/P4: black and white, a pixel black where its luma is below half
            pgm,         // P2/P5: gray
            ppm,         // P3/P6: RGB
            pam          // P7: the image's own channels, gray or RGB, with or without alpha
        };

        struct PnmOptions {
            PnmKind kind = PnmKind::automatic;
            bool plain = false;   // the ASCII forms P1, P2 and P3 (PAM has none: errc::invalid_argument)
        };

        // The Netpbm decoder (netpbm.sourceforge.net's pbm, pgm, ppm and pam
        // pages) over an input of MemoryInput's shape: P1 to P6, plain and
        // raw, and P7 (PAM: DEPTH 1 to 4, TUPLTYPE read past, the depth
        // telling gray, gray with alpha, RGB and RGBA). A maxval of 255 or
        // 65535 gives the samples as they are, 8 or 16 bits; another the
        // samples scaled to 8 bits (maxval under 256) or 16, to the nearest;
        // a sample past maxval is taken as maxval. PBM is gray8, 1 black.
        template<class Input>
        class PnmDecoder {
        public:
            SGCL_INLINE_HOT PnmDecoder(Input& in, const decode_options& o) noexcept
            : _in(in), _o(o) {
            }

            expected<image, error> run() noexcept(NothrowInput<Input>) {
                if (_o.want && !valid(*_o.want)) {
                    return unexpected(error(errc::invalid_argument, 0, "pnm: decode_options.want outside the list"));
                }
                int c0 = _get(), c1 = _get();
                if (c0 != 'P' || c1 < '1' || c1 > '7') {
                    return unexpected(_bad(0, "pnm: not a Netpbm signature (P1 to P7)"));
                }
                const int kind = c1 - '0';
                uint32_t w = 0, h = 0, maxval = 1, depth = 1;
                if (kind == 7) {
                    if (!_pam_header(w, h, depth, maxval)) {
                        return unexpected(_err);
                    }
                } else {
                    if (!_number(w) || !_number(h)) {
                        return unexpected(_err);
                    }
                    if (kind != 1 && kind != 4 && !_number(maxval)) {
                        return unexpected(_err);
                    }
                    depth = kind == 3 || kind == 6 ? 3 : 1;
                    // (the one whitespace byte after the header ended its last number)
                    if (kind >= 4 && _ended_by_end) {
                        return unexpected(_short());
                    }
                }
                if (maxval < 1 || maxval > 65535) {
                    return unexpected(_bad(_in.offset(), "pnm: a maxval outside 1..65535"));
                }
                if (depth < 1 || depth > 4) {
                    return unexpected(_bad(_in.offset(), "pnm: a PAM depth outside 1..4"));
                }
                if (auto e = check_size(w, h, _o.limits, 3)) {
                    return unexpected(*e);
                }
                const bool bits = kind == 1 || kind == 4;
                const bool wide = maxval > 255;
                static constexpr pixel_format formats8[5] = {pixel_format::gray8, pixel_format::gray8, pixel_format::gray_alpha8, pixel_format::rgb8, pixel_format::rgba8};
                static constexpr pixel_format formats16[5] = {pixel_format::gray16, pixel_format::gray16, pixel_format::gray_alpha16, pixel_format::rgb16, pixel_format::rgba16};
                const pixel_format native = bits ? pixel_format::gray8 : wide ? formats16[depth] : formats8[depth];
                const pixel_format f = _o.want ? *_o.want : native;
                image out(w, h, f);
                auto& s = ImageAccess::state(out);
                const ConvertRow convert = f == native ? nullptr : converter(native, f);
                const size_t samples = size_t(w) * depth;
                const size_t row_bytes = size_t(w) * bytes_per_pixel(native);
                std::unique_ptr<uint8_t[]> row(new uint8_t[std::max(row_bytes, (size_t(w) + 7) / 8 + 1)]);
                std::unique_ptr<uint8_t[]> raw(new uint8_t[samples * (wide ? 2 : 1) + 1]);
                const bool exact = maxval == 255 || maxval == 65535;
                for (uint32_t y = 0; y < h; ++y) {
                    uint8_t* dst8 = row.get();
                    if (bits) {
                        if (kind == 4) {
                            const size_t n = (size_t(w) + 7) / 8;
                            if (!_read(raw.get(), n)) {
                                return unexpected(_short());
                            }
                            for (uint32_t x = 0; x < w; ++x) {
                                dst8[x] = (raw[x >> 3] >> (7 - (x & 7))) & 1 ? 0 : 255;
                            }
                        } else {
                            for (uint32_t x = 0; x < w; ++x) {
                                int c;
                                do {
                                    c = _get();
                                    if (c == '#') {
                                        _comment();
                                        c = ' ';
                                    }
                                } while (_space(c));
                                if (c != '0' && c != '1') {
                                    return unexpected(c < 0 ? _short() : _bad(_in.offset(), "pnm: a PBM pixel other than 0 or 1"));
                                }
                                dst8[x] = c == '1' ? 0 : 255;
                            }
                        }
                    } else if (kind >= 4) {
                        const size_t n = samples * (wide ? 2 : 1);
                        if (!_read(raw.get(), n)) {
                            return unexpected(_short());
                        }
                        if (wide) {
                            uint16_t* d = reinterpret_cast<uint16_t*>(dst8);
                            for (size_t i = 0; i < samples; ++i) {
                                const uint32_t v = std::min<uint32_t>(uint32_t(raw[2 * i]) << 8 | raw[2 * i + 1], maxval);
                                const uint16_t sv = exact ? uint16_t(v) : uint16_t((v * 65535u + maxval / 2) / maxval);
                                std::memcpy(d + i, &sv, 2);
                            }
                        } else if (exact) {
                            sgcl::detail::copy_bytes(dst8, raw.get(), samples);
                        } else {
                            for (size_t i = 0; i < samples; ++i) {
                                const uint32_t v = std::min<uint32_t>(raw[i], maxval);
                                dst8[i] = uint8_t((v * 255u + maxval / 2) / maxval);
                            }
                        }
                    } else {
                        // plain: numbers in text
                        for (size_t i = 0; i < samples; ++i) {
                            uint32_t v;
                            if (!_number(v)) {
                                return unexpected(_err);
                            }
                            v = std::min(v, maxval);
                            if (wide) {
                                const uint16_t sv = exact ? uint16_t(v) : uint16_t((v * 65535u + maxval / 2) / maxval);
                                std::memcpy(dst8 + 2 * i, &sv, 2);
                            } else {
                                dst8[i] = exact ? uint8_t(v) : uint8_t((v * 255u + maxval / 2) / maxval);
                            }
                        }
                    }
                    std::byte* d = s.pixels.data() + size_t(y) * s.stride;
                    if (convert) {
                        convert(reinterpret_cast<const std::byte*>(dst8), d, w);
                    } else {
                        sgcl::detail::copy_bytes(d, dst8, row_bytes);
                    }
                }
                return out;
            }

        private:
            static bool _space(int c) noexcept {
                return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
            }

            // A byte, -1 at the end (or a failed stream, its error kept)
            int _get() noexcept(NothrowInput<Input>) {
                const uint8_t* p;
                size_t got;
                if (!_in.peek(1, p, got)) {
                    _io = true;
                    return -1;
                }
                if (got == 0) {
                    return -1;
                }
                const int c = *p;
                _in.consume(1);
                return c;
            }

            void _comment() noexcept(NothrowInput<Input>) {
                int c;
                do {
                    c = _get();
                } while (c >= 0 && c != '\n' && c != '\r');
            }

            // A decimal number of the header or a plain raster, after
            // whitespace and comments; false with the error kept
            bool _number(uint32_t& v) noexcept(NothrowInput<Input>) {
                int c;
                do {
                    c = _get();
                    if (c == '#') {
                        _comment();
                        c = ' ';
                    }
                } while (_space(c));
                if (c < '0' || c > '9') {
                    _err = c < 0 ? _short() : _bad(_in.offset(), "pnm: a number expected");
                    return false;
                }
                uint64_t n = 0;
                while (c >= '0' && c <= '9') {
                    n = n * 10 + uint64_t(c - '0');
                    if (n > 0xFFFFFFFFu) {
                        _err = _bad(_in.offset(), "pnm: a number past 32 bits");
                        return false;
                    }
                    c = _get();
                }
                _ended_by_end = c < 0;
                if (c >= 0 && !_space(c) && c != '#') {
                    _err = _bad(_in.offset(), "pnm: a number followed by what is not whitespace");
                    return false;
                }
                if (c == '#') {
                    _comment();
                }
                v = uint32_t(n);
                return true;
            }

            // PAM's header: lines of a keyword and its value, to ENDHDR
            bool _pam_header(uint32_t& w, uint32_t& h, uint32_t& depth, uint32_t& maxval) noexcept(NothrowInput<Input>) {
                bool have_w = false, have_h = false, have_d = false, have_m = false;
                std::string line;
                for (;;) {
                    line.clear();
                    int c;
                    while ((c = _get()) >= 0 && c != '\n') {
                        if (line.size() < 256) {
                            line.push_back(char(c));
                        }
                    }
                    if (c < 0 && line.empty()) {
                        _err = _short();
                        return false;
                    }
                    size_t a = 0;
                    while (a < line.size() && _space(line[a])) {
                        ++a;
                    }
                    if (a == line.size() || line[a] == '#') {
                        if (c < 0) {
                            _err = _short();
                            return false;
                        }
                        continue;
                    }
                    size_t b = a;
                    while (b < line.size() && !_space(line[b])) {
                        ++b;
                    }
                    const std::string key = line.substr(a, b - a);
                    if (key == "ENDHDR") {
                        break;
                    }
                    if (key == "TUPLTYPE") {
                        continue;
                    }
                    uint64_t n = 0;
                    size_t k = b;
                    while (k < line.size() && _space(line[k])) {
                        ++k;
                    }
                    const size_t start = k;
                    while (k < line.size() && line[k] >= '0' && line[k] <= '9' && n <= 0xFFFFFFFFu) {
                        n = n * 10 + uint64_t(line[k] - '0');
                        ++k;
                    }
                    if (k == start || n > 0xFFFFFFFFu) {
                        _err = _bad(_in.offset(), "pnm: a PAM header line without its number");
                        return false;
                    }
                    if (key == "WIDTH") {
                        w = uint32_t(n);
                        have_w = true;
                    } else if (key == "HEIGHT") {
                        h = uint32_t(n);
                        have_h = true;
                    } else if (key == "DEPTH") {
                        depth = uint32_t(n);
                        have_d = true;
                    } else if (key == "MAXVAL") {
                        maxval = uint32_t(n);
                        have_m = true;
                    } else {
                        _err = _bad(_in.offset(), "pnm: an unknown PAM header line");
                        return false;
                    }
                    if (c < 0) {
                        _err = _short();
                        return false;
                    }
                }
                if (!have_w || !have_h || !have_d || !have_m) {
                    _err = _bad(_in.offset(), "pnm: a PAM header without WIDTH, HEIGHT, DEPTH or MAXVAL");
                    return false;
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

            error _bad(uint64_t at, const char* what) noexcept {
                return error(errc::corrupt, at, string(what));
            }

            error _short() noexcept {
                if (_io && _in.failure) {
                    return *_in.failure;
                }
                return error(errc::unexpected_end, _in.offset(), "pnm: the data ends before the last pixel");
            }

            Input& _in;
            const decode_options& _o;
            error _err;
            bool _io = false;
            bool _ended_by_end = false;   // the last number ended by the end of the data
        };

        // The encoder: the kind asked (automatic by the image's format), raw
        // or plain, maxval 255 or 65535 by the image's depth (PBM: 1)
        template<class Sink>
        bool pnm_encode(const image& im, const PnmOptions& o, Sink& sink) noexcept(NothrowSink<Sink>) {
            const auto& s = ImageAccess::state(im);
            PnmKind kind = o.kind;
            if (kind == PnmKind::automatic) {
                kind = alpha(s.format) ? PnmKind::pam : gray(s.format) ? PnmKind::pgm : PnmKind::ppm;
            }
            if (kind == PnmKind::pam && o.plain) {
                sink.failure = error(errc::invalid_argument, 0, "pnm: PAM has no plain form");
                return false;
            }
            if (kind < PnmKind::automatic || kind > PnmKind::pam) {
                sink.failure = error(errc::invalid_argument, 0, "pnm: options.kind outside the list");
                return false;
            }
            const bool wide16 = wide(s.format) && kind != PnmKind::pbm;
            pixel_format target;
            unsigned depth;
            switch (kind) {
                case PnmKind::pbm:
                case PnmKind::pgm:
                    target = wide16 ? pixel_format::gray16 : pixel_format::gray8;
                    depth = 1;
                    break;
                case PnmKind::ppm:
                    target = wide16 ? pixel_format::rgb16 : pixel_format::rgb8;
                    depth = 3;
                    break;
                default:
                    depth = channels(s.format == pixel_format::cmyk8 ? pixel_format::rgb8 : s.format);
                    target = s.format == pixel_format::cmyk8 ? pixel_format::rgb8 : s.format;
                    break;
            }
            const uint32_t w = s.width, h = s.height;
            std::string head;
            if (kind == PnmKind::pam) {
                static const char* types[5] = {"", "GRAYSCALE", "GRAYSCALE_ALPHA", "RGB", "RGB_ALPHA"};
                head = "P7\nWIDTH " + std::to_string(w) + "\nHEIGHT " + std::to_string(h) + "\nDEPTH " + std::to_string(depth) +
                       "\nMAXVAL " + (wide16 ? "65535" : "255") + "\nTUPLTYPE " + types[depth] + "\nENDHDR\n";
            } else {
                const char magic = kind == PnmKind::pbm ? (o.plain ? '1' : '4') : kind == PnmKind::pgm ? (o.plain ? '2' : '5') : (o.plain ? '3' : '6');
                head = std::string("P") + magic + "\n" + std::to_string(w) + " " + std::to_string(h) + "\n";
                if (kind != PnmKind::pbm) {
                    head += wide16 ? "65535\n" : "255\n";
                }
            }
            if (!sink.put(reinterpret_cast<const uint8_t*>(head.data()), head.size())) {
                return false;
            }
            const ConvertRow convert = s.format == target ? nullptr : converter(s.format, target);
            const size_t row_bytes = size_t(w) * bytes_per_pixel(target);
            std::unique_ptr<uint8_t[]> row(new uint8_t[row_bytes]);
            std::string text;
            std::unique_ptr<uint8_t[]> out(new uint8_t[row_bytes + 8]);
            const uint8_t* pixels = reinterpret_cast<const uint8_t*>(s.pixels.data());
            for (uint32_t y = 0; y < h; ++y) {
                const uint8_t* src = pixels + size_t(y) * s.stride;
                if (convert) {
                    convert(reinterpret_cast<const std::byte*>(src), reinterpret_cast<std::byte*>(row.get()), w);
                    src = row.get();
                }
                const size_t samples = size_t(w) * depth;
                auto sample = [&](size_t i) -> uint32_t {
                    if (wide16) {
                        uint16_t v;
                        std::memcpy(&v, src + 2 * i, 2);
                        return v;
                    }
                    return src[i];
                };
                if (kind == PnmKind::pbm) {
                    // black where the gray is below half
                    if (o.plain) {
                        text.clear();
                        for (uint32_t x = 0; x < w; ++x) {
                            text += sample(x) < 128 ? '1' : '0';
                            text += (x + 1) % 35 == 0 || x + 1 == w ? '\n' : ' ';
                        }
                        if (!sink.put(reinterpret_cast<const uint8_t*>(text.data()), text.size())) {
                            return false;
                        }
                    } else {
                        const size_t n = (size_t(w) + 7) / 8;
                        std::memset(out.get(), 0, n);
                        for (uint32_t x = 0; x < w; ++x) {
                            out[x >> 3] = uint8_t(out[x >> 3] | (sample(x) < 128 ? 0x80 >> (x & 7) : 0));
                        }
                        if (!sink.put(out.get(), n)) {
                            return false;
                        }
                    }
                } else if (o.plain) {
                    text.clear();
                    size_t line = 0;
                    for (size_t i = 0; i < samples; ++i) {
                        const std::string v = std::to_string(sample(i));
                        if (line + v.size() + 1 > 70) {
                            text += '\n';
                            line = 0;
                        } else if (line > 0) {
                            text += ' ';
                            ++line;
                        }
                        text += v;
                        line += v.size();
                    }
                    text += '\n';
                    if (!sink.put(reinterpret_cast<const uint8_t*>(text.data()), text.size())) {
                        return false;
                    }
                } else if (wide16) {
                    for (size_t i = 0; i < samples; ++i) {
                        const uint32_t v = sample(i);
                        out[2 * i] = uint8_t(v >> 8);
                        out[2 * i + 1] = uint8_t(v);
                    }
                    if (!sink.put(out.get(), samples * 2)) {
                        return false;
                    }
                } else if (!sink.put(src, samples)) {
                    return false;
                }
            }
            return true;
        }
    }

    // Netpbm: PBM, PGM, PPM (P1 to P6, plain and raw) and PAM (P7). decode
    // gives gray8 for PBM (1 black), gray, gray with alpha, RGB or RGBA at
    // 8 bits for a maxval up to 255 and 16 above, the samples scaled to the
    // full range where maxval is neither 255 nor 65535. encode writes the
    // kind the options ask, PGM for gray, PPM for color and PAM for an
    // image with alpha by default, raw unless plain.
    class pnm {
    public:
        using kind = detail::PnmKind;
        using options = detail::PnmOptions;

        SGCL_INLINE_HOT static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {}) noexcept {
            detail::MemoryInput in(data);
            return detail::PnmDecoder<detail::MemoryInput>(in, o).run();
        }

        SGCL_INLINE_HOT static expected<image, error> decode(const io::reader& in, const decode_options& o = {}) {
            detail::ReaderInput source(in);
            return detail::PnmDecoder<detail::ReaderInput>(source, o).run();
        }

        // The file as bytes: errc::invalid_argument for PAM asked plain or
        // a kind outside the list
        SGCL_INLINE_HOT static expected<vector<byte>, error> encode(const image& im, const options& o = {}) noexcept {
            vector<byte> out;
            detail::VectorSink sink{out, nullopt};
            if (!detail::pnm_encode(im, o, sink)) {
                return unexpected(*sink.failure);
            }
            return out;
        }

        SGCL_INLINE_HOT static expected<void, error> encode(const image& im, const io::writer& out, const options& o = {}) {
            detail::WriterSink sink{out, 0, nullopt};
            if (!detail::pnm_encode(im, o, sink)) {
                return unexpected(*sink.failure);
            }
            return {};
        }
    };
}
