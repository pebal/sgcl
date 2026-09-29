//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "exif.h"
#include "input.h"
#include "pixels.h"
#include "vp8_decoder.h"
#include "vp8_yuv.h"
#include "vp8l_decoder.h"
#include "vp8l_simd.h"
#include "../error.h"
#include "../frames.h"
#include "../image.h"
#include "../options.h"
#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../core/vector.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace sgcl::codec::detail {
    // A frame of a WebP file as its headers give it: where it lies on the
    // canvas, how long it shows, how it is blended and disposed of, and its
    // bitstream (the input standing after the 5 bytes of the VP8L header)
    struct WebpFrame {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t duration = 0;   // milliseconds
        bool blend = true;       // alpha-blended onto the canvas; false: written over it
        bool dispose = false;    // cleared to transparent before the next frame
        uint64_t at = 0;         // the offset of the bitstream chunk
        uint64_t size = 0;       // VP8L: the bitstream's bytes after its header; VP8: the chunk's
        bool lossy = false;      // VP8, else VP8L
        bool alpha = false;      // VP8 with an ALPH chunk before it
    };

    // Why a lossy image was refused where libwebp may decode it: its data
    // is invalid by RFC 6386 or RFC 9649, and libwebp shows what its
    // decoder reads past the end (browsers with it). The module refuses
    // such data; the tests and the fuzzer tell these refusals from others
    enum class WebpDamage : uint8_t {
        none,
        partition_overrun,   // a VP8 partition read past its end
        partition_start,     // a VP8 partition whose first byte no encoder writes (0xFF)
        alpha_overrun,       // an ALPH chunk's stream read past its end
    };

    // The WebP container (RFC 9649 §2) over an input of MemoryInput's shape:
    // the RIFF header, then the simple format (one VP8L or VP8 chunk) or the
    // extended one (VP8X, then ICCP, ANIM, the image or its ANMF frames,
    // EXIF, XMP and unknown chunks), walked chunk by chunk. What it takes
    // and refuses is libwebp's demuxer's, which WebPAnimDecoder reads with,
    // found against it chunk by chunk (tests/codec/webp.cpp) and by the
    // differential fuzzer, more than RFC 9649 says:
    //   - the data to the end of the RIFF size, bytes after it passed over;
    //     every chunk inside it, its padding byte counted;
    //   - the simple format: the image first; after it every chunk inside
    //     the RIFF size up to the first that is not ALPH (one ALPH passed
    //     over, VP8L after it refused), the rest never read;
    //   - VP8X first and of 10 bytes, never twice; an image outside ANMF in
    //     an animation refused, a second still image refused, a still image
    //     of the canvas's size; ANIM before ANMF, of 6 bytes (its padding
    //     byte one of them); unknown chunks anywhere at the top level;
    //   - a frame is what follows its ANMF header as long as it is the
    //     frame's (_anmf says how); every frame inside the canvas;
    //   - ALPH with VP8L refused, ALPH right before its VP8; a VP8L
    //     bitstream may read its chunk's padding byte, a simple code's
    //     symbol past its alphabet is no symbol (Vp8lDecoder).
    // Data RFC 6386 or RFC 9649 make invalid, which libwebp decodes anyway
    // (a partition or an alpha stream read past its end), is refused:
    // WebpDamage says so.
    template<class Input>
    class WebpReader {
    public:
        enum class Step : uint8_t { frame, end, failed };

        WebpReader(Input& in, const decode_options& o) noexcept
        : _in(in), _o(o) {
        }

        // The RIFF header and the chunks before the first image
        bool start() {
            uint8_t h[12];
            if (!_read(h, 12)) {
                return false;
            }
            if (std::memcmp(h, "RIFF", 4) != 0 || std::memcmp(h + 8, "WEBP", 4) != 0) {
                return _fail(errc::corrupt, 0, "webp: not a RIFF WEBP header");
            }
            const uint32_t riff = le32(h + 4);
            if (riff < 12 || riff > 0xFFFFFFF6u) {
                return _fail(errc::corrupt, 4, "webp: a RIFF size out of range");
            }
            _riff_end = 8 + uint64_t(riff);
            if (!_have(_riff_end)) {
                return _fail(errc::unexpected_end, _data_end(), "webp: the data ends before the RIFF size");
            }
            uint8_t c[8];
            if (!_chunk_header(c)) {
                return false;
            }
            const uint64_t at = _in.offset() - 8;
            const uint32_t size = le32(c + 4);
            if (tag(c, "VP8X")) {
                return _vp8x(at, size);
            }
            // the simple format: the image first, its header read now for
            // the size of the canvas
            // (an image first: ALPH there, alpha of the simple format, is
            // refused as libwebp refuses it)
            _simple = true;
            const uint64_t image_at = at;
            const uint32_t image_size = size;
            if (!tag(c, "VP8L") && !tag(c, "VP8 ")) {
                return _fail(errc::corrupt, at, "webp: a chunk " + _name(c) + " where the image belongs");
            }
            if (uint64_t(image_size) + (image_size & 1) > _riff_end - (image_at + 8)) {
                return _fail(errc::corrupt, image_at, "webp: a chunk past the RIFF size");
            }
            if (!_bitstream(c, image_at, image_size, _ready)) {
                return false;
            }
            _canvas_width = _ready.width;
            _canvas_height = _ready.height;
            _alpha = _ready.lossy ? _ready.alpha : _header.alpha;
            if (auto e = check_size(_ready.width, _ready.height, _o.limits, image_at + 9)) {
                _err = *e;
                return false;
            }
            _frame_end = image_at + 8 + uint64_t(image_size) + (image_size & 1);
            _has_ready = true;
            return true;
        }

        // The next frame: its headers read, the input at its bitstream
        Step next(WebpFrame& f) {
            if (_err) {
                return Step::failed;
            }
            if (_has_ready) {
                _has_ready = false;
                f = _ready;
                ++_images;
                return Step::frame;
            }
            for (;;) {
                uint8_t c[8];
                uint64_t at;
                if (_has_pending) {
                    // a chunk a frame stopped at: read here, at the top level
                    std::memcpy(c, _pending, 8);
                    at = _pending_at;
                    _has_pending = false;
                } else {
                    if (_in.offset() >= _riff_end) {
                        return _end() ? Step::end : Step::failed;
                    }
                    if (!_chunk_header(c)) {
                        return Step::failed;
                    }
                    at = _in.offset() - 8;
                }
                const uint32_t size = le32(c + 4);
                const uint64_t padded = uint64_t(size) + (size & 1);
                if (padded > _riff_end - (at + 8)) {
                    _fail(errc::corrupt, at, "webp: a chunk past the RIFF size");
                    return Step::failed;
                }
                if (_simple) {
                    // after the simple format's image, as libwebp's demuxer
                    // reads it: one ALPH chunk passed over, VP8L after it
                    // refused, and at any other chunk (a second image too)
                    // the file ends, the rest never read
                    if (tag(c, "ALPH") && !_simple_alph) {
                        _simple_alph = true;
                        if (!_skip_padded(size)) {
                            return Step::failed;
                        }
                        continue;
                    }
                    if (tag(c, "VP8L") && _simple_alph) {
                        _fail(errc::corrupt, at, "webp: VP8L after an ALPH chunk");
                        return Step::failed;
                    }
                    return _end() ? Step::end : Step::failed;
                }
                if (_alph_pending && !tag(c, "VP8L") && !tag(c, "VP8 ")) {
                    // an ALPH chunk's image comes right after it
                    _fail(errc::corrupt, at, "webp: an ALPH chunk not followed by its image");
                    return Step::failed;
                }
                if (tag(c, "VP8L") || tag(c, "VP8 ") || tag(c, "ALPH")) {
                    if (_animated || _anim_seen) {
                        _fail(errc::corrupt, at, "webp: an image outside ANMF in an animation");
                        return Step::failed;
                    }
                    if (_images) {
                        _fail(errc::corrupt, at, "webp: a second image in a still image");
                        return Step::failed;
                    }
                    if (tag(c, "ALPH")) {
                        if (_alph_pending) {
                            _fail(errc::corrupt, at, "webp: a second ALPH chunk");
                            return Step::failed;
                        }
                        if (!_read_alph(size)) {
                            return Step::failed;
                        }
                        continue;
                    }
                    if (!_bitstream(c, at, size, f)) {
                        return Step::failed;
                    }
                    if (f.width != _canvas_width || f.height != _canvas_height) {
                        _fail(errc::corrupt, at, "webp: a still image of another size than the canvas");
                        return Step::failed;
                    }
                    ++_images;
                    _frame_end = at + 8 + padded;
                    return Step::frame;
                }
                if (tag(c, "VP8X")) {
                    _fail(errc::corrupt, at, "webp: a second VP8X chunk");
                    return Step::failed;
                }
                if (tag(c, "ANIM")) {
                    // 6 bytes, the padding byte of one of 5 among them (as
                    // libwebp reads it)
                    if (padded < 6) {
                        _fail(errc::corrupt, at, "webp: an ANIM chunk shorter than 6 bytes");
                        return Step::failed;
                    }
                    uint8_t a[6];
                    if (!_read(a, 6) || !_skip(padded - 6)) {
                        return Step::failed;
                    }
                    _loops = uint32_t(a[4] | a[5] << 8);
                    _anim_seen = true;
                    continue;
                }
                if (tag(c, "ANMF")) {
                    if (!_anim_seen) {
                        _fail(errc::corrupt, at, "webp: an ANMF chunk before ANIM");
                        return Step::failed;
                    }
                    auto s = _anmf(at, size, f);
                    if (s == Step::end) {
                        continue;
                    }
                    return s;
                }
                if (tag(c, "ICCP") && _o.metadata && _icc.empty()) {
                    if (!_metadata(_icc, at, size)) {
                        return Step::failed;
                    }
                    continue;
                }
                if (tag(c, "EXIF") && _o.metadata && _exif.empty()) {
                    if (!_metadata(_exif, at, size)) {
                        return Step::failed;
                    }
                    continue;
                }
                if (!_skip_padded(size)) {
                    return Step::failed;
                }
            }
        }

        // The frame's pixels decoded (the VP8L bitstream the input stands
        // at), then what is left of the frame passed over
        bool decode(const WebpFrame& f) {
            _lossy = f.lossy;
            _direct_done = false;
            if (f.lossy) {
                return _decode_lossy(f) && _after_frame();
            }
            // the bits may run into the chunk's padding byte: libwebp's
            // reader is given the chunk with it
            const uint64_t readable = f.size + ((f.size + 5) & 1);
            if (!_vp8l.decode(_in, readable, f.width, f.height, _err, f.at)) {
                return false;
            }
            return _after_frame();
        }

        // The VP8 decoder of the last lossy frame: its planes (the tests
        // hold them against the reference decoders' YUV)
        const vp8::Decoder& lossy_decoder() const noexcept {
            return _vp8;
        }

        // Why the last frame was refused, when its data is invalid where
        // libwebp decodes it anyway (WebpDamage); none otherwise
        WebpDamage damage() const noexcept {
            return _damage;
        }

        // The frame's bitstream passed over, not decoded
        bool skip(const WebpFrame& f) {
            (void)f;
            return _after_frame();
        }

        const uint32_t* pixels() const noexcept {
            return _lossy ? _argb.data() : _vp8l.pixels();
        }

        // The next lossy frame's pixels written straight into `dst` (rows
        // `stride` apart) as rgb8 or rgba8, not into the ARGB words; a
        // lossless frame ignores it. direct_done() says whether the last
        // decode() wrote there (pixels() then holds nothing of it)
        void direct(uint8_t* dst, size_t stride, bool rgba) noexcept {
            _direct = dst;
            _direct_stride = stride;
            _direct_rgba = rgba;
        }

        bool direct_done() const noexcept {
            return _direct_done;
        }

        uint32_t canvas_width() const noexcept {
            return _canvas_width;
        }

        uint32_t canvas_height() const noexcept {
            return _canvas_height;
        }

        bool animated() const noexcept {
            return _animated;
        }

        // Whether the file says it has alpha (VP8X's flag, or VP8L's
        // alpha_is_used in the simple format)
        bool alpha() const noexcept {
            return _alpha;
        }

        // The plays of the animation: 0 forever
        uint32_t loops() const noexcept {
            return _loops;
        }

        const optional<error>& failure() const noexcept {
            return _err;
        }

        vector<byte>& icc() noexcept {
            return _icc;
        }

        vector<byte>& exif() noexcept {
            return _exif;
        }

    private:
        static uint32_t le32(const uint8_t* p) noexcept {
            return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
        }

        static uint32_t le24(const uint8_t* p) noexcept {
            return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16;
        }

        static bool tag(const uint8_t* c, const char* t) noexcept {
            return std::memcmp(c, t, 4) == 0;
        }

        static std::string _name(const uint8_t* c) {
            std::string s;
            for (int i = 0; i < 4; ++i) {
                s += c[i] >= 0x20 && c[i] < 0x7f ? char(c[i]) : '?';
            }
            return s;
        }

        bool _fail(errc code, uint64_t at, const std::string& what) {
            _err = error(code, at, string(what));
            return false;
        }

        bool _fail_input() {
            _err = *_in.failure;
            return false;
        }

        // Memory knows where its data ends; a stream finds out as it reads
        bool _have(uint64_t end) {
            if constexpr (requires { _in.end; }) {
                return uint64_t(_in.end - _in.begin) >= end;
            } else {
                (void)end;
                return true;
            }
        }

        uint64_t _data_end() const noexcept {
            if constexpr (requires { _in.end; }) {
                return uint64_t(_in.end - _in.begin);
            } else {
                return _in.offset();
            }
        }

        bool _read(uint8_t* dst, size_t n) {
            while (n) {
                const uint8_t* p;
                size_t got;
                if (!_in.peek(n, p, got)) {
                    return _fail_input();
                }
                if (!got) {
                    return _fail(errc::unexpected_end, _in.offset(), "webp: the data ends in the middle");
                }
                std::memcpy(dst, p, got);
                _in.consume(got);
                dst += got;
                n -= got;
            }
            return true;
        }

        // n bytes into buf from offset `at` on, buf grown to at + n: from
        // memory only when that much is left (checked before anything is
        // allocated); from a stream at once up to 4 MB (one allocation,
        // whatever the size), past that in steps that double as the bytes
        // come, so that a size a header claims past what the stream holds
        // costs 4 MB at most, not the claim
        template<class Buffer>
        bool _read_into(Buffer& buf, size_t at, size_t n) {
            auto bytes = [&buf] { return reinterpret_cast<uint8_t*>(buf.data()); };
            if constexpr (requires { _in.end; _in.at; }) {
                if (size_t(_in.end - _in.at) < n) {
                    return _fail(errc::unexpected_end, _in.offset(), "webp: a chunk past the end of the data");
                }
                buf.resize(at + n);
                return _read(bytes() + at, n);
            } else {
                size_t done = 0, step = size_t(1) << 22;
                while (done < n) {
                    const size_t take = std::min(n - done, step);
                    buf.resize(at + done + take);
                    if (!_read(bytes() + at + done, take)) {
                        return false;
                    }
                    done += take;
                    step = std::min<size_t>(step * 2, size_t(1) << 26);
                }
                buf.resize(at + n);
                return true;
            }
        }

        bool _skip(uint64_t n) {
            while (n) {
                const uint8_t* p;
                size_t got;
                if (!_in.peek(size_t(std::min<uint64_t>(n, uint64_t(1) << 30)), p, got)) {
                    return _fail_input();
                }
                if (!got) {
                    return _fail(errc::unexpected_end, _in.offset(), "webp: the data ends in the middle");
                }
                _in.consume(got);
                n -= got;
            }
            return true;
        }

        bool _skip_to(uint64_t offset) {
            return offset <= _in.offset() || _skip(offset - _in.offset());
        }

        // The rest of a chunk of `size` bytes, `read` of them taken, and
        // its padding byte
        bool _skip_padded(uint32_t size, uint32_t read = 0) {
            return _skip(uint64_t(size) - read + (size & 1));
        }

        // A chunk's header, inside the RIFF size
        bool _chunk_header(uint8_t* c) {
            if (_riff_end - _in.offset() < 8) {
                return _fail(errc::corrupt, _in.offset(), "webp: a chunk header past the RIFF size");
            }
            return _read(c, 8);
        }

        bool _vp8x(uint64_t at, uint32_t size) {
            if (size != 10) {
                return _fail(errc::corrupt, at, "webp: a VP8X chunk not of 10 bytes");
            }
            if (uint64_t(size) + (size & 1) > _riff_end - (at + 8)) {
                return _fail(errc::corrupt, at, "webp: a chunk past the RIFF size");
            }
            uint8_t v[10];
            if (!_read(v, 10) || !_skip_padded(size, 10)) {
                return false;
            }
            _alpha = v[0] & 0x10;
            _animated = v[0] & 0x02;
            _canvas_width = le24(v + 4) + 1;
            _canvas_height = le24(v + 7) + 1;
            if (uint64_t(_canvas_width) * _canvas_height > 0xFFFFFFFFu) {
                return _fail(errc::corrupt, at + 12, "webp: a canvas of 2^32 pixels or more");
            }
            if (auto e = check_size(_canvas_width, _canvas_height, _o.limits, at + 12)) {
                _err = *e;
                return false;
            }
            return true;
        }

        // A bitstream chunk's header (VP8L or VP8)
        bool _bitstream(const uint8_t* c, uint64_t at, uint32_t size, WebpFrame& f) {
            f.x = f.y = 0;
            f.duration = 0;
            f.blend = false;
            f.dispose = false;
            if (tag(c, "VP8 ")) {
                // its frame header, as libwebp's demuxer checks it: a key
                // frame, version 0..3, shown, the start code, the first
                // partition inside the chunk, a size of neither side 0
                if (size < 10) {
                    return _fail(errc::corrupt, at, "webp: a VP8 chunk shorter than its header");
                }
                if (!_read(_vp8_head, 10)) {
                    return false;
                }
                uint32_t w, h;
                if (!vp8::Decoder::size_of(_vp8_head, size, w, h)) {
                    return _fail(errc::corrupt, at + 8, "webp: not a VP8 key frame header (frame tag, start code, size)");
                }
                f.width = w;
                f.height = h;
                f.at = at;
                f.size = size;
                f.lossy = true;
                f.alpha = _alph_pending;
                _bitstream_end = at + 8 + uint64_t(size) + (size & 1);
                return true;
            }
            f.lossy = false;
            f.alpha = false;
            if (_alph_pending) {
                return _fail(errc::corrupt, at, "webp: an ALPH chunk before VP8L, which carries its own alpha");
            }
            if (size < 5) {
                return _fail(errc::corrupt, at, "webp: a VP8L chunk shorter than its header");
            }
            uint8_t h[5];
            if (!_read(h, 5)) {
                return false;
            }
            if (!_header.parse(h)) {
                return _fail(errc::corrupt, at + 8, "webp: not a VP8L header (signature 0x2F, version 0)");
            }
            f.width = _header.width;
            f.height = _header.height;
            f.at = at;
            f.size = size - 5;
            _bitstream_end = at + 8 + uint64_t(size) + (size & 1);
            return true;
        }

        // An ANMF chunk as libwebp's demuxer reads it: the frame's place and
        // timing (16 bytes), then the frame's chunks from there, taken as
        // long as they are the frame's: an ALPH chunk, then the image. The
        // first chunk that is neither ends the frame and is read at the top
        // level, the rest of the ANMF chunk with it (an unknown chunk there
        // is passed over, an image outside ANMF refused). A frame with no
        // image is dropped; one with ALPH and no image refused; the frame's
        // chunks may not reach past its ANMF chunk. Frames of an ANMF chunk
        // in a file not animated are read and dropped. The frame's width
        // and height are its image's; the ANMF chunk's own only bound it as
        // libwebp's demuxer bounds them, their product under 2^32 (its
        // MAX_IMAGE_AREA), a frame claiming more refused (the differential
        // fuzzer's find: tests/codec/fuzz/seeds/webp_decode/regress_anmf_area)
        Step _anmf(uint64_t at, uint32_t size, WebpFrame& f) {
            if (size < 16) {
                _fail(errc::corrupt, at, "webp: an ANMF chunk shorter than 16 bytes");
                return Step::failed;
            }
            uint8_t a[16];
            if (!_read(a, 16)) {
                return Step::failed;
            }
            const uint32_t fx = le24(a) * 2, fy = le24(a + 3) * 2;
            if ((uint64_t(le24(a + 6)) + 1) * (uint64_t(le24(a + 9)) + 1) >= (uint64_t(1) << 32)) {
                _fail(errc::corrupt, at, "webp: an ANMF chunk claiming a frame of 2^32 pixels or more");
                return Step::failed;
            }
            const uint32_t duration = le24(a + 12);
            const bool blend = !(a[15] & 2), dispose = a[15] & 1;
            const uint64_t start = _in.offset();
            const uint64_t payload = uint64_t(size) - 16;
            bool alpha = false;
            for (;;) {
                if (_riff_end - _in.offset() < 8) {
                    _fail(errc::corrupt, at, "webp: the data ends in a frame with no image");
                    return Step::failed;
                }
                uint8_t c[8];
                if (!_read(c, 8)) {
                    return Step::failed;
                }
                const uint64_t sub = _in.offset() - 8;
                const uint32_t n = le32(c + 4);
                const uint64_t padded = uint64_t(n) + (n & 1);
                if (padded > _riff_end - (sub + 8)) {
                    _fail(errc::corrupt, sub, "webp: a chunk past the RIFF size");
                    return Step::failed;
                }
                if (tag(c, "ALPH") && !alpha) {
                    if (sub + 8 + padded - start > payload) {
                        _fail(errc::corrupt, sub, "webp: a frame's ALPH chunk past its ANMF chunk");
                        return Step::failed;
                    }
                    alpha = true;
                    if (!_read_alph(n)) {
                        return Step::failed;
                    }
                    continue;
                }
                if (tag(c, "VP8L") || tag(c, "VP8 ")) {
                    if (sub + 8 + padded - start > payload) {
                        _fail(errc::corrupt, sub, "webp: a frame's image past its ANMF chunk");
                        return Step::failed;
                    }
                    if (!_bitstream(c, sub, n, f)) {
                        return Step::failed;
                    }
                    if (uint64_t(fx) + f.width > _canvas_width || uint64_t(fy) + f.height > _canvas_height) {
                        _fail(errc::corrupt, at, "webp: a frame outside the canvas");
                        return Step::failed;
                    }
                    f.x = fx;
                    f.y = fy;
                    f.duration = duration;
                    f.blend = blend;
                    f.dispose = dispose;
                    _in_anmf = true;
                    if (!_animated) {
                        return _after_frame() ? Step::end : Step::failed;
                    }
                    ++_images;
                    return Step::frame;
                }
                // not the frame's: the frame ends, the chunk is the top level's
                std::memcpy(_pending, c, 8);
                _pending_at = sub;
                _has_pending = true;
                break;
            }
            if (alpha) {
                _fail(errc::corrupt, at, "webp: a frame of an ALPH chunk and no image");
                return Step::failed;
            }
            return Step::end;
        }

        // An ALPH chunk's bytes kept for the VP8 image after it (their size
        // the chunk's: the image comes first in memory, its alpha when the
        // image's size is known)
        bool _read_alph(uint32_t size) {
            // with its padding byte, read over (_decode_alpha takes the
            // chunk's own bytes); the buffer grows with the bytes there are
            // (the size is the chunk's claim, bounded by the RIFF size its
            // header claims and nothing else)
            const size_t padded = size_t(size) + (size & 1);
            _alph_size = size;
            _alph.clear();
            if (!_read_into(_alph, 0, padded)) {
                return false;
            }
            _alph_pending = true;
            return true;
        }

        // The VP8 chunk in one piece: memory read in place, a stream's
        // copied into a buffer of the chunk's size
        bool _decode_lossy(const WebpFrame& f) {
            // the partitions may run into the padding byte: libwebp gives
            // the chunk with it
            const size_t size = size_t(f.size + (f.size & 1));
            const uint8_t* data;
            if constexpr (requires { _in.begin; }) {
                data = _in.begin + f.at + 8;
                if (!_skip(size - 10)) {
                    return false;
                }
            } else {
                // the chunk's claimed size grows the buffer only as its bytes come
                _vp8_buf.assign(_vp8_head, _vp8_head + 10);
                if (!_read_into(_vp8_buf, 10, size - 10)) {
                    return false;
                }
                data = _vp8_buf.data();
            }
            if (!_vp8.decode(data, size, f.at, _err)) {
                if (_vp8.damage() == vp8::Damage::overrun) {
                    _damage = WebpDamage::partition_overrun;
                } else if (_vp8.damage() == vp8::Damage::bad_start) {
                    _damage = WebpDamage::partition_start;
                }
                return false;
            }
            const uint32_t w = f.width, h = f.height;
            const uint8_t* alpha = nullptr;
            if (f.alpha) {
                if (!_decode_alpha(w, h, f.at)) {
                    return false;
                }
                alpha = _alpha_plane.data();
            }
            // the chroma upsampled row by row (the row of chroma nearest to
            // a luma row, and the one on its other side), the colors to ARGB
            // or, asked for (direct()), to the image's bytes
            uint8_t* const direct = _direct;
            _direct = nullptr;
            if (!direct) {
                _argb.resize(size_t(w) * h);
            }
            _upsampled.resize(size_t(w) * 2);
            _blend.resize(size_t(w + 1) / 2 + 2);
            const uint32_t ch = (h + 1) / 2;
            const size_t ys = _vp8.y_stride(), cs = _vp8.uv_stride();
            for (uint32_t y = 0; y < h; ++y) {
                const uint32_t near = y / 2;
                const uint32_t far = (y & 1) ? std::min(near + 1, ch - 1) : (near ? near - 1 : 0);
                uint8_t* ur = _upsampled.data();
                uint8_t* vr = ur + w;
                vp8::upsample_row(_vp8.u() + near * cs, _vp8.u() + far * cs, ur, w, _blend.data());
                vp8::upsample_row(_vp8.v() + near * cs, _vp8.v() + far * cs, vr, w, _blend.data());
                const uint8_t* arow = alpha ? alpha + size_t(y) * w : nullptr;
                if (!direct) {
                    vp8::yuv_to_argb(_vp8.y() + y * ys, ur, vr, arow, _argb.data() + size_t(y) * w, w);
                } else if (_direct_rgba) {
                    vp8::yuv_to_rgba(_vp8.y() + y * ys, ur, vr, arow, direct + y * _direct_stride, w);
                } else {
                    vp8::yuv_to_rgb(_vp8.y() + y * ys, ur, vr, direct + y * _direct_stride, w);
                }
            }
            _direct_done = direct != nullptr;
            return true;
        }

        // The alpha of a VP8 image (RFC 9649 §2.7.1.2): the header byte
        // (reserved bits 0, preprocessing 0 or 1, compression 0 or 1), the
        // plane raw or a VP8L image stream of the picture's size with no
        // header (the alpha its green), the filter undone
        // (Its bytes: the chunk's, not its padding byte. A stream that reads
        // past them is refused, WebpDamage::alpha_overrun: libwebp reads on,
        // into the padding in an animation's frame, into what its bit
        // reader holds in a still image.)
        bool _decode_alpha(uint32_t w, uint32_t h, uint64_t at) {
            const size_t length = _alph_size;
            if (length == 0) {
                return _fail(errc::corrupt, at, "webp: an empty ALPH chunk");
            }
            const unsigned head = _alph[0];
            const unsigned method = head & 3, filter = (head >> 2) & 3, pre = (head >> 4) & 3;
            if (method > 1 || pre > 1 || (head >> 6) != 0) {
                return _fail(errc::corrupt, at, "webp: an ALPH header of unknown values");
            }
            const size_t n = size_t(w) * h;
            _alpha_plane.resize(n);
            uint8_t* a = _alpha_plane.data();
            if (method == 0) {
                if (length - 1 < n) {
                    _damage = WebpDamage::alpha_overrun;
                    return _fail(errc::unexpected_end, at, "webp: raw alpha shorter than the picture");
                }
                std::memcpy(a, _alph.data() + 1, n);
            } else {
                MemoryInput in(slice<const byte>(reinterpret_cast<const byte*>(_alph.data() + 1), length - 1));
                if (!_alpha_vp8l.decode(in, length - 1, w, h, _err, at)) {
                    if (_err && _err->code() == errc::unexpected_end) {
                        _damage = WebpDamage::alpha_overrun;
                    }
                    return false;
                }
                const uint32_t* g = _alpha_vp8l.pixels();
                for (size_t i = 0; i < n; ++i) {
                    a[i] = uint8_t(g[i] >> 8);
                }
            }
            // the filters: predictors from the left (1), above (2), or A + B − C clipped (3)
            if (filter) {
                for (uint32_t x = 1; x < w; ++x) {
                    a[x] = uint8_t(a[x] + a[x - 1]);
                }
                for (uint32_t y = 1; y < h; ++y) {
                    uint8_t* row = a + size_t(y) * w;
                    const uint8_t* up = row - w;
                    row[0] = uint8_t(row[0] + up[0]);
                    for (uint32_t x = 1; x < w; ++x) {
                        int pred;
                        if (filter == 1) {
                            pred = row[x - 1];
                        } else if (filter == 2) {
                            pred = up[x];
                        } else {
                            pred = row[x - 1] + up[x] - up[x - 1];
                            pred = pred < 0 ? 0 : pred > 255 ? 255 : pred;
                        }
                        row[x] = uint8_t(row[x] + pred);
                    }
                }
            }
            return true;
        }

        // After a frame's bitstream: its padding passed over; in an ANMF
        // chunk, the chunk after the image read (an ALPH chunk after the
        // image refused, anything else the top level's)
        bool _after_frame() {
            _alph_pending = false;
            if (!_skip_to(_bitstream_end)) {
                return false;
            }
            if (_in_anmf) {
                _in_anmf = false;
                if (_riff_end - _in.offset() >= 8) {
                    uint8_t c[8];
                    if (!_read(c, 8)) {
                        return false;
                    }
                    const uint64_t sub = _in.offset() - 8;
                    if (tag(c, "ALPH")) {
                        return _fail(errc::corrupt, sub, "webp: an ALPH chunk after a frame's image");
                    }
                    std::memcpy(_pending, c, 8);
                    _pending_at = sub;
                    _has_pending = true;
                }
                return true;
            }
            return _skip_to(_frame_end);
        }

        bool _metadata(vector<byte>& out, uint64_t at, uint32_t size) {
            if (size > _o.limits.max_metadata) {
                return _fail(errc::too_large, at, "webp: metadata past limits.max_metadata");
            }
            out.clear();
            if (!_read_into(out, 0, size) || !_skip_padded(size, size)) {
                return false;
            }
            return true;
        }

        // The end of the RIFF data: an image was there
        bool _end() {
            if (_images == 0) {
                return _fail(errc::corrupt, _in.offset(), "webp: no image");
            }
            return true;
        }

        Input& _in;
        const decode_options& _o;
        Vp8lDecoder _vp8l;
        Vp8lHeader _header;
        vp8::Decoder _vp8;
        Vp8lDecoder _alpha_vp8l;
        uint8_t _vp8_head[10] = {};
        std::vector<uint8_t> _vp8_buf;      // a stream's VP8 chunk
        std::vector<uint8_t> _alph;         // the ALPH chunk's bytes and its padding byte
        size_t _alph_size = 0;              // the chunk's own size
        std::vector<uint8_t> _alpha_plane;
        std::vector<uint32_t> _argb;        // the last lossy frame
        uint8_t* _direct = nullptr;         // direct(): the next lossy frame's target
        size_t _direct_stride = 0;
        bool _direct_rgba = false;
        bool _direct_done = false;
        std::vector<uint8_t> _upsampled;    // a row of U and one of V at full width
        std::vector<uint16_t> _blend;
        bool _lossy = false;
        optional<error> _err;
        uint64_t _riff_end = 0;
        uint64_t _frame_end = 0;
        uint64_t _bitstream_end = 0;     // after the bitstream chunk's padding
        bool _in_anmf = false;
        uint8_t _pending[8] = {};        // a chunk's header a frame stopped at
        uint64_t _pending_at = 0;
        bool _has_pending = false;
        uint32_t _canvas_width = 0;
        uint32_t _canvas_height = 0;
        uint32_t _loops = 0;
        uint32_t _images = 0;
        bool _simple = false;
        bool _simple_alph = false;
        WebpDamage _damage = WebpDamage::none;         // an ALPH chunk after the simple format's image
        bool _animated = false;
        bool _anim_seen = false;
        bool _alpha = false;
        bool _alph_pending = false;
        WebpFrame _ready;          // the simple format's image, its header read by start()
        bool _has_ready = false;
        vector<byte> _icc;
        vector<byte> _exif;
    };

    // The canvas of an animation (RFC 9649 §2.7.2): ARGB words, transparent
    // at the start; a frame disposed of is cleared to transparent (the
    // ANIM background color is a hint RFC 9649 lets a reader leave, as
    // libwebp's WebPAnimDecoder and browsers do); a frame blended onto it
    // by RFC 9649's formula in integers, rounded to the nearest, the one
    // written over it replacing its rectangle.
    class WebpCanvas {
    public:
        void reset(uint32_t width, uint32_t height) {
            _width = width;
            _height = height;
            _pixels.assign(size_t(width) * height, 0);
            _has_prev = false;
        }

        void draw(const WebpFrame& f, const uint32_t* src) {
            if (_has_prev && _prev.dispose) {
                for (uint32_t y = 0; y < _prev.height; ++y) {
                    std::fill_n(_pixels.data() + size_t(_prev.y + y) * _width + _prev.x, _prev.width, 0u);
                }
            }
            for (uint32_t y = 0; y < f.height; ++y) {
                uint32_t* dst = _pixels.data() + size_t(f.y + y) * _width + f.x;
                const uint32_t* row = src + size_t(y) * f.width;
                if (!f.blend) {
                    std::memcpy(dst, row, size_t(f.width) * 4);
                } else {
                    for (uint32_t x = 0; x < f.width; ++x) {
                        dst[x] = blend(row[x], dst[x]);
                    }
                }
            }
            _prev = f;
            _has_prev = true;
        }

        const uint32_t* pixels() const noexcept {
            return _pixels.data();
        }

        // src over dst, not premultiplied: A = sA + dA(1 − sA/255), each
        // color (sC·sA + dC·dA(1 − sA/255)) / A, in integers scaled by 255
        // and rounded to the nearest; A = 0 gives transparent black
        static uint32_t blend(uint32_t src, uint32_t dst) noexcept {
            const uint32_t sa = src >> 24;
            if (sa == 255) {
                return src;
            }
            const uint32_t da = dst >> 24;
            if (sa == 0) {
                return da ? dst : 0;
            }
            const uint32_t weight = da * (255 - sa);       // dA(255 − sA): dst's share, × 255
            const uint32_t total = sa * 255 + weight;      // A × 255
            if (total == 0) {
                return 0;
            }
            const uint32_t a = (total + 127) / 255;
            uint32_t out = a << 24;
            for (unsigned s = 0; s < 24; s += 8) {
                const uint32_t sc = (src >> s) & 0xff, dc = (dst >> s) & 0xff;
                const uint32_t num = sc * sa * 255 + dc * weight;
                out |= ((num + total / 2) / total) << s;
            }
            return out;
        }

    private:
        uint32_t _width = 0;
        uint32_t _height = 0;
        std::vector<uint32_t> _pixels;
        WebpFrame _prev;
        bool _has_prev = false;
    };

    // ARGB words as an image of the format asked for
    inline image webp_image(const uint32_t* argb, uint32_t width, uint32_t height, pixel_format out) {
        image im(width, height, out);
        auto& s = ImageAccess::state(im);
        auto* dst = reinterpret_cast<uint8_t*>(s.pixels.data());
        if (out == pixel_format::rgba8) {
            vp8l::argb_to_rgba(argb, dst, size_t(width) * height);
        } else if (out == pixel_format::rgb8) {
            vp8l::argb_to_rgb(argb, dst, size_t(width) * height);
        } else {
            std::vector<uint8_t> row(size_t(width) * 4);
            const auto run = converter(pixel_format::rgba8, out);
            for (uint32_t y = 0; y < height; ++y) {
                vp8l::argb_to_rgba(argb + size_t(y) * width, row.data(), width);
                run(reinterpret_cast<const std::byte*>(row.data()), reinterpret_cast<std::byte*>(dst + y * s.stride), width);
            }
        }
        return im;
    }

    // The metadata read, onto the image: EXIF without the "Exif\0\0" some
    // writers put before its TIFF header, and its orientation
    inline void webp_metadata(image& im, vector<byte>& icc, vector<byte>& exif) {
        auto& s = ImageAccess::state(im);
        if (!exif.empty()) {
            if (exif.size() >= 6 && std::memcmp(exif.data(), "Exif\0\0", 6) == 0) {
                exif.erase(exif.begin(), exif.begin() + 6);
            }
            ImageAccess::set_orientation(im, exif_orientation(reinterpret_cast<const uint8_t*>(exif.data()), exif.size()));
            s.exif = std::move(exif);
        }
        if (!icc.empty()) {
            s.icc = std::move(icc);
        }
    }

    // A still image, or an animation's first frame on its canvas; the rest
    // of the file walked to its end (its frames' headers read, their data
    // passed over), for what libwebp refuses to be refused here too
    template<class Input>
    expected<image, error> webp_first(Input& in, const decode_options& o) {
        if (o.want && !valid(*o.want)) {
            return unexpected(error(errc::invalid_argument, 0, "webp: the pixel format outside the list"));
        }
        // on the stack: its error and metadata hold tracked words (Rule 1)
        WebpReader<Input> r(in, o);
        if (!r.start()) {
            return unexpected(*r.failure());
        }
        WebpFrame f;
        auto step = r.next(f);
        if (step != WebpReader<Input>::Step::frame) {
            return unexpected(*r.failure());
        }
        optional<image> result;
        // a still lossy image of rgb8 or rgba8: the decoder writes the
        // image's bytes itself, no ARGB between
        const pixel_format native = r.alpha() ? pixel_format::rgba8 : pixel_format::rgb8;
        const pixel_format want = o.want.value_or(native);
        if (!r.animated() && f.lossy && (want == pixel_format::rgb8 || want == pixel_format::rgba8)) {
            result.emplace(f.width, f.height, want);
            auto& s = ImageAccess::state(*result);
            r.direct(reinterpret_cast<uint8_t*>(s.pixels.data()), s.stride, want == pixel_format::rgba8);
        }
        if (!r.decode(f)) {
            return unexpected(*r.failure());
        }
        if (r.direct_done()) {
            // written already
        } else if (r.animated()) {
            WebpCanvas canvas;
            canvas.reset(r.canvas_width(), r.canvas_height());
            canvas.draw(f, r.pixels());
            result = webp_image(canvas.pixels(), r.canvas_width(), r.canvas_height(), o.want.value_or(pixel_format::rgba8));
        } else {
            result = webp_image(r.pixels(), f.width, f.height, want);
        }
        for (;;) {
            step = r.next(f);
            if (step == WebpReader<Input>::Step::end) {
                break;
            }
            if (step == WebpReader<Input>::Step::failed || !r.skip(f)) {
                return unexpected(*r.failure());
            }
        }
        webp_metadata(*result, r.icc(), r.exif());
        return std::move(*result);
    }

    // The frames of a WebP file, decoded as next() asks: each the canvas
    // after the frame is drawn, rgba8 unless the options ask for another
    // format; a still image is one frame
    template<class Input>
    struct WebpFrames final : FramesState {
        slice<const byte> data;   // the bytes read from memory (empty for a stream)
        Input input;
        decode_options options;
        WebpReader<Input> reader;
        WebpCanvas canvas;
        optional<error> failed;
        bool ended = false;

        template<class Source>
        WebpFrames(const slice<const byte>& d, const Source& source, const decode_options& o)
        : data(d), input(source), options(o), reader(input, options) {
        }

        expected<optional<frame>, error> next() override {
            if (failed) {
                return unexpected(*failed);
            }
            if (ended) {
                return optional<frame>();
            }
            WebpFrame f;
            const auto step = reader.next(f);
            plays = reader.animated() ? reader.loops() : 1;
            switch (step) {
                case WebpReader<Input>::Step::end:
                    ended = true;
                    return optional<frame>();
                case WebpReader<Input>::Step::frame:
                    if (reader.decode(f)) {
                        break;
                    }
                    [[fallthrough]];
                default:
                    failed = *reader.failure();
                    return unexpected(*failed);
            }
            canvas.draw(f, reader.pixels());
            image picture = webp_image(canvas.pixels(), width, height, options.want.value_or(pixel_format::rgba8));
            return optional<frame>(frame{picture, duration(std::chrono::milliseconds(f.duration))});
        }
    };

    template<class Input, class Source>
    expected<frames, error> webp_frames(const slice<const byte>& data, const Source& source, const decode_options& o) {
        if (o.want && !valid(*o.want)) {
            return unexpected(error(errc::invalid_argument, 0, "webp: the pixel format outside the list"));
        }
        auto s = make_tracked<WebpFrames<Input>>(data, source, o);
        if (!s->reader.start()) {
            return unexpected(*s->reader.failure());
        }
        s->width = s->reader.canvas_width();
        s->height = s->reader.canvas_height();
        s->canvas.reset(s->width, s->height);
        s->plays = s->reader.animated() ? s->reader.loops() : 1;
        tracked_ptr<FramesState> state = std::move(s);
        return FramesAccess::make(state);
    }
}
