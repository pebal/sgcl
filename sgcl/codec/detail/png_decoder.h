//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "exif.h"
#include "input.h"
#include "pixels.h"
#include "png_filter.h"
#include "../error.h"
#include "../image.h"
#include "../options.h"
#include "../../compress/detail/inflate.h"
#include "../../compress/zlib.h"
#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"
#include "../../core/expected.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../core/vector.h"
#include "../../hash/adler32.h"
#include "../../hash/crc32.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace sgcl::codec::detail {
    // A chunk's type as the number its four bytes make, big-endian
    SGCL_INLINE_HOT constexpr uint32_t png_tag(const char (&s)[5]) noexcept {
        return uint32_t(uint8_t(s[0])) << 24 | uint32_t(uint8_t(s[1])) << 16 | uint32_t(uint8_t(s[2])) << 8 | uint8_t(s[3]);
    }

    // The PNG decoder (PNG 3rd ed., W3C 2025; ISO/IEC 15948 before it),
    // over an input of MemoryInput's shape: the chunks read in order, each
    // one's CRC-32 checked; the IDAT chunks' data fed to one zlib stream
    // (RFC 1950 around DEFLATE, compress's inflate) as it comes, the rows
    // taken from its output as they complete, unfiltered, expanded to the
    // image's pixel format and written in place (interlaced: each of the
    // seven passes of Adam7 to its pixels). Memory: the image, a window of
    // the zlib stream (its 32 KB of history and room for a row and more),
    // three rows; all of it made when the first IDAT comes, none per row.
    //
    // The pixel format of the image, when decode_options.want asks for
    // none, is the file's own: gray of 1 to 8 bits gray8 (scaled to 8
    // bits), 16 bits gray16, gray with alpha gray_alpha8 or 16, truecolor
    // rgb8 or 16, with alpha rgba8 or 16, a palette always rgba8; a tRNS
    // chunk adds alpha to gray and truecolor (gray_alpha, rgba). 16-bit
    // samples are big-endian in the file and in the byte order of the
    // machine in the image.
    //
    // Strict where the format is (a critical chunk out of place, a bad
    // CRC, a zlib stream that does not check, missing image data) as
    // libpng and Go are; lenient where they are too: an ancillary chunk
    // unknown or out of place (a tRNS of a wrong size, one with an alpha
    // channel) is skipped, a palette index past the palette's end is
    // opaque black (libpng's), the stream's bytes past the image are
    // ignored, and a stream whose rows are all there but whose end never
    // comes is taken (its Adler-32 then unchecked). What is not read:
    // gamma and color (gAMA, cHRM, sRGB, sBIT: no color management), text,
    // time, background, APNG's frames (the default image is the image).
    template<class Input>
    class PngDecoder {
    public:
        PngDecoder(Input& in, const decode_options& o) noexcept
        : _in(in), _o(o) {
            for (auto& entry : _palette) {
                entry[3] = 255;
            }
        }

        expected<image, error> run() noexcept(NothrowInput<Input>) {
            if (_o.want && !valid(*_o.want)) {
                return unexpected(error(errc::invalid_argument, 0, "png: decode_options.want outside the list"));
            }
            uint8_t sig[8];
            if (!_read(sig, 8, false)) {
                return unexpected(*_err);
            }
            if (std::memcmp(sig, "\x89PNG\r\n\x1a\n", 8) != 0) {
                return unexpected(error(errc::corrupt, 0, "png: not a PNG signature"));
            }
            for (;;) {
                const uint64_t at = _in.offset();
                uint8_t head[8];
                if (!_read(head, 8, false)) {
                    return unexpected(*_err);
                }
                const uint32_t length = be32(head);
                const uint32_t type = be32(head + 4);
                if (length > 0x7FFFFFFFu) {
                    return unexpected(error(errc::corrupt, at, "png: chunk length past 2^31 - 1"));
                }
                for (int i = 4; i < 8; ++i) {
                    const uint8_t c = head[i] | 0x20;
                    if (c < 'a' || c > 'z') {
                        return unexpected(error(errc::corrupt, at, "png: chunk type not of letters"));
                    }
                }
                _crc = hash::crc32();
                _crc.update(slice<const byte>(reinterpret_cast<const byte*>(head + 4), 4));
                if (!_have_header && type != IHDR) {
                    return unexpected(error(errc::corrupt, at, "png: the first chunk is not IHDR"));
                }
                if (_idat_seen && type != IDAT) {
                    _idat_over = true;
                }
                if (!_chunk(type, length, at)) {
                    return unexpected(*_err);
                }
                uint8_t crc[4];
                if (!_read(crc, 4, false)) {
                    return unexpected(*_err);
                }
                if (be32(crc) != _crc.value()) {
                    return unexpected(error(errc::checksum, at, string("png: CRC-32 of chunk " + _name(type))));
                }
                if (type == IEND) {
                    break;
                }
            }
            if (!_pixels_done) {
                return unexpected(error(errc::corrupt, _in.offset(), "png: not enough image data"));
            }
            auto& s = ImageAccess::state(*_image);
            if (!_exif.empty()) {
                ImageAccess::set_orientation(*_image, exif_orientation(reinterpret_cast<const uint8_t*>(_exif.data()), _exif.size()));
                s.exif = std::move(_exif);
            }
            if (!_icc.empty()) {
                s.icc = std::move(_icc);
            }
            return std::move(*_image);
        }

    private:
        static constexpr uint32_t IHDR = png_tag("IHDR");
        static constexpr uint32_t PLTE = png_tag("PLTE");
        static constexpr uint32_t IDAT = png_tag("IDAT");
        static constexpr uint32_t IEND = png_tag("IEND");
        static constexpr uint32_t tRNS = png_tag("tRNS");
        static constexpr uint32_t iCCP = png_tag("iCCP");
        static constexpr uint32_t eXIf = png_tag("eXIf");

        static constexpr size_t Window = compress::detail::WindowSize;   // the history of DEFLATE

        SGCL_INLINE_HOT static uint32_t be32(const uint8_t* p) noexcept {
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }

        SGCL_INLINE_HOT static uint16_t be16(const uint8_t* p) noexcept {
            return static_cast<uint16_t>(p[0] << 8 | p[1]);
        }

        SGCL_INLINE_HOT static void put16(uint8_t* p, uint16_t v) noexcept {
            std::memcpy(p, &v, 2);
        }

        static std::string _name(uint32_t type) noexcept {
            std::string s(4, ' ');
            for (int i = 0; i < 4; ++i) {
                s[i] = char(type >> (24 - 8 * i));
            }
            return s;
        }

        bool _fail(errc code, uint64_t at, const std::string& what) noexcept {
            _err = error(code, at, string(what));
            return false;
        }

        bool _fail_input() noexcept {
            _err = *_in.failure;
            return false;
        }

        // n bytes into dst, through the chunk's CRC when `crc`
        bool _read(uint8_t* dst, size_t n, bool crc) noexcept(NothrowInput<Input>) {
            while (n) {
                const uint8_t* p;
                size_t got;
                if (!_in.peek(n, p, got)) {
                    return _fail_input();
                }
                if (!got) {
                    return _fail(errc::unexpected_end, _in.offset(), "png: the data ends in the middle");
                }
                sgcl::detail::copy_bytes(dst, p, got);
                if (crc) {
                    _crc.update(slice<const byte>(reinterpret_cast<const byte*>(p), got));
                }
                _in.consume(got);
                dst += got;
                n -= got;
            }
            return true;
        }

        // n bytes of the chunk passed over, through its CRC
        bool _skip(size_t n) noexcept(NothrowInput<Input>) {
            while (n) {
                const uint8_t* p;
                size_t got;
                if (!_in.peek(n, p, got)) {
                    return _fail_input();
                }
                if (!got) {
                    return _fail(errc::unexpected_end, _in.offset(), "png: the data ends in the middle");
                }
                _crc.update(slice<const byte>(reinterpret_cast<const byte*>(p), got));
                _in.consume(got);
                n -= got;
            }
            return true;
        }

        bool _chunk(uint32_t type, uint32_t length, uint64_t at) noexcept(NothrowInput<Input>) {
            switch (type) {
                case IHDR:
                    return _ihdr(length, at);
                case PLTE:
                    return _plte(length, at);
                case IDAT:
                    return _idat(length, at);
                case IEND:
                    if (!_idat_seen) {
                        return _fail(errc::corrupt, at, "png: no IDAT chunk");
                    }
                    return _skip(length);
                case tRNS:
                    return _trns(length);
                case iCCP:
                    return _iccp(length, at);
                case eXIf:
                    return _exif_chunk(length, at);
                default:
                    if (!(type & 0x20000000u)) {
                        return _fail(errc::unsupported, at, "png: unknown critical chunk " + _name(type));
                    }
                    return _skip(length);
            }
        }

        bool _ihdr(uint32_t length, uint64_t at) noexcept(NothrowInput<Input>) {
            if (_have_header) {
                return _fail(errc::corrupt, at, "png: a second IHDR");
            }
            if (length != 13) {
                return _fail(errc::corrupt, at, "png: IHDR of " + std::to_string(length) + " bytes");
            }
            uint8_t b[13];
            if (!_read(b, 13, true)) {
                return false;
            }
            _width = be32(b);
            _height = be32(b + 4);
            _depth = b[8];
            _color = b[9];
            if (_width == 0 || _height == 0 || _width > 0x7FFFFFFFu || _height > 0x7FFFFFFFu) {
                return _fail(errc::corrupt, at, "png: IHDR size " + std::to_string(_width) + "x" + std::to_string(_height));
            }
            bool fits;
            switch (_color) {
                case 0: fits = _depth == 1 || _depth == 2 || _depth == 4 || _depth == 8 || _depth == 16; break;
                case 3: fits = _depth == 1 || _depth == 2 || _depth == 4 || _depth == 8; break;
                case 2: case 4: case 6: fits = _depth == 8 || _depth == 16; break;
                default:
                    return _fail(errc::corrupt, at, "png: color type " + std::to_string(_color));
            }
            if (!fits) {
                return _fail(errc::corrupt, at, "png: bit depth " + std::to_string(_depth) + " of color type " + std::to_string(_color));
            }
            if (b[10] != 0) {
                return _fail(errc::corrupt, at, "png: compression method " + std::to_string(b[10]));
            }
            if (b[11] != 0) {
                return _fail(errc::corrupt, at, "png: filter method " + std::to_string(b[11]));
            }
            if (b[12] > 1) {
                return _fail(errc::corrupt, at, "png: interlace method " + std::to_string(b[12]));
            }
            _interlaced = b[12] == 1;
            if (auto e = check_size(_width, _height, _o.limits, at)) {
                _err = *e;
                return false;
            }
            _have_header = true;
            return true;
        }

        bool _plte(uint32_t length, uint64_t at) noexcept(NothrowInput<Input>) {
            if (_have_plte) {
                return _fail(errc::corrupt, at, "png: a second PLTE");
            }
            if (_idat_seen) {
                return _fail(errc::corrupt, at, "png: PLTE after IDAT");
            }
            if (_color == 0 || _color == 4) {
                return _fail(errc::corrupt, at, "png: PLTE in a gray image");
            }
            if (length == 0 || length > 768 || length % 3 != 0) {
                return _fail(errc::corrupt, at, "png: PLTE of " + std::to_string(length) + " bytes");
            }
            uint8_t b[768];
            if (!_read(b, length, true)) {
                return false;
            }
            _have_plte = true;
            if (_color == 3) {
                _palette_size = length / 3;
                for (unsigned i = 0; i < _palette_size; ++i) {
                    _palette[i][0] = b[3 * i];
                    _palette[i][1] = b[3 * i + 1];
                    _palette[i][2] = b[3 * i + 2];
                }
            }
            // truecolor: a suggested palette, not the image's
            return true;
        }

        // Transparency: ignored (as libpng does) when out of place or of
        // the wrong size, or in an image with an alpha channel
        bool _trns(uint32_t length) noexcept(NothrowInput<Input>) {
            uint8_t b[256];
            if (_idat_seen || _have_trns || length > 256) {
                return _skip(length);
            }
            if (!_read(b, length, true)) {
                return false;
            }
            switch (_color) {
                case 0:
                    if (length == 2) {
                        _key[0] = be16(b);
                        _have_trns = true;
                    }
                    break;
                case 2:
                    if (length == 6) {
                        _key[0] = be16(b);
                        _key[1] = be16(b + 2);
                        _key[2] = be16(b + 4);
                        _have_trns = true;
                    }
                    break;
                case 3:
                    if (_have_plte && length <= _palette_size) {
                        for (unsigned i = 0; i < length; ++i) {
                            _palette[i][3] = b[i];
                        }
                        _have_trns = true;
                    }
                    break;
                default:
                    break;
            }
            return true;
        }

        SGCL_INLINE_HOT bool _exif_chunk(uint32_t length, uint64_t at) noexcept(NothrowInput<Input>) {
            if (!_o.metadata || !_exif.empty()) {
                return _skip(length);
            }
            if (length > _o.limits.max_metadata) {
                return _fail(errc::too_large, at, "png: eXIf past limits.max_metadata");
            }
            _exif.resize(length);
            return _read(reinterpret_cast<uint8_t*>(_exif.data()), length, true);
        }

        // An ICC profile: a name of 1 to 79 bytes and its 0, the
        // compression method 0, the profile in a zlib stream. One that
        // does not read (a bad name, a stream that fails) is dropped, as
        // libpng drops it; one past limits.max_metadata is too_large.
        bool _iccp(uint32_t length, uint64_t at) noexcept(NothrowInput<Input>) {
            if (!_o.metadata || _have_icc || _idat_seen) {
                return _skip(length);
            }
            // the chunk holds the name (79 bytes at most), its 0, the
            // method and the profile in a zlib stream: a profile of
            // max_metadata bytes that does not compress takes more than
            // that, zlib's 6 bytes and 5 for each stored block of 65 535
            const size_t most = _o.limits.max_metadata;
            const size_t room = most > SIZE_MAX / 2 ? SIZE_MAX : most + 81 + 6 + 5 * (most / 65535 + 1);
            if (length > room) {
                return _fail(errc::too_large, at, "png: iCCP past limits.max_metadata");
            }
            std::vector<uint8_t> body(length);
            if (!_read(body.data(), length, true)) {
                return false;
            }
            _have_icc = true;
            size_t name = 0;
            while (name < body.size() && name < 80 && body[name] != 0) {
                ++name;
            }
            if (name == 0 || name >= 80 || name + 2 > body.size() || body[name + 1] != 0) {
                return true;
            }
            auto profile = compress::zlib::decompress(
                slice<const byte>(reinterpret_cast<const byte*>(body.data()) + name + 2, body.size() - name - 2),
                compress::limits{.max_size = _o.limits.max_metadata});
            if (!profile) {
                if (profile.error().code() == compress::errc::too_large) {
                    return _fail(errc::too_large, at, "png: iCCP profile past limits.max_metadata");
                }
                return true;
            }
            _icc = std::move(*profile);
            return true;
        }

        bool _idat(uint32_t length, uint64_t at) noexcept(NothrowInput<Input>) {
            if (_idat_over) {
                return _fail(errc::corrupt, at, "png: IDAT chunks not one after another");
            }
            if (!_idat_seen) {
                if (!_start(at)) {
                    return false;
                }
                _idat_seen = true;
            }
            size_t left = length;
            while (left) {
                const uint8_t* p;
                size_t got;
                if (!_in.peek(left, p, got)) {
                    return _fail_input();
                }
                if (!got) {
                    return _fail(errc::unexpected_end, _in.offset(), "png: the data ends in the middle");
                }
                _crc.update(slice<const byte>(reinterpret_cast<const byte*>(p), got));
                if (!_feed(p, got)) {
                    return false;
                }
                _in.consume(got);
                left -= got;
            }
            return true;
        }

        // The image's format, the pipeline of the rows, the memory: at
        // the first IDAT, when PLTE and tRNS, which come before it, are known
        bool _start(uint64_t at) noexcept {
            if (_color == 3 && !_have_plte) {
                return _fail(errc::corrupt, at, "png: indexed color without PLTE");
            }
            const bool wide16 = _depth == 16;
            switch (_color) {
                case 0:
                    _channels = 1;
                    _native = _have_trns ? (wide16 ? pixel_format::gray_alpha16 : pixel_format::gray_alpha8) : (wide16 ? pixel_format::gray16 : pixel_format::gray8);
                    break;
                case 2:
                    _channels = 3;
                    _native = _have_trns ? (wide16 ? pixel_format::rgba16 : pixel_format::rgba8) : (wide16 ? pixel_format::rgb16 : pixel_format::rgb8);
                    break;
                case 3:
                    _channels = 1;
                    _native = pixel_format::rgba8;
                    break;
                case 4:
                    _channels = 2;
                    _native = wide16 ? pixel_format::gray_alpha16 : pixel_format::gray_alpha8;
                    break;
                default:
                    _channels = 4;
                    _native = wide16 ? pixel_format::rgba16 : pixel_format::rgba8;
                    break;
            }
            _out = _o.want.value_or(_native);
            _image.emplace(_width, _height, _out);
            _pixels = reinterpret_cast<uint8_t*>(ImageAccess::state(*_image).pixels.data());
            _stride = ImageAccess::state(*_image).stride;
            const unsigned bits = _channels * _depth;
            _fbpp = bits < 8 ? 1 : bits / 8;
            _bits = bits;
            const size_t rowbytes = (size_t(_width) * bits + 7) / 8;
            // The row as the file has it is the image's: unfiltered straight
            // into the image, the row above read from it
            _direct = !_interlaced && _depth == 8 && _out == _native && _color != 3 && !(_have_trns && (_color == 0 || _color == 2));
            _convert = _out == _native ? nullptr : converter(_native, _out);
            // One block: the window (history, a row and 64 KB more, so that
            // each call of inflate has room to fill), the row being made,
            // the row above, a row of zeros for the first of a pass, a row
            // in the native format, a row in the image's (interlaced)
            _window_size = Window + (rowbytes + 1) + 65536;
            const size_t row = rowbytes + 16;
            const size_t native_row = size_t(_width) * bytes_per_pixel(_native) + 16;
            const size_t out_row = size_t(_width) * bytes_per_pixel(_out) + 16;
            auto align = [](size_t n) { return (n + 15) & ~size_t(15); };
            size_t offset = align(sizeof(compress::detail::InflateState));
            const size_t window_at = offset;
            offset += align(_window_size);
            const size_t cur_at = offset;
            offset += align(row);
            const size_t prior_at = offset;
            offset += align(row);
            const size_t zero_at = offset;
            offset += align(row);
            const size_t native_at = offset;
            offset += align(native_row);
            const size_t out_at = offset;
            offset += align(out_row);
            _memory.reset(new uint8_t[offset]);   // aligned for any type: InflateState first
            _inflate = new (_memory.get()) compress::detail::InflateState;
            _inflate->reset();
            _window = _memory.get() + window_at;
            _cur = _memory.get() + cur_at;
            _prior = _memory.get() + prior_at;
            _zero = _memory.get() + zero_at;
            // libc's memset, not fill_bytes: a row may be megabytes, and a
            // large zero fill is libc's (whole cache lines, DESIGN 393)
            std::memset(_zero, 0, row);
            _native_row = _memory.get() + native_at;
            _out_row = _memory.get() + out_at;
            _pass = _interlaced ? 0 : 7;
            _pass_setup();
            return true;
        }

        // The pass of Adam7 (0 to 6), or 7 for an image not interlaced;
        // passes of no pixels skipped
        void _pass_setup() noexcept {
            static constexpr uint8_t X0[7] = {0, 4, 0, 2, 0, 1, 0};
            static constexpr uint8_t Y0[7] = {0, 0, 4, 0, 2, 0, 1};
            static constexpr uint8_t DX[7] = {8, 8, 4, 4, 2, 2, 1};
            static constexpr uint8_t DY[7] = {8, 8, 8, 4, 4, 2, 2};
            if (!_interlaced) {
                _x0 = _y0 = 0;
                _dx = _dy = 1;
                _pass_w = _width;
                _pass_h = _height;
            } else {
                for (; _pass < 7; ++_pass) {
                    _x0 = X0[_pass];
                    _y0 = Y0[_pass];
                    _dx = DX[_pass];
                    _dy = DY[_pass];
                    _pass_w = _width > _x0 ? (_width - _x0 + _dx - 1) / _dx : 0;
                    _pass_h = _height > _y0 ? (_height - _y0 + _dy - 1) / _dy : 0;
                    if (_pass_w && _pass_h) {
                        break;
                    }
                }
                if (_pass == 7) {
                    _pixels_done = true;
                    return;
                }
            }
            _pass_rowbytes = (size_t(_pass_w) * _bits + 7) / 8;
            _row = 0;
        }

        // IDAT data: the zlib header, the DEFLATE stream into the window,
        // the Adler-32 after it; bytes past the stream ignored
        bool _feed(const uint8_t* p, size_t n) noexcept {
            while (n) {
                switch (_zstage) {
                    case ZHeader:
                        _zhead[_zgot++] = *p++;
                        --n;
                        if (_zgot == 2) {
                            const unsigned cmf = _zhead[0], flg = _zhead[1];
                            if ((cmf & 15) != 8 || (cmf >> 4) > 7 || (cmf * 256 + flg) % 31 != 0) {
                                return _fail(errc::corrupt, _in.offset(), "png: not a zlib stream");
                            }
                            if (flg & 0x20) {
                                return _fail(errc::corrupt, _in.offset(), "png: a zlib stream with a preset dictionary");
                            }
                            _zstage = ZDeflate;
                            _zgot = 0;
                        }
                        break;
                    case ZDeflate: {
                        const uint8_t* in = p;
                        const size_t before = _window_pos;
                        auto status = compress::detail::inflate(*_inflate, in, p + n, _window, _window_pos, _window_size);
                        if (_window_pos > before) {
                            _adler.update(slice<const byte>(reinterpret_cast<const byte*>(_window + before), _window_pos - before));
                        }
                        n -= size_t(in - p);
                        p = in;
                        if (status == compress::detail::InflateStatus::failed) {
                            const char* text = _inflate->error_text;
                            return _fail(errc::corrupt, _in.offset(), std::string("png: ") + (text ? text : "corrupt zlib data"));
                        }
                        if (!_rows()) {
                            return false;
                        }
                        if (status == compress::detail::InflateStatus::need_room) {
                            _slide();
                        } else if (status == compress::detail::InflateStatus::done) {
                            _zstage = ZTrailer;
                        }
                        break;
                    }
                    case ZTrailer:
                        _zhead[_zgot++] = *p++;
                        --n;
                        if (_zgot == 4) {
                            if (be32(_zhead) != _adler.value()) {
                                return _fail(errc::checksum, _in.offset(), "png: Adler-32 of the image data");
                            }
                            _zstage = ZDone;
                        }
                        break;
                    case ZDone:
                        n = 0;
                        break;
                }
            }
            return true;
        }

        // The history the next back reference may reach and the row not yet
        // complete moved to the front of the window
        SGCL_INLINE_HOT void _slide() noexcept {
            size_t from = _window_pos > Window ? _window_pos - Window : 0;
            from = std::min(from, _window_read);
            // the bytes kept overlap where they go when fewer are dropped
            // than kept
            sgcl::detail::move_bytes(_window, _window + from, _window_pos - from);
            _window_pos -= from;
            _window_read -= from;
        }

        // Every row complete in the window
        bool _rows() noexcept {
            while (!_pixels_done) {
                const size_t need = 1 + _pass_rowbytes;
                if (_window_pos - _window_read < need) {
                    break;
                }
                if (!_one_row(_window + _window_read)) {
                    return false;
                }
                _window_read += need;
                if (++_row == _pass_h) {
                    if (_interlaced) {
                        ++_pass;
                        _pass_setup();
                    } else {
                        _pixels_done = true;
                    }
                }
            }
            if (_pixels_done) {
                _window_read = _window_pos;   // what follows the image is not read
            }
            return true;
        }

        bool _one_row(const uint8_t* raw) noexcept {

            const uint8_t filter = raw[0];
            if (filter > FilterPaeth) {
                return _fail(errc::corrupt, _in.offset(), "png: filter type " + std::to_string(filter));
            }
            if (_direct) {
                uint8_t* out = _pixels + size_t(_row) * _stride;
                const uint8_t* prior = _row ? out - _stride : _zero;
                unfilter(filter, raw + 1, prior, out, _pass_rowbytes, _fbpp);
                return true;
            }
            unfilter(filter, raw + 1, _row ? _prior : _zero, _cur, _pass_rowbytes, _fbpp);
            const uint32_t y = _y0 + _row * _dy;
            uint8_t* image_row = _pixels + size_t(y) * _stride;
            if (!_interlaced) {
                if (!_convert) {
                    _expand(_cur, image_row, _pass_w);
                } else {
                    _expand(_cur, _native_row, _pass_w);
                    _convert(reinterpret_cast<const std::byte*>(_native_row), reinterpret_cast<std::byte*>(image_row), _pass_w);
                }
            } else {
                const uint8_t* src = _native_row;
                _expand(_cur, _native_row, _pass_w);
                if (_convert) {
                    _convert(reinterpret_cast<const std::byte*>(_native_row), reinterpret_cast<std::byte*>(_out_row), _pass_w);
                    src = _out_row;
                }
                switch (bytes_per_pixel(_out)) {
                    case 1: _scatter<1>(src, image_row); break;
                    case 2: _scatter<2>(src, image_row); break;
                    case 3: _scatter<3>(src, image_row); break;
                    case 4: _scatter<4>(src, image_row); break;
                    case 6: _scatter<6>(src, image_row); break;
                    default: _scatter<8>(src, image_row); break;
                }
            }
            std::swap(_cur, _prior);
            return true;
        }

        // An interlaced pass's row into the image's: pixel i to column
        // x0 + i·dx, B bytes each (a constant, the copies plain moves)
        template<size_t B>
        void _scatter(const uint8_t* src, uint8_t* image_row) const noexcept {
            for (uint32_t i = 0; i < _pass_w; ++i) {
                std::memcpy(image_row + (size_t(_x0) + size_t(i) * _dx) * B, src + i * B, B);
            }
        }

        // A sample of `depth` bits (1, 2, 4, 8), the x-th of the row
        SGCL_INLINE_HOT static unsigned _sample(const uint8_t* s, uint32_t x, unsigned depth) noexcept {
            if (depth == 8) {
                return s[x];
            }
            const size_t bit = size_t(x) * depth;
            return (s[bit >> 3] >> (8 - depth - (bit & 7))) & ((1u << depth) - 1);
        }

        // A row as the file has it (unfiltered) into the native format
        void _expand(const uint8_t* s, uint8_t* d, uint32_t count) const noexcept {
            switch (_color) {
                case 0:
                    if (_depth == 16) {
                        for (uint32_t x = 0; x < count; ++x) {
                            const uint16_t v = be16(s + 2 * x);
                            put16(d, v);
                            d += 2;
                            if (_have_trns) {
                                put16(d, v == _key[0] ? 0 : 0xFFFF);
                                d += 2;
                            }
                        }
                    } else {
                        const unsigned scale = 255 / ((1u << _depth) - 1);
                        for (uint32_t x = 0; x < count; ++x) {
                            const unsigned v = _sample(s, x, _depth);
                            *d++ = static_cast<uint8_t>(v * scale);
                            if (_have_trns) {
                                *d++ = v == _key[0] ? 0 : 255;
                            }
                        }
                    }
                    break;
                case 2:
                    if (_depth == 16) {
                        for (uint32_t x = 0; x < count; ++x) {
                            const uint16_t r = be16(s + 6 * x), g = be16(s + 6 * x + 2), b = be16(s + 6 * x + 4);
                            put16(d, r);
                            put16(d + 2, g);
                            put16(d + 4, b);
                            d += 6;
                            if (_have_trns) {
                                put16(d, r == _key[0] && g == _key[1] && b == _key[2] ? 0 : 0xFFFF);
                                d += 2;
                            }
                        }
                    } else {
                        for (uint32_t x = 0; x < count; ++x) {
                            const uint8_t r = s[3 * x], g = s[3 * x + 1], b = s[3 * x + 2];
                            d[0] = r;
                            d[1] = g;
                            d[2] = b;
                            d += 3;
                            if (_have_trns) {
                                *d++ = r == _key[0] && g == _key[1] && b == _key[2] ? 0 : 255;
                            }
                        }
                    }
                    break;
                case 3:
                    for (uint32_t x = 0; x < count; ++x) {
                        std::memcpy(d, _palette[_sample(s, x, _depth)], 4);
                        d += 4;
                    }
                    break;
                default:
                    // gray with alpha, truecolor with alpha: the samples as
                    // they are, 16-bit ones turned to the machine's order
                    if (_depth == 16) {
                        const size_t values = size_t(count) * _channels;
                        for (size_t i = 0; i < values; ++i) {
                            put16(d + 2 * i, be16(s + 2 * i));
                        }
                    } else {
                        sgcl::detail::copy_bytes(d, s, size_t(count) * _channels);
                    }
                    break;
            }
        }

        enum ZStage : uint8_t {
            ZHeader,
            ZDeflate,
            ZTrailer,
            ZDone
        };

        Input& _in;
        const decode_options& _o;
        optional<error> _err;
        hash::crc32 _crc;
        hash::adler32 _adler;

        // IHDR
        uint32_t _width = 0, _height = 0;
        uint8_t _depth = 0, _color = 0;
        bool _interlaced = false;
        bool _have_header = false;

        // PLTE, tRNS: 256 entries, opaque black past the palette's end
        uint8_t _palette[256][4] = {};
        unsigned _palette_size = 0;
        bool _have_plte = false;
        bool _have_trns = false;
        uint16_t _key[3] = {};

        // metadata
        vector<byte> _exif;
        vector<byte> _icc;
        bool _have_icc = false;

        // the image and the pipeline of its rows
        optional<image> _image;
        uint8_t* _pixels = nullptr;
        size_t _stride = 0;
        pixel_format _native = pixel_format::rgba8;
        pixel_format _out = pixel_format::rgba8;
        ConvertRow _convert = nullptr;
        unsigned _channels = 0, _bits = 0, _fbpp = 1;
        bool _direct = false;
        bool _idat_seen = false;
        bool _idat_over = false;
        bool _pixels_done = false;

        // the pass and the row in it
        unsigned _pass = 0;
        uint32_t _x0 = 0, _y0 = 0, _dx = 1, _dy = 1, _pass_w = 0, _pass_h = 0, _row = 0;
        size_t _pass_rowbytes = 0;

        // the zlib stream
        ZStage _zstage = ZHeader;
        uint8_t _zhead[4] = {};
        unsigned _zgot = 0;

        // the memory of the decoding, one block
        std::unique_ptr<uint8_t[]> _memory;
        compress::detail::InflateState* _inflate = nullptr;
        uint8_t* _window = nullptr;
        size_t _window_size = 0, _window_pos = 0, _window_read = 0;
        uint8_t* _cur = nullptr;
        uint8_t* _prior = nullptr;
        uint8_t* _zero = nullptr;
        uint8_t* _native_row = nullptr;
        uint8_t* _out_row = nullptr;
    };
}
