//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "input.h"
#include "pixels.h"
#include "../error.h"
#include "../image.h"
#include "../options.h"
#include "../../compress/lzw.h"
#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"
#include "../../core/string.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace sgcl::codec::detail {
    // The GIF decoder (GIF89a, and GIF87a its subset), over an input of
    // MemoryInput's shape: the header and the logical screen, then the
    // blocks one by one. Each image's LZW data (least significant bit
    // first, the deferred clear GIF encoders use: compress's decoder) is
    // decoded from its sub-blocks into the frame's color indices, drawn onto
    // a canvas of the logical screen: its palette (local, else global), the
    // pixels of the transparent index of its Graphic Control Extension left
    // as they were, the rows of an interlaced image put in their places.
    // The canvas starts transparent, and what a frame leaves (its disposal)
    // is done before the next is drawn: 0 and 1 leave it, 2 clears its
    // rectangle to transparent (as browsers do; the background color is not
    // used), 3 puts back what was under it; 4 to 7 are taken as 1.
    //
    // Memory: the canvas (4 bytes a pixel), the indices of the largest frame
    // so far, and for disposal 3 a copy of the canvas, made the first time;
    // nothing per frame after that but the image handed out.
    //
    // Strict as Go is: data that ends early (unexpected_end), an unknown
    // block, a code past the LZW table, an image with fewer pixels than its
    // size, no color table (corrupt). Lenient: pixels past the size are
    // dropped, an index past the palette is opaque black, a frame reaching
    // past the logical screen is clipped to it.
    template<class Input>
    class GifDecoder {
    public:
        enum class Step : uint8_t { frame, end, failed };

        GifDecoder(Input& in, const decode_options& o) noexcept
        : _in(in), _o(o) {
        }

        // The header, the logical screen and its global color table, and
        // the extensions before the first image (the loop count among them)
        bool start() noexcept(NothrowInput<Input>) {
            uint8_t h[13];
            if (!_read(h, 13)) {
                return false;
            }
            if (std::memcmp(h, "GIF87a", 6) != 0 && std::memcmp(h, "GIF89a", 6) != 0) {
                return _fail(errc::corrupt, 0, "gif: not a GIF signature");
            }
            _width = uint32_t(h[6] | h[7] << 8);
            _height = uint32_t(h[8] | h[9] << 8);
            if (auto e = check_size(_width, _height, _o.limits, 6)) {
                _err = *e;
                return false;
            }
            if (h[10] & 0x80) {
                _global_size = 2u << (h[10] & 7);
                if (!_palette(_global, _global_size)) {
                    return false;
                }
            }
            _canvas.reset(new uint8_t[size_t(_width) * _height * 4]());
            return _extensions();
        }

        // The next image drawn onto the canvas (the one before it disposed
        // of first): frame; the trailer: end
        Step next() noexcept(NothrowInput<Input>) {
            if (_ended) {
                return Step::end;
            }
            _dispose();
            for (;;) {
                int b;
                if (!_byte(b)) {
                    return Step::failed;
                }
                if (b < 0) {
                    _fail(errc::unexpected_end, _in.offset(), "gif: the data ends before the trailer");
                    return Step::failed;
                }
                if (b == 0x3B) {
                    _ended = true;
                    return Step::end;
                }
                if (b == 0x21) {
                    if (!_extension()) {
                        return Step::failed;
                    }
                    continue;
                }
                if (b == 0x2C) {
                    return _image() ? Step::frame : Step::failed;
                }
                _fail(errc::corrupt, _in.offset() - 1, "gif: an unknown block introducer " + std::to_string(b));
                return Step::failed;
            }
        }

        uint32_t width() const noexcept {
            return _width;
        }

        uint32_t height() const noexcept {
            return _height;
        }

        // How many times the animation plays: 0 forever; NETSCAPE2.0's
        // count n is n + 1 plays (the first and n more); none: 1
        uint32_t plays() const noexcept {
            return _plays;
        }

        // The delay of the last frame, hundredths of a second
        unsigned delay() const noexcept {
            return _delay;
        }

        const uint8_t* canvas() const noexcept {
            return _canvas.get();
        }

        const optional<error>& failure() const noexcept {
            return _err;
        }

        // The canvas as an image of the format asked for (rgba8 when none;
        // gif_first and gif_frames check the format before the decoder is made)
        image picture() const noexcept {
            const pixel_format out = _o.want.value_or(pixel_format::rgba8);
            image im(_width, _height, out);
            auto& s = ImageAccess::state(im);
            auto* dst = reinterpret_cast<uint8_t*>(s.pixels.data());
            const size_t row = size_t(_width) * 4;
            if (out == pixel_format::rgba8) {
                sgcl::detail::copy_bytes(dst, _canvas.get(), row * _height);
            } else {
                const auto run = converter(pixel_format::rgba8, out);
                for (uint32_t y = 0; y < _height; ++y) {
                    run(reinterpret_cast<const std::byte*>(_canvas.get() + y * row), reinterpret_cast<std::byte*>(dst + y * s.stride), _width);
                }
            }
            return im;
        }

    private:
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
                    return _fail(errc::unexpected_end, _in.offset(), "gif: the data ends in the middle");
                }
                sgcl::detail::copy_bytes(dst, p, got);
                _in.consume(got);
                dst += got;
                n -= got;
            }
            return true;
        }

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

        bool _palette(uint8_t (&table)[256][4], unsigned n) noexcept(NothrowInput<Input>) {
            uint8_t rgb[768];
            if (!_read(rgb, 3 * size_t(n))) {
                return false;
            }
            for (unsigned i = 0; i < 256; ++i) {
                if (i < n) {
                    table[i][0] = rgb[3 * i];
                    table[i][1] = rgb[3 * i + 1];
                    table[i][2] = rgb[3 * i + 2];
                } else {
                    table[i][0] = table[i][1] = table[i][2] = 0;
                }
                table[i][3] = 255;
            }
            return true;
        }

        // Sub-blocks (a length, that many bytes) to the terminator of length 0
        bool _skip_blocks() noexcept(NothrowInput<Input>) {
            for (;;) {
                uint8_t n;
                if (!_read(&n, 1)) {
                    return false;
                }
                if (n == 0) {
                    return true;
                }
                uint8_t buf[255];
                if (!_read(buf, n)) {
                    return false;
                }
            }
        }

        // The extensions before an image: read while the next block is one
        bool _extensions() noexcept(NothrowInput<Input>) {
            for (;;) {
                const uint8_t* p;
                size_t got;
                if (!_in.peek(1, p, got)) {
                    return _fail_input();
                }
                if (!got || *p != 0x21) {
                    return true;
                }
                _in.consume(1);
                if (!_extension()) {
                    return false;
                }
            }
        }

        // An extension, its introducer read: the Graphic Control Extension
        // for the next image, NETSCAPE2.0 (or ANIMEXTS1.0) for the loop
        // count; comments, plain text and the rest passed over
        bool _extension() noexcept(NothrowInput<Input>) {
            uint8_t label;
            if (!_read(&label, 1)) {
                return false;
            }
            if (label == 0xF9) {
                uint8_t n;
                if (!_read(&n, 1)) {
                    return false;
                }
                uint8_t body[255];
                if (!_read(body, n)) {
                    return false;
                }
                if (n >= 4) {
                    _gce = true;
                    _gce_disposal = (body[0] >> 2) & 7;
                    _gce_transparent = body[0] & 1;
                    _gce_delay = unsigned(body[1] | body[2] << 8);
                    _gce_index = body[3];
                }
                return _skip_blocks();
            }
            if (label == 0xFF) {
                uint8_t n;
                if (!_read(&n, 1)) {
                    return false;
                }
                uint8_t id[255];
                if (!_read(id, n)) {
                    return false;
                }
                const bool looping = n == 11 && (std::memcmp(id, "NETSCAPE2.0", 11) == 0 || std::memcmp(id, "ANIMEXTS1.0", 11) == 0);
                for (bool first = true;; first = false) {
                    uint8_t m;
                    if (!_read(&m, 1)) {
                        return false;
                    }
                    if (m == 0) {
                        return true;
                    }
                    uint8_t buf[255];
                    if (!_read(buf, m)) {
                        return false;
                    }
                    if (looping && first && m >= 3 && buf[0] == 1) {
                        const unsigned count = unsigned(buf[1] | buf[2] << 8);
                        _plays = count == 0 ? 0 : count + 1;
                    }
                }
            }
            return _skip_blocks();
        }

        // What the frame before asked to be done with its rectangle
        void _dispose() noexcept {
            if (_last_disposal == 2 || _last_disposal == 3) {
                const size_t row = size_t(_width) * 4;
                for (uint32_t y = _last_top; y < _last_bottom; ++y) {
                    uint8_t* d = _canvas.get() + y * row + size_t(_last_left) * 4;
                    const size_t n = size_t(_last_right - _last_left) * 4;
                    if (_last_disposal == 2) {
                        // libc's memset, not fill_bytes: a zero fill of up
                        // to a quarter megabyte a row (DESIGN 393)
                        std::memset(d, 0, n);
                    } else {
                        sgcl::detail::copy_bytes(d, _saved.get() + y * row + size_t(_last_left) * 4, n);
                    }
                }
            }
            _last_disposal = 0;
        }

        bool _image() noexcept(NothrowInput<Input>) {

            const uint64_t at = _in.offset() - 1;
            uint8_t d[9];
            if (!_read(d, 9)) {
                return false;
            }
            const uint32_t left = uint32_t(d[0] | d[1] << 8), top = uint32_t(d[2] | d[3] << 8);
            const uint32_t w = uint32_t(d[4] | d[5] << 8), h = uint32_t(d[6] | d[7] << 8);
            const bool interlaced = d[8] & 0x40;
            const uint8_t (*palette)[4] = _global;
            unsigned palette_size = _global_size;
            if (d[8] & 0x80) {
                palette_size = 2u << (d[8] & 7);
                if (!_palette(_local, palette_size)) {
                    return false;
                }
                palette = _local;
            }
            if (palette_size == 0) {
                return _fail(errc::corrupt, at, "gif: an image with no color table");
            }
            if (uint64_t(w) * h > _o.limits.max_pixels) {
                return _fail(errc::too_large, at, "gif: a frame past limits.max_pixels");
            }
            uint8_t lzw;
            if (!_read(&lzw, 1)) {
                return false;
            }
            if (lzw < 2 || lzw > 8) {
                return _fail(errc::corrupt, at, "gif: an LZW code size of " + std::to_string(lzw));
            }
            // the frame's disposal and delay, from its GCE (which is used up)
            const bool transparent = _gce && _gce_transparent;
            const uint8_t transparent_index = _gce_index;
            _delay = _gce ? _gce_delay : 0;
            unsigned disposal = _gce ? _gce_disposal : 0;
            _gce = false;
            if (disposal > 3) {
                disposal = 1;
            }
            // room for the image and one string more (4096 codes long at
            // most): the decoder writes a string whole, so the one that
            // crosses the image's last pixel fills it, the rest dropped
            const size_t count = size_t(w) * h;
            const size_t room = count + compress::detail::LzwCodes;
            if (_indices.size() < room) {
                _indices.resize(room);
            }
            // the indices, from the sub-blocks through the LZW decoder
            // the LZW decoder's tables (24 KB, no tracked words): one block for
            // the decoder's life, made again in place for each image
            if (!_lzw) {
                _lzw = std::make_unique<compress::detail::LzwDecoder>(false, int(lzw));
            } else {
                _lzw->~LzwDecoder();
                new (_lzw.get()) compress::detail::LzwDecoder(false, int(lzw));
            }
            size_t pos = 0;
            bool stopped = false;
            for (;;) {
                uint8_t n;
                if (!_read(&n, 1)) {
                    return false;
                }
                if (n == 0) {
                    break;
                }
                uint8_t buf[255];
                if (!_read(buf, n)) {
                    return false;
                }
                if (stopped) {
                    continue;
                }
                const uint8_t* p = buf;
                auto status = _lzw->decode(p, buf + n, false, _indices.data(), pos, room);
                if (status == compress::detail::LzwStatus::failed) {
                    return _fail(errc::corrupt, _in.offset(), std::string("gif: ") + (_lzw->error_text ? _lzw->error_text : "corrupt LZW data"));
                }
                if (status == compress::detail::LzwStatus::done || status == compress::detail::LzwStatus::need_room || pos >= count) {
                    stopped = true;   // the end code, or the image whole (what follows dropped)
                }
            }
            if (pos < count) {
                return _fail(errc::corrupt, _in.offset(), "gif: an image with fewer pixels than its size");
            }
            // the frame's rectangle on the canvas, clipped to it; for
            // disposal 3, what is under it kept
            const uint32_t l = std::min(left, _width), t = std::min(top, _height);
            const uint32_t right = uint32_t(std::min<uint64_t>(_width, uint64_t(left) + w));
            const uint32_t bottom = uint32_t(std::min<uint64_t>(_height, uint64_t(top) + h));
            if (disposal == 3) {
                if (!_saved) {
                    _saved.reset(new uint8_t[size_t(_width) * _height * 4]);
                }
                const size_t row = size_t(_width) * 4;
                for (uint32_t y = t; y < bottom; ++y) {
                    sgcl::detail::copy_bytes(_saved.get() + y * row + size_t(l) * 4, _canvas.get() + y * row + size_t(l) * 4, size_t(right - l) * 4);
                }
            }
            // drawn: the rows in their places (interlaced: every 8th row from
            // 0, every 8th from 4, every 4th from 2, every 2nd from 1)
            static constexpr uint32_t Start[4] = {0, 4, 2, 1}, Step_[4] = {8, 8, 4, 2};
            uint32_t pass = 0, fy = 0;
            for (uint32_t r = 0; r < h; ++r) {
                uint32_t y;
                if (interlaced) {
                    while (pass < 4 && Start[pass] + fy * Step_[pass] >= h) {
                        ++pass;
                        fy = 0;
                    }
                    y = Start[pass] + fy * Step_[pass];
                    ++fy;
                } else {
                    y = r;
                }
                const uint32_t cy = top + y;
                if (cy >= _height) {
                    continue;
                }
                const uint8_t* src = _indices.data() + size_t(r) * w;
                uint8_t* dst = _canvas.get() + (size_t(cy) * _width) * 4;
                for (uint32_t x = 0; x < w; ++x) {
                    const uint32_t cx = left + x;
                    if (cx >= _width) {
                        break;
                    }
                    const uint8_t i = src[x];
                    if (transparent && i == transparent_index) {
                        continue;
                    }
                    if (i < palette_size) {
                        std::memcpy(dst + size_t(cx) * 4, palette[i], 4);
                    } else {
                        uint8_t* q = dst + size_t(cx) * 4;
                        q[0] = q[1] = q[2] = 0;
                        q[3] = 255;
                    }
                }
            }
            _last_disposal = disposal;
            _last_left = l;
            _last_top = t;
            _last_right = right;
            _last_bottom = bottom;
            return true;
        }

        Input& _in;
        const decode_options& _o;
        optional<error> _err;

        uint32_t _width = 0, _height = 0;
        uint8_t _global[256][4] = {};
        uint8_t _local[256][4] = {};
        unsigned _global_size = 0;
        uint32_t _plays = 1;
        bool _ended = false;

        // the Graphic Control Extension waiting for its image
        bool _gce = false;
        unsigned _gce_disposal = 0;
        bool _gce_transparent = false;
        unsigned _gce_delay = 0;
        uint8_t _gce_index = 0;

        // the last frame: its delay, and what is to be done with its rectangle
        unsigned _delay = 0;
        unsigned _last_disposal = 0;
        uint32_t _last_left = 0, _last_top = 0, _last_right = 0, _last_bottom = 0;

        std::unique_ptr<uint8_t[]> _canvas;
        std::unique_ptr<uint8_t[]> _saved;
        std::vector<uint8_t> _indices;
        std::unique_ptr<compress::detail::LzwDecoder> _lzw;
    };
}
