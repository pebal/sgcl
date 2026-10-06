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

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace sgcl::codec {
    namespace detail {
        // QOI (the specification of 2022): a header of 14 bytes ("qoif", the
        // sides big-endian, the channels 3 or 4, the color space), then
        // chunks: RGB (0xFE), RGBA (0xFF), INDEX (00xxxxxx, the array of 64
        // colors seen, by the hash r·3 + g·5 + b·7 + a·11), DIFF (01, each
        // channel −2..1 off the previous pixel), LUMA (10, green −32..31 and
        // red and blue −8..7 off green's difference), RUN (11, the previous
        // pixel 1..62 times); the end: seven zeros and a one. The previous
        // pixel starts as opaque black.
        inline constexpr uint8_t QoiEnd[8] = {0, 0, 0, 0, 0, 0, 0, 1};

        SGCL_INLINE_HOT unsigned qoi_hash(uint32_t rgba) noexcept {
            const unsigned r = rgba & 0xff, g = (rgba >> 8) & 0xff, b = (rgba >> 16) & 0xff, a = rgba >> 24;
            return (r * 3 + g * 5 + b * 7 + a * 11) & 63;
        }

        // The decoder over an input of MemoryInput's shape: the header, the
        // size against the limits, then the chunks a row at a time into the
        // image (rgb8 for 3 channels, rgba8 for 4, or decode_options.want).
        // The end marker is not required (the reference decoder does not
        // look at it); data that ends before the last pixel is
        // unexpected_end.
        template<class Input>
        class QoiDecoder {
        public:
            SGCL_INLINE_HOT QoiDecoder(Input& in, const decode_options& o) noexcept
            : _in(in), _o(o) {
            }

            expected<image, error> run() noexcept(NothrowInput<Input>) {
                if (_o.want && !valid(*_o.want)) {
                    return unexpected(error(errc::invalid_argument, 0, "qoi: decode_options.want outside the list"));
                }
                uint8_t h[14];
                if (!_read(h, 14)) {
                    return unexpected(_failure(0));
                }
                if (std::memcmp(h, "qoif", 4) != 0) {
                    return unexpected(error(errc::corrupt, 0, "qoi: not a QOI signature"));
                }
                const uint32_t w = be32(h + 4), height = be32(h + 8);
                const unsigned channels = h[12];
                if ((channels != 3 && channels != 4) || h[13] > 1) {
                    return unexpected(error(errc::corrupt, 12, "qoi: channels other than 3 or 4, or a color space past 1"));
                }
                if (auto e = check_size(w, height, _o.limits, 4)) {
                    return unexpected(*e);
                }
                const pixel_format native = channels == 4 ? pixel_format::rgba8 : pixel_format::rgb8;
                const pixel_format f = _o.want ? *_o.want : native;
                image out(w, height, f);
                auto& s = ImageAccess::state(out);
                const ConvertRow convert = f == pixel_format::rgba8 ? nullptr : converter(pixel_format::rgba8, f);
                std::unique_ptr<uint32_t[]> row(new uint32_t[w]);
                uint32_t seen[64] = {};
                uint32_t px = 0xff000000u;   // r, g, b, a in the bytes from the lowest
                uint32_t run = 0;
                for (uint32_t y = 0; y < height; ++y) {
                    for (uint32_t x = 0; x < w; ++x) {
                        if (run > 0) {
                            --run;
                        } else {
                            uint8_t b0;
                            if (!_read(&b0, 1)) {
                                return unexpected(_failure(_in.offset()));
                            }
                            if (b0 == 0xFE || b0 == 0xFF) {
                                uint8_t c[4];
                                const size_t n = b0 == 0xFE ? 3 : 4;
                                if (!_read(c, n)) {
                                    return unexpected(_failure(_in.offset()));
                                }
                                px = uint32_t(c[0]) | uint32_t(c[1]) << 8 | uint32_t(c[2]) << 16 | (n == 4 ? uint32_t(c[3]) << 24 : px & 0xff000000u);
                            } else {
                                switch (b0 >> 6) {
                                    case 0:
                                        px = seen[b0];
                                        break;
                                    case 1: {
                                        const uint32_t r = ((px & 0xff) + ((b0 >> 4) & 3) - 2) & 0xff;
                                        const uint32_t g = (((px >> 8) & 0xff) + ((b0 >> 2) & 3) - 2) & 0xff;
                                        const uint32_t bl = (((px >> 16) & 0xff) + (b0 & 3) - 2) & 0xff;
                                        px = (px & 0xff000000u) | bl << 16 | g << 8 | r;
                                        break;
                                    }
                                    case 2: {
                                        uint8_t b1;
                                        if (!_read(&b1, 1)) {
                                            return unexpected(_failure(_in.offset()));
                                        }
                                        const int dg = int(b0 & 63) - 32;
                                        const int dr = dg + int(b1 >> 4) - 8, db = dg + int(b1 & 15) - 8;
                                        const uint32_t r = uint32_t(int(px & 0xff) + dr) & 0xff;
                                        const uint32_t g = uint32_t(int((px >> 8) & 0xff) + dg) & 0xff;
                                        const uint32_t bl = uint32_t(int((px >> 16) & 0xff) + db) & 0xff;
                                        px = (px & 0xff000000u) | bl << 16 | g << 8 | r;
                                        break;
                                    }
                                    default:
                                        run = b0 & 63;   // this pixel and `run` more
                                        break;
                                }
                            }
                            seen[qoi_hash(px)] = px;
                        }
                        row[x] = px;
                    }
                    std::byte* dst = s.pixels.data() + size_t(y) * s.stride;
                    if (convert) {
                        // the row as rgba8 bytes first (the words are in their order)
                        convert(reinterpret_cast<const std::byte*>(_bytes_of(row.get(), w)), dst, w);
                    } else {
                        sgcl::detail::copy_bytes(dst, _bytes_of(row.get(), w), size_t(w) * 4);
                    }
                }
                return out;
            }

        private:
            SGCL_INLINE_HOT static uint32_t be32(const uint8_t* p) noexcept {
                return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
            }

            // The words of a row as bytes r, g, b, a: in place on a
            // little-endian machine, where they lie so already
            const uint8_t* _bytes_of(uint32_t* row, uint32_t w) noexcept {
                if constexpr (std::endian::native == std::endian::big) {
                    for (uint32_t x = 0; x < w; ++x) {
                        const uint32_t v = row[x];
                        row[x] = (v & 0xff) << 24 | ((v >> 8) & 0xff) << 16 | ((v >> 16) & 0xff) << 8 | v >> 24;
                    }
                }
                return reinterpret_cast<const uint8_t*>(row);
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

            error _failure(uint64_t at) noexcept {
                if (_io && _in.failure) {
                    return *_in.failure;
                }
                return error(errc::unexpected_end, at, "qoi: the data ends before the last pixel");
            }

            Input& _in;
            const decode_options& _o;
            bool _io = false;
        };

        // The encoder: an image of any format as QOI of 4 channels when it
        // has alpha (an alpha format), 3 when not; 16-bit and gray formats
        // through image::convert's rules, a row at a time
        template<class Sink>
        bool qoi_encode(const image& im, Sink& sink) noexcept(NothrowSink<Sink>) {
            const auto& s = ImageAccess::state(im);
            const bool has_alpha = alpha(s.format);
            uint8_t h[14] = {'q', 'o', 'i', 'f'};
            const uint32_t w = s.width, height = s.height;
            for (int i = 0; i < 4; ++i) {
                h[4 + i] = uint8_t(w >> (24 - 8 * i));
                h[8 + i] = uint8_t(height >> (24 - 8 * i));
            }
            h[12] = has_alpha ? 4 : 3;
            h[13] = 0;
            if (!sink.put(h, 14)) {
                return false;
            }
            const ConvertRow convert = s.format == pixel_format::rgba8 ? nullptr : converter(s.format, pixel_format::rgba8);
            std::unique_ptr<uint8_t[]> row(new uint8_t[size_t(w) * 4]);
            // the chunks of a row at most 5 bytes a pixel, flushed a row at a time
            std::unique_ptr<uint8_t[]> out(new uint8_t[size_t(w) * 5 + 8]);
            uint32_t seen[64] = {};
            uint32_t prev = 0xff000000u;
            uint32_t run = 0;
            const uint8_t* pixels = reinterpret_cast<const uint8_t*>(s.pixels.data());
            for (uint32_t y = 0; y < height; ++y) {
                const uint8_t* src = pixels + size_t(y) * s.stride;
                if (convert) {
                    convert(reinterpret_cast<const std::byte*>(src), reinterpret_cast<std::byte*>(row.get()), w);
                    src = row.get();
                }
                uint8_t* o = out.get();
                for (uint32_t x = 0; x < w; ++x) {
                    const uint32_t px = uint32_t(src[4 * x]) | uint32_t(src[4 * x + 1]) << 8 | uint32_t(src[4 * x + 2]) << 16 | uint32_t(src[4 * x + 3]) << 24;
                    if (px == prev) {
                        if (++run == 62) {
                            *o++ = uint8_t(0xC0 | (run - 1));
                            run = 0;
                        }
                        continue;
                    }
                    if (run > 0) {
                        *o++ = uint8_t(0xC0 | (run - 1));
                        run = 0;
                    }
                    const unsigned hsh = qoi_hash(px);
                    if (seen[hsh] == px) {
                        *o++ = uint8_t(hsh);
                    } else {
                        seen[hsh] = px;
                        if ((px >> 24) == (prev >> 24)) {
                            const int dr = int8_t(uint8_t((px & 0xff) - (prev & 0xff)));
                            const int dg = int8_t(uint8_t(((px >> 8) & 0xff) - ((prev >> 8) & 0xff)));
                            const int db = int8_t(uint8_t(((px >> 16) & 0xff) - ((prev >> 16) & 0xff)));
                            const int dr_dg = dr - dg, db_dg = db - dg;
                            if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
                                *o++ = uint8_t(0x40 | (dr + 2) << 4 | (dg + 2) << 2 | (db + 2));
                            } else if (dg >= -32 && dg <= 31 && dr_dg >= -8 && dr_dg <= 7 && db_dg >= -8 && db_dg <= 7) {
                                *o++ = uint8_t(0x80 | (dg + 32));
                                *o++ = uint8_t((dr_dg + 8) << 4 | (db_dg + 8));
                            } else {
                                *o++ = 0xFE;
                                *o++ = uint8_t(px);
                                *o++ = uint8_t(px >> 8);
                                *o++ = uint8_t(px >> 16);
                            }
                        } else {
                            *o++ = 0xFF;
                            *o++ = uint8_t(px);
                            *o++ = uint8_t(px >> 8);
                            *o++ = uint8_t(px >> 16);
                            *o++ = uint8_t(px >> 24);
                        }
                    }
                    prev = px;
                }
                if (o != out.get() && !sink.put(out.get(), size_t(o - out.get()))) {
                    return false;
                }
            }
            if (run > 0) {
                const uint8_t r = uint8_t(0xC0 | (run - 1));
                if (!sink.put(&r, 1)) {
                    return false;
                }
            }
            return sink.put(QoiEnd, 8);
        }
    }

    // QOI, the Quite OK Image format: lossless RGB and RGBA in one pass,
    // its chunks indices into the colors seen, small differences from the
    // pixel before and runs. decode gives rgb8 or rgba8 as the header's
    // channels say (or decode_options.want); encode writes 4 channels for
    // an image of an alpha format, 3 for any other, every pixel as it is
    // (16-bit channels to 8, gray as RGB, CMYK through RGB).
    class qoi {
    public:
        // The file in memory, read in place
        SGCL_INLINE_HOT static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {}) noexcept {
            detail::MemoryInput in(data);
            return detail::QoiDecoder<detail::MemoryInput>(in, o).run();
        }

        // The file from a stream, read as it comes
        SGCL_INLINE_HOT static expected<image, error> decode(const io::reader& in, const decode_options& o = {}) {
            detail::ReaderInput source(in);
            return detail::QoiDecoder<detail::ReaderInput>(source, o).run();
        }

        // The file as bytes: every image encodes
        SGCL_INLINE_HOT static expected<vector<byte>, error> encode(const image& im) noexcept {
            vector<byte> out;
            detail::VectorSink sink{out, nullopt};
            detail::qoi_encode(im, sink);
            return out;
        }

        // The file into a stream: errc::io when the stream fails, at the
        // offset of the bytes written before
        SGCL_INLINE_HOT static expected<void, error> encode(const image& im, const io::writer& out) {
            detail::WriterSink sink{out, 0, nullopt};
            if (!detail::qoi_encode(im, sink)) {
                return unexpected(*sink.failure);
            }
            return {};
        }
    };
}
