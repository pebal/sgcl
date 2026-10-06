//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "image.h"
#include "jpeg.h"
#include "options.h"
#include "detail/input.h"
#include "detail/output.h"
#include "detail/pixels.h"
#include "detail/tiff_codecs.h"
#include "../compress/level.h"
#include "../compress/zlib.h"
#include "../core/aliases.h"
#include "../core/detail/bytes.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/vector.h"
#include "../io/stream.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace sgcl::codec {
    namespace detail {
        // tiff::compression and tiff::options, outside the class (a nested
        // struct with member initializers cannot be a default argument
        // inside its class)
        enum class TiffCompression : uint8_t {
            none,
            lzw,       // with the horizontal predictor
            deflate    // zlib's, with the horizontal predictor
        };

        struct TiffOptions {
            TiffCompression compression = TiffCompression::lzw;
        };

        // The directory of a page (an IFD): what decoding needs of it
        struct TiffPage {
            uint32_t width = 0, height = 0;
            unsigned samples = 1;
            unsigned bits = 1;
            unsigned compression = 1;
            unsigned photometric = 1;
            unsigned planar = 1;
            unsigned predictor = 1;
            unsigned extra = 0;          // the first extra sample: 1 associated alpha, 2 unassociated, 0 none or unspecified
            unsigned sample_format = 1;
            unsigned fill_order = 1;
            unsigned orientation = 1;
            uint32_t rows_per_strip = 0xFFFFFFFFu;
            uint32_t tile_width = 0, tile_height = 0;
            std::vector<uint64_t> offsets, counts;
            std::vector<uint16_t> colormap;
            std::vector<uint8_t> jpeg_tables;
            std::vector<uint8_t> icc;
            uint64_t next = 0;           // the next IFD's offset, 0 for none
        };

        // The TIFF reader (TIFF 6.0, Adobe's 1992 specification, with the
        // Deflate and JPEG of the technical notes) over the file in memory:
        // the header ("II" or "MM", 42), a chain of IFDs, each a page; per
        // page the strips or tiles, decompressed (none, PackBits, LZW,
        // Deflate both codes, JPEG through the module's decoder with
        // JPEGTables in front), the horizontal predictor undone, the
        // samples to pixels: bilevel and gray of 1, 2, 4, 8 and 16 bits
        // (WhiteIsZero inverted), palette, RGB and CMYK of 8 and 16, an
        // extra sample as alpha (associated alpha divided out), chunky or
        // planar. Floating point samples, old JPEG (6), CCITT, LAB and
        // uncompressed YCbCr are unsupported. The orientation tag and the
        // ICC profile come with the image.
        class TiffReader {
        public:
            TiffReader(const slice<const byte>& data, const decode_options& o) noexcept
            : _p(reinterpret_cast<const uint8_t*>(data.data())), _n(data.size()), _o(o) {
            }

            // The header: false with the error
            bool start() noexcept {
                if (_n < 8) {
                    return _fail(errc::unexpected_end, _n, "tiff: the data ends in the header");
                }
                if (_p[0] == 'I' && _p[1] == 'I') {
                    _le = true;
                } else if (_p[0] == 'M' && _p[1] == 'M') {
                    _le = false;
                } else {
                    return _fail(errc::corrupt, 0, "tiff: not a TIFF byte order");
                }
                const unsigned magic = _u16(2);
                if (magic == 43) {
                    return _fail(errc::unsupported, 2, "tiff: BigTIFF");
                }
                if (magic != 42) {
                    return _fail(errc::corrupt, 2, "tiff: not TIFF's 42");
                }
                _next = _u32(4);
                return true;
            }

            // Whether a page follows
            SGCL_INLINE_HOT bool more() const noexcept {
                return _next != 0;
            }

            // The next page's image: false with the error
            expected<image, error> next() noexcept {
                TiffPage pg;
                if (!_ifd(_next, pg)) {
                    return unexpected(_err);
                }
                _next = pg.next;
                return _page(pg);
            }

            SGCL_INLINE_HOT const error& failure() const noexcept {
                return _err;
            }

        private:
            bool _fail(errc code, uint64_t at, const char* what) noexcept {
                _err = error(code, at, string(what));
                return false;
            }

            SGCL_INLINE_HOT unsigned _u16(uint64_t at) const noexcept {
                return _le ? unsigned(_p[at]) | unsigned(_p[at + 1]) << 8 : unsigned(_p[at]) << 8 | _p[at + 1];
            }

            SGCL_INLINE_HOT uint32_t _u32(uint64_t at) const noexcept {
                return _le ? uint32_t(_u16(at)) | uint32_t(_u16(at + 2)) << 16 : uint32_t(_u16(at)) << 16 | _u16(at + 2);
            }

            // A field's values as numbers (BYTE, SHORT, LONG; the first of
            // RATIONALs' numerators otherwise ignored): false when the
            // values lie outside the data
            bool _values(uint64_t entry, std::vector<uint64_t>& out, uint64_t most) noexcept {
                const unsigned type = _u16(entry + 2);
                const uint64_t count = _u32(entry + 4);
                static constexpr unsigned sizes[19] = {0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8, 4, 0, 0, 8, 8, 8};
                const unsigned size = type < 19 ? sizes[type] : 0;
                if (size == 0) {
                    out.clear();
                    return true;   // an unknown type: no values
                }
                if (count > most) {
                    return _fail(errc::too_large, entry, "tiff: a field of more values than the image can have");
                }
                const uint64_t bytes = count * size;
                const uint64_t at = bytes <= 4 ? entry + 8 : _u32(entry + 8);
                if (at + bytes > _n) {
                    return _fail(errc::corrupt, entry, "tiff: a field's values outside the file");
                }
                out.resize(size_t(count));
                for (uint64_t i = 0; i < count; ++i) {
                    switch (type) {
                        case 1: case 2: case 6: case 7: out[i] = _p[at + i]; break;
                        case 3: case 8: out[i] = _u16(at + 2 * i); break;
                        case 4: case 9: case 13: out[i] = _u32(at + 4 * i); break;
                        default: out[i] = _u32(at + size * i); break;
                    }
                }
                return true;
            }

            bool _ifd(uint64_t at, TiffPage& pg) noexcept {
                if (++_pages > 65536 || at < 8 || at + 2 > _n) {
                    return _fail(at + 2 > _n ? errc::corrupt : errc::corrupt, at, "tiff: an IFD outside the file, or a chain of IFDs past 65536");
                }
                for (uint64_t seen : _visited) {
                    if (seen == at) {
                        return _fail(errc::corrupt, at, "tiff: a chain of IFDs that loops");
                    }
                }
                _visited.push_back(at);
                const unsigned count = _u16(at);
                if (at + 2 + uint64_t(count) * 12 + 4 > _n) {
                    return _fail(errc::unexpected_end, _n, "tiff: the data ends in an IFD");
                }
                std::vector<uint64_t> v;
                const uint64_t most = uint64_t(_n);   // no field holds more values than the file has bytes
                for (unsigned i = 0; i < count; ++i) {
                    const uint64_t e = at + 2 + uint64_t(i) * 12;
                    const unsigned tag = _u16(e);
                    if (!_values(e, v, most)) {
                        return false;
                    }
                    const uint64_t first = v.empty() ? 0 : v[0];
                    switch (tag) {
                        case 256: pg.width = uint32_t(first); break;
                        case 257: pg.height = uint32_t(first); break;
                        case 258: pg.bits = unsigned(first); break;   // every sample the same: the first
                        case 259: pg.compression = unsigned(first); break;
                        case 262: pg.photometric = unsigned(first); break;
                        case 266: pg.fill_order = unsigned(first); break;
                        case 273: case 324: pg.offsets = v; break;
                        case 274: pg.orientation = unsigned(first); break;
                        case 277: pg.samples = unsigned(first); break;
                        case 278: pg.rows_per_strip = uint32_t(first); break;
                        case 279: case 325: pg.counts = v; break;
                        case 284: pg.planar = unsigned(first); break;
                        case 317: pg.predictor = unsigned(first); break;
                        case 320: pg.colormap.assign(v.begin(), v.end()); break;
                        case 322: pg.tile_width = uint32_t(first); break;
                        case 323: pg.tile_height = uint32_t(first); break;
                        case 338: pg.extra = unsigned(first); break;
                        case 339: pg.sample_format = unsigned(first); break;
                        case 347:
                        case 34675: {
                            // bytes kept: JPEG's tables, the ICC profile
                            const uint64_t n = _u32(e + 4);
                            const unsigned type = _u16(e + 2);
                            if (type != 7 && type != 1) {
                                break;
                            }
                            if (tag == 34675 && (!_o.metadata || n > _o.limits.max_metadata)) {
                                break;
                            }
                            const uint64_t src = n <= 4 ? e + 8 : _u32(e + 8);
                            auto& dst = tag == 347 ? pg.jpeg_tables : pg.icc;
                            dst.assign(_p + src, _p + src + n);   // inside the file: _values checked it
                            break;
                        }
                        default: break;
                    }
                }
                pg.next = _u32(at + 2 + uint64_t(count) * 12);
                return true;
            }

            // A strip's or tile's bytes decompressed into out (cap bytes
            // wanted): false with the error
            bool _chunk(const TiffPage& pg, size_t index, uint8_t* out, size_t cap) noexcept {
                const uint64_t off = pg.offsets[index], cnt = pg.counts[index];
                if (off > _n || cnt > _n - off) {
                    return _fail(errc::corrupt, off, "tiff: a strip or tile outside the file");
                }
                const uint8_t* in = _p + off;
                switch (pg.compression) {
                    case 1: {
                        if (cnt < cap) {
                            return _fail(errc::unexpected_end, off, "tiff: an uncompressed strip shorter than its rows");
                        }
                        sgcl::detail::copy_bytes(out, in, cap);
                        return true;
                    }
                    case 32773:
                        if (tiff_codec::unpackbits(in, size_t(cnt), out, cap) < cap) {
                            return _fail(errc::unexpected_end, off, "tiff: a PackBits strip shorter than its rows");
                        }
                        return true;
                    case 5: {
                        bool ok;
                        const size_t got = tiff_codec::unlzw(in, size_t(cnt), out, cap, ok);
                        if (!ok) {
                            return _fail(errc::corrupt, off, "tiff: an LZW code past the table");
                        }
                        if (got < cap) {
                            return _fail(errc::unexpected_end, off, "tiff: an LZW strip shorter than its rows");
                        }
                        return true;
                    }
                    case 8:
                    case 32946: {
                        auto z = compress::zlib::decompress(slice<const byte>(reinterpret_cast<const byte*>(in), size_t(cnt)), compress::limits{cap, cap, 1});
                        if (!z) {
                            if (z.error().code() == compress::errc::too_large) {
                                // more than its rows: the rows taken (libtiff does)
                                auto partial = _inflate_prefix(in, size_t(cnt), out, cap);
                                if (partial) {
                                    return true;
                                }
                            }
                            return _fail(errc::corrupt, off, "tiff: a Deflate strip that does not inflate");
                        }
                        if (z->size() < cap) {
                            return _fail(errc::unexpected_end, off, "tiff: a Deflate strip shorter than its rows");
                        }
                        sgcl::detail::copy_bytes(out, z->data(), cap);
                        return true;
                    }
                    default:
                        return _fail(errc::unsupported, off, "tiff: a compression the module does not read");
                }
            }

            // A zlib stream's first cap bytes, for a strip that holds more
            bool _inflate_prefix(const uint8_t* in, size_t n, uint8_t* out, size_t cap) noexcept {
                auto z = compress::zlib::decompress(slice<const byte>(reinterpret_cast<const byte*>(in), n), compress::limits{uint64_t(cap) * 2 + 65536, uint64_t(cap) * 2 + 65536, 1});
                if (!z || z->size() < cap) {
                    return false;
                }
                sgcl::detail::copy_bytes(out, z->data(), cap);
                return true;
            }

            expected<image, error> _page(TiffPage& pg) noexcept {
                if (pg.width == 0 || pg.height == 0) {
                    return unexpected(error(errc::corrupt, 0, "tiff: a page of zero pixels"));
                }
                if (auto e = check_size(pg.width, pg.height, _o.limits, 0)) {
                    return unexpected(*e);
                }
                if (pg.sample_format != 1 && pg.sample_format != 4) {
                    return unexpected(error(errc::unsupported, 0, "tiff: samples other than unsigned integers"));
                }
                const unsigned ph = pg.photometric;
                const unsigned color = ph == 2 ? 3 : ph == 5 ? 4 : 1;   // the color samples
                if (ph > 6 || ph == 4 || (ph == 6 && pg.compression != 7)) {
                    return unexpected(error(errc::unsupported, 0, "tiff: a photometric interpretation the module does not read"));
                }
                if (pg.compression == 7) {
                    return _jpeg_page(pg);
                }
                const unsigned bits = pg.bits;
                const bool ok_bits = (ph <= 1 || ph == 3) ? (bits == 1 || bits == 2 || bits == 4 || bits == 8 || bits == 16) : (bits == 8 || bits == 16);
                if (!ok_bits || pg.samples < color || pg.samples > color + 1 + 2 || (ph == 3 && pg.samples != 1)) {
                    return unexpected(error(errc::unsupported, 0, "tiff: samples of a depth or count the module does not read"));
                }
                if (pg.predictor != 1 && pg.predictor != 2) {
                    return unexpected(error(errc::unsupported, 0, "tiff: a predictor other than none or horizontal"));
                }
                if (ph == 3 && pg.colormap.size() < 3u << bits) {
                    return unexpected(error(errc::corrupt, 0, "tiff: a palette image without its color map"));
                }
                // an extra sample is alpha when ExtraSamples says so (as Go
                // and libtiff read it); CMYK keeps four samples
                const bool alpha = pg.samples > color && (pg.extra == 1 || pg.extra == 2) && ph != 5;
                const bool wide16 = bits == 16;
                pixel_format native;
                if (ph == 3) {
                    native = pixel_format::rgb8;
                } else if (ph == 5) {
                    native = pixel_format::cmyk8;
                } else if (color == 1) {
                    native = alpha ? (wide16 ? pixel_format::gray_alpha16 : pixel_format::gray_alpha8) : (wide16 ? pixel_format::gray16 : pixel_format::gray8);
                } else {
                    native = alpha ? (wide16 ? pixel_format::rgba16 : pixel_format::rgba8) : (wide16 ? pixel_format::rgb16 : pixel_format::rgb8);
                }
                const unsigned used = ph == 5 ? 4 : color + (alpha ? 1 : 0);   // the samples the image takes
                // the layout of the chunks
                const bool tiled = pg.tile_width && pg.tile_height;
                const uint32_t cw = tiled ? pg.tile_width : pg.width;
                const uint32_t chh = tiled ? pg.tile_height : std::min(pg.rows_per_strip ? pg.rows_per_strip : pg.height, pg.height);
                if (tiled && (pg.tile_width % 16 || pg.tile_height % 16)) {
                    return unexpected(error(errc::corrupt, 0, "tiff: a tile's side not a multiple of 16"));
                }
                const uint32_t across = (pg.width + cw - 1) / cw, down = (pg.height + chh - 1) / chh;
                const unsigned planes = pg.planar == 2 ? pg.samples : 1;
                const unsigned per_chunk = pg.planar == 2 ? 1 : pg.samples;
                const uint64_t chunks = uint64_t(across) * down * planes;
                if (pg.offsets.size() < chunks || pg.counts.size() < chunks) {
                    return unexpected(error(errc::corrupt, 0, "tiff: fewer strip or tile offsets than the page has"));
                }
                const size_t chunk_row = (size_t(cw) * per_chunk * bits + 7) / 8;
                const size_t chunk_bytes = chunk_row * chh;
                // the common case straight into the image: strips of whole
                // rows, chunky, 8 or 16 bits, samples as the image holds
                // them (no inversion, palette or alpha to divide out)
                if (!tiled && pg.planar == 1 && (bits == 8 || bits == 16) && (ph == 1 || ph == 2) && pg.samples == used && !(alpha && pg.extra == 1)) {
                    image out(pg.width, pg.height, native);
                    auto& s = ImageAccess::state(out);
                    uint8_t* px = reinterpret_cast<uint8_t*>(s.pixels.data());
                    for (uint32_t cy = 0; cy < down; ++cy) {
                        const uint32_t rows = std::min(chh, pg.height - cy * chh);
                        uint8_t* dst = px + size_t(cy) * chh * s.stride;
                        if (!_chunk(pg, cy, dst, s.stride * rows)) {
                            return unexpected(_err);
                        }
                        if (pg.predictor == 2) {
                            _unpredict(dst, s.stride, rows, pg.width, pg.samples, bits);
                        }
                        if (bits == 16 && _le != (std::endian::native == std::endian::little)) {
                            for (size_t i = 0; i < s.stride * rows; i += 2) {
                                std::swap(dst[i], dst[i + 1]);
                            }
                        }
                    }
                    _metadata(out, pg);
                    if (_o.want && *_o.want != native) {
                        return out.convert(*_o.want);
                    }
                    return out;
                }
                std::unique_ptr<uint8_t[]> buf(new uint8_t[chunk_bytes]);
                // the samples of the whole page, unpacked to 8 or 16 bits a
                // sample, chunky: samples × width a row
                const size_t sample_bytes = wide16 ? 2 : 1;
                const size_t page_row = size_t(pg.width) * pg.samples * sample_bytes;
                std::unique_ptr<uint8_t[]> page(new uint8_t[page_row * pg.height]());
                for (unsigned plane = 0; plane < planes; ++plane) {
                    for (uint32_t cy = 0; cy < down; ++cy) {
                        for (uint32_t cx = 0; cx < across; ++cx) {
                            const size_t index = (size_t(plane) * down + cy) * across + cx;
                            // the last strip may hold fewer rows
                            const uint32_t rows = tiled ? chh : std::min(chh, pg.height - cy * chh);
                            if (!_chunk(pg, index, buf.get(), chunk_row * rows)) {
                                return unexpected(_err);
                            }
                            if (pg.fill_order == 2 && bits < 8) {
                                for (size_t i = 0; i < chunk_row * rows; ++i) {
                                    uint8_t b = buf[i];
                                    b = uint8_t((b * 0x0202020202ull & 0x010884422010ull) % 1023);   // bits reversed
                                    buf[i] = b;
                                }
                            }
                            if (pg.predictor == 2) {
                                _unpredict(buf.get(), chunk_row, rows, cw, per_chunk, bits);
                            }
                            // into the page: rows of this chunk, columns inside the page
                            const uint32_t x0 = cx * cw, y0 = cy * chh;
                            const uint32_t xn = std::min(cw, pg.width - x0);
                            for (uint32_t r = 0; r < rows && y0 + r < pg.height; ++r) {
                                const uint8_t* src = buf.get() + size_t(r) * chunk_row;
                                uint8_t* dst = page.get() + size_t(y0 + r) * page_row;
                                for (uint32_t x = 0; x < xn; ++x) {
                                    for (unsigned k = 0; k < per_chunk; ++k) {
                                        const unsigned sample = pg.planar == 2 ? plane : k;
                                        const size_t si = size_t(x) * per_chunk + k;   // the sample's index in the chunk's row
                                        uint32_t v;
                                        if (bits == 16) {
                                            v = _le ? uint32_t(src[2 * si]) | uint32_t(src[2 * si + 1]) << 8 : uint32_t(src[2 * si]) << 8 | src[2 * si + 1];
                                        } else if (bits == 8) {
                                            v = src[si];
                                        } else {
                                            const size_t bit = si * bits;
                                            v = (src[bit / 8] >> (8 - bits - bit % 8)) & ((1u << bits) - 1);
                                        }
                                        const size_t di = (size_t(x0 + x) * pg.samples + sample);
                                        if (wide16) {
                                            const uint16_t sv = uint16_t(v);
                                            std::memcpy(dst + 2 * di, &sv, 2);
                                        } else {
                                            dst[di] = uint8_t(v);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                // the samples to the image's pixels
                const pixel_format f = _o.want ? *_o.want : native;
                image out(pg.width, pg.height, native);
                auto& s = ImageAccess::state(out);
                const unsigned maxv = (1u << bits) - 1;
                for (uint32_t y = 0; y < pg.height; ++y) {
                    const uint8_t* src = page.get() + size_t(y) * page_row;
                    uint8_t* d8 = reinterpret_cast<uint8_t*>(s.pixels.data()) + size_t(y) * s.stride;
                    for (uint32_t x = 0; x < pg.width; ++x) {
                        auto sample = [&](unsigned k) -> uint32_t {
                            const size_t i = size_t(x) * pg.samples + k;
                            if (wide16) {
                                uint16_t v;
                                std::memcpy(&v, src + 2 * i, 2);
                                return v;
                            }
                            return src[i];
                        };
                        if (ph == 3) {
                            const uint32_t i = sample(0);
                            const size_t n = size_t(1) << bits;
                            d8[3 * x] = narrow(pg.colormap[i]);
                            d8[3 * x + 1] = narrow(pg.colormap[n + i]);
                            d8[3 * x + 2] = narrow(pg.colormap[2 * n + i]);
                            continue;
                        }
                        uint32_t c[5];
                        for (unsigned k = 0; k < used; ++k) {
                            c[k] = sample(k);
                        }
                        if (ph <= 1) {
                            if (bits < 8) {
                                c[0] = (c[0] * 255 + maxv / 2) / maxv;   // to 8 bits
                            }
                            if (ph == 0) {
                                c[0] = (wide16 ? 65535u : 255u) - c[0];
                            }
                        }
                        if (alpha && pg.extra == 1 && ph != 5) {
                            // associated alpha: the color divided by it
                            const uint32_t a = c[color], top = wide16 ? 65535u : 255u;
                            for (unsigned k = 0; k < color; ++k) {
                                c[k] = a ? std::min<uint32_t>(top, (c[k] * top + a / 2) / a) : 0;
                            }
                        }
                        if (wide16) {
                            for (unsigned k = 0; k < used; ++k) {
                                const uint16_t v = uint16_t(c[k]);
                                std::memcpy(d8 + (size_t(x) * used + k) * 2, &v, 2);
                            }
                        } else if (ph == 5 && bits == 16) {
                            for (unsigned k = 0; k < 4; ++k) {
                                d8[4 * x + k] = narrow(uint16_t(c[k]));
                            }
                        } else {
                            for (unsigned k = 0; k < used; ++k) {
                                d8[size_t(x) * used + k] = uint8_t(c[k]);
                            }
                        }
                    }
                }
                _metadata(out, pg);
                if (f != native) {
                    return out.convert(f);
                }
                return out;
            }

            // The horizontal predictor (13): each sample the difference from
            // the one of its channel to the left, 8 and 16 bits
            void _unpredict(uint8_t* buf, size_t row_bytes, uint32_t rows, uint32_t width, unsigned per, unsigned bits) const noexcept {
                for (uint32_t r = 0; r < rows; ++r) {
                    uint8_t* p = buf + size_t(r) * row_bytes;
                    if (bits == 8) {
                        for (size_t i = per; i < size_t(width) * per; ++i) {
                            p[i] = uint8_t(p[i] + p[i - per]);
                        }
                    } else if (bits == 16) {
                        auto get = [&](size_t i) { return _le ? unsigned(p[2 * i]) | unsigned(p[2 * i + 1]) << 8 : unsigned(p[2 * i]) << 8 | p[2 * i + 1]; };
                        auto set = [&](size_t i, unsigned v) {
                            if (_le) {
                                p[2 * i] = uint8_t(v);
                                p[2 * i + 1] = uint8_t(v >> 8);
                            } else {
                                p[2 * i] = uint8_t(v >> 8);
                                p[2 * i + 1] = uint8_t(v);
                            }
                        };
                        for (size_t i = per; i < size_t(width) * per; ++i) {
                            set(i, (get(i) + get(i - per)) & 0xffff);
                        }
                    }
                }
            }

            void _metadata(image& out, const TiffPage& pg) noexcept {
                if (!_o.metadata) {
                    return;
                }
                ImageAccess::set_orientation(out, pg.orientation);
                if (!pg.icc.empty()) {
                    out.set_icc(slice<const byte>(reinterpret_cast<const byte*>(pg.icc.data()), pg.icc.size()));
                }
            }

            // A page of JPEG (7): each strip or tile a JPEG stream after the
            // tables of JPEGTables (their SOI and EOI dropped), decoded by
            // the module's JPEG decoder and put in its place
            expected<image, error> _jpeg_page(const TiffPage& pg) noexcept {
                const bool tiled = pg.tile_width && pg.tile_height;
                const uint32_t cw = tiled ? pg.tile_width : pg.width;
                const uint32_t chh = tiled ? pg.tile_height : std::min(pg.rows_per_strip ? pg.rows_per_strip : pg.height, pg.height);
                const uint32_t across = (pg.width + cw - 1) / cw, down = (pg.height + chh - 1) / chh;
                if (pg.planar == 2 || pg.offsets.size() < uint64_t(across) * down || pg.counts.size() < uint64_t(across) * down) {
                    return unexpected(error(errc::unsupported, 0, "tiff: JPEG of planar samples, or fewer strips than the page has"));
                }
                const pixel_format native = pg.samples == 1 ? pixel_format::gray8 : pg.samples == 4 ? pixel_format::cmyk8 : pixel_format::rgb8;
                image out(pg.width, pg.height, native);
                auto& s = ImageAccess::state(out);
                const size_t bpp = bytes_per_pixel(native);
                std::vector<uint8_t> stream;
                for (uint32_t cy = 0; cy < down; ++cy) {
                    for (uint32_t cx = 0; cx < across; ++cx) {
                        const size_t index = size_t(cy) * across + cx;
                        const uint64_t off = pg.offsets[index], cnt = pg.counts[index];
                        if (off > _n || cnt > _n - off || cnt < 4) {
                            return unexpected(error(errc::corrupt, off, "tiff: a JPEG strip or tile outside the file"));
                        }
                        stream.clear();
                        if (pg.jpeg_tables.size() > 4) {
                            stream.insert(stream.end(), pg.jpeg_tables.begin(), pg.jpeg_tables.end() - 2);   // without EOI
                            stream.insert(stream.end(), _p + off + 2, _p + off + cnt);                       // without SOI
                        } else {
                            stream.insert(stream.end(), _p + off, _p + off + cnt);
                        }
                        decode_options jo;
                        jo.limits = _o.limits;
                        jo.metadata = false;
                        jo.want = native;
                        auto part = jpeg::decode(slice<const byte>(reinterpret_cast<const byte*>(stream.data()), stream.size()), jo);
                        if (!part) {
                            return unexpected(error(part.error().code(), off, string(std::string("tiff: a JPEG strip or tile: ") + std::string(part.error().message().view()))));
                        }
                        const uint32_t x0 = cx * cw, y0 = cy * chh;
                        const uint32_t xn = std::min({cw, pg.width - x0, part->width()});
                        const uint32_t yn = std::min({chh, pg.height - y0, part->height()});
                        for (uint32_t r = 0; r < yn; ++r) {
                            sgcl::detail::copy_bytes(s.pixels.data() + size_t(y0 + r) * s.stride + size_t(x0) * bpp, part->row(r).data(), size_t(xn) * bpp);
                        }
                    }
                }
                _metadata(out, pg);
                if (_o.want && *_o.want != native) {
                    return out.convert(*_o.want);
                }
                return out;
            }

            const uint8_t* _p;
            size_t _n;
            const decode_options& _o;
            bool _le = true;
            uint64_t _next = 0;
            unsigned _pages = 0;
            std::vector<uint64_t> _visited;
            error _err;
        };

        // The TIFF writer: one page or several, little-endian, chunky,
        // strips of about 64 KB; gray (BlackIsZero), gray with alpha, RGB,
        // RGBA (unassociated alpha) at 8 or 16 bits, CMYK at 8; the
        // compression asked (LZW and Deflate with the horizontal
        // predictor); the orientation and the ICC profile of each image
        template<class Sink>
        bool tiff_encode(const slice<const image>& pages, const TiffOptions& o, Sink& sink) noexcept(NothrowSink<Sink>) {
            if (pages.size() == 0) {
                sink.failure = error(errc::invalid_argument, 0, "tiff: no page");
                return false;
            }
            if (o.compression != TiffCompression::none && o.compression != TiffCompression::lzw && o.compression != TiffCompression::deflate) {
                sink.failure = error(errc::invalid_argument, 0, "tiff: options.compression outside the list");
                return false;
            }
            std::vector<uint8_t> file = {'I', 'I', 42, 0, 0, 0, 0, 0};
            auto put16 = [](std::vector<uint8_t>& b, uint32_t v) {
                b.push_back(uint8_t(v));
                b.push_back(uint8_t(v >> 8));
            };
            auto put32 = [&](std::vector<uint8_t>& b, uint32_t v) {
                put16(b, v & 0xffff);
                put16(b, v >> 16);
            };
            auto set32 = [](std::vector<uint8_t>& b, size_t at, uint32_t v) {
                for (int k = 0; k < 4; ++k) {
                    b[at + size_t(k)] = uint8_t(v >> (8 * k));
                }
            };
            size_t link = 4;   // where the offset of the next IFD goes
            for (const image& im : pages) {
                const auto& s = ImageAccess::state(im);
                pixel_format target = s.format;
                unsigned photometric = 1, samples = 1, bits = wide(s.format) ? 16 : 8, extra = 0;
                switch (s.format) {
                    case pixel_format::gray8: case pixel_format::gray16: break;
                    case pixel_format::gray_alpha8: case pixel_format::gray_alpha16: samples = 2; extra = 2; break;
                    case pixel_format::rgb8: case pixel_format::rgb16: photometric = 2; samples = 3; break;
                    case pixel_format::rgba8: case pixel_format::rgba16: photometric = 2; samples = 4; extra = 2; break;
                    default: photometric = 5; samples = 4; bits = 8; target = pixel_format::cmyk8; break;
                }
                const uint32_t w = s.width, h = s.height;
                const size_t row = size_t(w) * samples * (bits / 8);
                const uint32_t rows_per_strip = uint32_t(std::max<size_t>(1, std::min<size_t>(h, 65536 / std::max<size_t>(row, 1))));
                const uint32_t strips = (h + rows_per_strip - 1) / rows_per_strip;
                std::vector<uint32_t> offsets, counts;
                std::vector<uint8_t> chunk, raw(row * rows_per_strip);
                const uint8_t* pixels = reinterpret_cast<const uint8_t*>(s.pixels.data());
                for (uint32_t k = 0; k < strips; ++k) {
                    const uint32_t y0 = k * rows_per_strip, rows = std::min(rows_per_strip, h - y0);
                    for (uint32_t r = 0; r < rows; ++r) {
                        const uint8_t* src = pixels + size_t(y0 + r) * s.stride;
                        uint8_t* d = raw.data() + size_t(r) * row;
                        if (bits == 16) {
                            for (size_t i = 0; i < row; i += 2) {
                                uint16_t v;
                                std::memcpy(&v, src + i, 2);
                                d[i] = uint8_t(v);
                                d[i + 1] = uint8_t(v >> 8);
                            }
                        } else {
                            sgcl::detail::copy_bytes(d, src, row);
                        }
                        if (o.compression != TiffCompression::none) {
                            // the horizontal predictor, from the right
                            if (bits == 8) {
                                for (size_t i = row; i-- > samples;) {
                                    d[i] = uint8_t(d[i] - d[i - samples]);
                                }
                            } else {
                                for (size_t i = row / 2; i-- > samples;) {
                                    const unsigned v = (unsigned(d[2 * i]) | unsigned(d[2 * i + 1]) << 8) - (unsigned(d[2 * (i - samples)]) | unsigned(d[2 * (i - samples) + 1]) << 8);
                                    d[2 * i] = uint8_t(v);
                                    d[2 * i + 1] = uint8_t(v >> 8);
                                }
                            }
                        }
                    }
                    chunk.clear();
                    const size_t n = row * rows;
                    if (o.compression == TiffCompression::lzw) {
                        tiff_codec::LzwWriter lzw;
                        lzw.write(raw.data(), n, chunk);
                        lzw.finish(chunk);
                    } else if (o.compression == TiffCompression::deflate) {
                        auto z = compress::zlib::compress(slice<const byte>(reinterpret_cast<const byte*>(raw.data()), n));
                        const auto* b = reinterpret_cast<const uint8_t*>(z.data());
                        chunk.assign(b, b + z.size());
                    } else {
                        chunk.assign(raw.data(), raw.data() + n);
                    }
                    if (file.size() & 1) {
                        file.push_back(0);
                    }
                    offsets.push_back(uint32_t(file.size()));
                    counts.push_back(uint32_t(chunk.size()));
                    file.insert(file.end(), chunk.begin(), chunk.end());
                    if (file.size() > 0xFFFFFFF0u) {
                        sink.failure = error(errc::invalid_argument, 0, "tiff: a file past 4 GB, more than TIFF's offsets hold");
                        return false;
                    }
                }
                // the values that do not fit an entry, then the IFD
                auto out_of_line = [&](const std::vector<uint8_t>& bytes) {
                    if (file.size() & 1) {
                        file.push_back(0);
                    }
                    const uint32_t at = uint32_t(file.size());
                    file.insert(file.end(), bytes.begin(), bytes.end());
                    return at;
                };
                std::vector<uint8_t> tmp;
                uint32_t bits_at = 0;
                if (samples > 2) {
                    tmp.clear();
                    for (unsigned i = 0; i < samples; ++i) {
                        put16(tmp, bits);
                    }
                    bits_at = out_of_line(tmp);
                }
                tmp.clear();
                for (uint32_t v : offsets) {
                    put32(tmp, v);
                }
                const uint32_t offsets_at = strips > 1 ? out_of_line(tmp) : offsets[0];
                tmp.clear();
                for (uint32_t v : counts) {
                    put32(tmp, v);
                }
                const uint32_t counts_at = strips > 1 ? out_of_line(tmp) : counts[0];
                const bool icc = !s.icc.empty() && s.icc.size() <= 0x7FFFFFFF;
                uint32_t icc_at = 0;
                if (icc) {
                    const auto* b = reinterpret_cast<const uint8_t*>(s.icc.data());
                    icc_at = out_of_line(std::vector<uint8_t>(b, b + s.icc.size()));
                }
                if (file.size() & 1) {
                    file.push_back(0);
                }
                const uint32_t ifd = uint32_t(file.size());
                set32(file, link, ifd);
                struct Entry {
                    uint16_t tag, type;
                    uint32_t count, value;
                };
                std::vector<Entry> e;
                e.push_back({256, 4, 1, w});
                e.push_back({257, 4, 1, h});
                e.push_back({258, 3, samples, samples > 2 ? bits_at : (samples == 2 ? (bits | bits << 16) : bits)});
                e.push_back({259, 3, 1, o.compression == TiffCompression::lzw ? 5u : o.compression == TiffCompression::deflate ? 8u : 1u});
                e.push_back({262, 3, 1, photometric});
                e.push_back({273, 4, strips, offsets_at});
                e.push_back({274, 3, 1, s.orientation});
                e.push_back({277, 3, 1, samples});
                e.push_back({278, 4, 1, rows_per_strip});
                e.push_back({279, 4, strips, counts_at});
                e.push_back({284, 3, 1, 1});
                if (o.compression != TiffCompression::none) {
                    e.push_back({317, 3, 1, 2});
                }
                if (extra) {
                    e.push_back({338, 3, 1, extra});
                }
                if (icc) {
                    e.push_back({34675, 7, uint32_t(s.icc.size()), icc_at});
                }
                put16(file, uint32_t(e.size()));
                for (const Entry& x : e) {
                    put16(file, x.tag);
                    put16(file, x.type);
                    put32(file, x.count);
                    if (x.type == 3 && x.count == 1) {
                        put16(file, x.value);
                        put16(file, 0);
                    } else {
                        put32(file, x.value);
                    }
                }
                link = file.size();
                put32(file, 0);
                (void)target;
            }
            return sink.put(file.data(), file.size());
        }
    }

    // TIFF: baseline and the common extensions read (detail::TiffReader
    // says which), every page through decode_all; written uncompressed,
    // with LZW (the default) or Deflate, one page or several. A stream is
    // read to its end first (TIFF's directories point anywhere in the
    // file); a file is made in memory and written at once.
    class tiff {
    public:
        using compression = detail::TiffCompression;
        using options = detail::TiffOptions;

        // The first page
        static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {}) noexcept {
            if (o.want && !detail::valid(*o.want)) {
                return unexpected(error(errc::invalid_argument, 0, "tiff: decode_options.want outside the list"));
            }
            detail::TiffReader r(data, o);
            if (!r.start()) {
                return unexpected(r.failure());
            }
            if (!r.more()) {
                return unexpected(error(errc::corrupt, 4, "tiff: no page"));
            }
            return r.next();
        }

        static expected<image, error> decode(const io::reader& in, const decode_options& o = {}) {
            vector<byte> all;
            if (auto e = _read_all(in, o, all); !e) {
                return unexpected(e.error());
            }
            return decode(all.as_slice(), o);
        }

        // Every page, in the file's order
        static expected<vector<image>, error> decode_all(const slice<const byte>& data, const decode_options& o = {}) noexcept {
            if (o.want && !detail::valid(*o.want)) {
                return unexpected(error(errc::invalid_argument, 0, "tiff: decode_options.want outside the list"));
            }
            detail::TiffReader r(data, o);
            if (!r.start()) {
                return unexpected(r.failure());
            }
            vector<image> out;
            while (r.more()) {
                auto im = r.next();
                if (!im) {
                    return unexpected(im.error());
                }
                out.push_back(*im);
            }
            if (out.empty()) {
                return unexpected(error(errc::corrupt, 4, "tiff: no page"));
            }
            return out;
        }

        static expected<vector<image>, error> decode_all(const io::reader& in, const decode_options& o = {}) {
            vector<byte> all;
            if (auto e = _read_all(in, o, all); !e) {
                return unexpected(e.error());
            }
            return decode_all(all.as_slice(), o);
        }

        // One page, or several, as bytes: errc::invalid_argument for no
        // page, a compression outside the list or a file past 4 GB
        static expected<vector<byte>, error> encode(const image& im, const options& o = {}) noexcept {
            return encode(slice<const image>(&im, 1), o);
        }

        static expected<vector<byte>, error> encode(const slice<const image>& pages, const options& o = {}) noexcept {
            vector<byte> out;
            detail::VectorSink sink{out, nullopt};
            if (!detail::tiff_encode(pages, o, sink)) {
                return unexpected(*sink.failure);
            }
            return out;
        }

        static expected<void, error> encode(const image& im, const io::writer& out, const options& o = {}) {
            return encode(slice<const image>(&im, 1), out, o);
        }

        static expected<void, error> encode(const slice<const image>& pages, const io::writer& out, const options& o = {}) {
            detail::WriterSink sink{out, 0, nullopt};
            if (!detail::tiff_encode(pages, o, sink)) {
                return unexpected(*sink.failure);
            }
            return {};
        }

    private:
        // A stream to its end: at most 8 bytes a pixel of limits.max_pixels
        // and 64 MB more, errc::too_large past it
        static expected<void, error> _read_all(const io::reader& in, const decode_options& o, vector<byte>& all) {
            detail::ReaderInput source(in);
            const uint64_t most = o.limits.max_pixels * 8 + (uint64_t(64) << 20);
            for (;;) {
                const uint8_t* p;
                size_t got;
                if (!source.peek(65536, p, got)) {
                    return unexpected(*source.failure);
                }
                if (got == 0) {
                    return {};
                }
                if (all.size() + got > most) {
                    return unexpected(error(errc::too_large, all.size()));
                }
                all.insert(all.end(), reinterpret_cast<const byte*>(p), reinterpret_cast<const byte*>(p) + got);
                source.consume(got);
            }
        }
    };
}
