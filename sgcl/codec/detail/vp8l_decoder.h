//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "input.h"
#include "vp8l_simd.h"
#include "../error.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>

namespace sgcl::codec::detail {
    // The bits of a VP8L bitstream (RFC 9649 §3), least significant first,
    // read from an input of MemoryInput's shape: `size` bytes from where it
    // stands, never past them. Memory is read in place; a stream through
    // its input's block, no copy of the chunk. Bits past the chunk read as
    // zeros and are counted; overrun() says whether any was used (the data
    // ended before the image did).
    template<class Input>
    class Vp8lBits {
    public:
        Vp8lBits(Input& in, uint64_t size) noexcept
        : _in(in), _left(size) {
        }

        // At least 32 bits held (zeros past the end)
        void fill() {
            if (_nbits >= 32) {
                return;
            }
            if (_e - _p >= 8) {
                uint64_t v;
                std::memcpy(&v, _p, 8);
                _val |= v << _nbits;
                const unsigned take = (63 - _nbits) >> 3;
                _p += take;
                _nbits += take * 8;
                _val &= (uint64_t(1) << _nbits) - 1;   // the bytes past them are loaded again
                return;
            }
            _fill_slow();
        }

        uint32_t bits(unsigned n) {
            fill();
            const uint32_t r = uint32_t(_val) & ((uint32_t(1) << n) - 1);
            _val >>= n;
            _nbits -= n;
            return r;
        }

        // The low bits held, for a table lookup; skip() takes them
        uint64_t peek() const noexcept {
            return _val;
        }

        void skip(unsigned n) noexcept {
            _val >>= n;
            _nbits -= n;
        }

        bool overrun() const noexcept {
            return _padded > _nbits;
        }

        // The input failed (a stream's error), or ended before the chunk
        bool failed() const noexcept {
            return _failed;
        }

        bool truncated() const noexcept {
            return _truncated;
        }

        // Done with the chunk: its bytes not read yet taken from the input
        // (the bits held and not used are dropped); false when the input
        // failed or ended on the way
        bool finish() {
            _in.consume(size_t(_p - _span));
            _span = _p = _e;
            while (_left) {
                const uint8_t* q;
                size_t got;
                if (!_in.peek(size_t(std::min<uint64_t>(_left, uint64_t(1) << 30)), q, got)) {
                    _failed = true;
                    return false;
                }
                if (!got) {
                    _truncated = true;
                    return false;
                }
                _in.consume(got);
                _left -= got;
            }
            return true;
        }

    private:
        void _fill_slow() {
            while (_nbits <= 56) {
                if (_p < _e) {
                    _val |= uint64_t(*_p++) << _nbits;
                    _nbits += 8;
                    continue;
                }
                if (_left && !_failed && !_truncated) {
                    _in.consume(size_t(_e - _span));
                    const uint8_t* q;
                    size_t got;
                    if (!_in.peek(size_t(std::min<uint64_t>(_left, uint64_t(1) << 30)), q, got)) {
                        _failed = true;
                    } else if (!got) {
                        _truncated = true;
                    } else {
                        _span = _p = q;
                        _e = q + got;
                        _left -= got;
                        continue;
                    }
                    _span = _p = _e;
                }
                // past the chunk: zeros, counted
                _nbits += 8;
                _padded += 8;
            }
        }

        Input& _in;
        uint64_t _val = 0;
        unsigned _nbits = 0;
        unsigned _padded = 0;
        const uint8_t* _span = nullptr;   // the bytes peeked and not consumed yet
        const uint8_t* _p = nullptr;
        const uint8_t* _e = nullptr;
        uint64_t _left;                   // the chunk's bytes not peeked yet
        bool _failed = false;
        bool _truncated = false;
    };

    // A prefix code of VP8L as a table of two levels: 256 entries for the
    // code's first 8 bits, and for longer codes a table of the rest under
    // each first-8-bit prefix that has them. An entry: the symbol (bits
    // 0-15), the bits taken at its level (16-23), and bit 24 when it points
    // to a table of the second level (the symbol then its offset, the bits
    // the width of its index). A code of one symbol takes no bits.
    namespace vp8l {
        inline constexpr uint32_t Pointer = uint32_t(1) << 24;
        inline constexpr unsigned MaxLength = 15;

        inline uint32_t entry(uint32_t value, unsigned len) noexcept {
            return value | uint32_t(len) << 16;
        }

        // The canonical code of the lengths (0: no symbol), appended to
        // `table`; false when the lengths are not a whole tree (one symbol
        // is whole) or name no symbol
        inline bool build(const uint8_t* lengths, unsigned n, std::vector<uint32_t>& table, uint32_t& offset) {
            unsigned count[MaxLength + 1] = {};
            unsigned symbols = 0;
            unsigned last = 0;
            for (unsigned s = 0; s < n; ++s) {
                if (lengths[s]) {
                    ++count[lengths[s]];
                    ++symbols;
                    last = s;
                }
            }
            if (symbols == 0) {
                return false;
            }
            offset = uint32_t(table.size());
            if (symbols == 1) {
                table.resize(table.size() + 256, entry(last, 0));
                return true;
            }
            uint32_t kraft = 0;
            for (unsigned l = 1; l <= MaxLength; ++l) {
                kraft += count[l] << (MaxLength - l);
            }
            if (kraft != uint32_t(1) << MaxLength) {
                return false;
            }
            uint32_t next[MaxLength + 2];
            uint32_t code = 0;
            count[0] = 0;
            for (unsigned l = 1; l <= MaxLength; ++l) {
                code = (code + count[l - 1]) << 1;
                next[l] = code;
            }
            auto reverse = [](uint32_t c, unsigned len) {
                uint32_t r = 0;
                for (unsigned i = 0; i < len; ++i) {
                    r = r << 1 | (c & 1);
                    c >>= 1;
                }
                return r;
            };
            // the longest code under each first-8-bit prefix, for the size
            // of its second table
            uint8_t longest[256] = {};
            uint32_t codes[2328];
            for (unsigned s = 0; s < n; ++s) {
                const unsigned len = lengths[s];
                if (len) {
                    codes[s] = reverse(next[len]++, len);
                    if (len > 8) {
                        uint8_t& m = longest[codes[s] & 0xff];
                        m = std::max<uint8_t>(m, uint8_t(len));
                    }
                }
            }
            const size_t root = table.size();
            table.resize(root + 256, 0);
            uint32_t sub[256];
            for (unsigned i = 0; i < 256; ++i) {
                if (longest[i]) {
                    const unsigned width = longest[i] - 8u;
                    sub[i] = uint32_t(table.size() - root);
                    table[root + i] = Pointer | entry(sub[i], width);
                    table.resize(table.size() + (size_t(1) << width), 0);
                }
            }
            for (unsigned s = 0; s < n; ++s) {
                const unsigned len = lengths[s];
                if (!len) {
                    continue;
                }
                const uint32_t c = codes[s];
                if (len <= 8) {
                    for (uint32_t i = c; i < 256; i += uint32_t(1) << len) {
                        table[root + i] = entry(s, len);
                    }
                } else {
                    const unsigned width = longest[c & 0xff] - 8u;
                    const size_t base = root + sub[c & 0xff];
                    for (uint32_t i = c >> 8; i < (uint32_t(1) << width); i += uint32_t(1) << (len - 8)) {
                        table[base + i] = entry(s, len - 8);
                    }
                }
            }
            return true;
        }

        // The symbol at the bits held (at least 15 of them)
        template<class Bits>
        inline uint32_t read_symbol(const uint32_t* t, Bits& b) noexcept {
            const uint64_t v = b.peek();
            uint32_t e = t[v & 0xff];
            if (e & Pointer) {
                const unsigned width = (e >> 16) & 0xff;
                e = t[(e & 0xffff) + ((v >> 8) & ((uint32_t(1) << width) - 1))];
                b.skip(8 + ((e >> 16) & 0xff));
            } else {
                b.skip((e >> 16) & 0xff);
            }
            return e & 0xffff;
        }

        // The (xi, yi) of distance codes 1 to 120 (RFC 9649 §3.6.2.2.1):
        // xi to the left, yi up
        inline constexpr int8_t DistanceMap[120][2] = {
            {0, 1},  {1, 0},  {1, 1},  {-1, 1}, {0, 2},  {2, 0},  {1, 2},  {-1, 2}, {2, 1},  {-2, 1}, {2, 2},  {-2, 2}, {0, 3},
            {3, 0},  {1, 3},  {-1, 3}, {3, 1},  {-3, 1}, {2, 3},  {-2, 3}, {3, 2},  {-3, 2}, {0, 4},  {4, 0},  {1, 4},  {-1, 4},
            {4, 1},  {-4, 1}, {3, 3},  {-3, 3}, {2, 4},  {-2, 4}, {4, 2},  {-4, 2}, {0, 5},  {3, 4},  {-3, 4}, {4, 3},  {-4, 3},
            {5, 0},  {1, 5},  {-1, 5}, {5, 1},  {-5, 1}, {2, 5},  {-2, 5}, {5, 2},  {-5, 2}, {4, 4},  {-4, 4}, {3, 5},  {-3, 5},
            {5, 3},  {-5, 3}, {0, 6},  {6, 0},  {1, 6},  {-1, 6}, {6, 1},  {-6, 1}, {2, 6},  {-2, 6}, {6, 2},  {-6, 2}, {4, 5},
            {-4, 5}, {5, 4},  {-5, 4}, {3, 6},  {-3, 6}, {6, 3},  {-6, 3}, {0, 7},  {7, 0},  {1, 7},  {-1, 7}, {5, 5},  {-5, 5},
            {7, 1},  {-7, 1}, {4, 6},  {-4, 6}, {6, 4},  {-6, 4}, {2, 7},  {-2, 7}, {7, 2},  {-7, 2}, {3, 7},  {-3, 7}, {7, 3},
            {-7, 3}, {5, 6},  {-5, 6}, {6, 5},  {-6, 5}, {8, 0},  {4, 7},  {-4, 7}, {7, 4},  {-7, 4}, {8, 1},  {8, 2},  {6, 6},
            {-6, 6}, {8, 3},  {5, 7},  {-5, 7}, {7, 5},  {-7, 5}, {8, 4},  {6, 7},  {-6, 7}, {7, 6},  {-7, 6}, {8, 5},  {7, 7},
            {-7, 7}, {8, 6},  {8, 7}};

        inline constexpr uint8_t CodeLengthOrder[19] = {17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

        inline uint32_t div_round_up(uint32_t n, unsigned bits) noexcept {
            return (n + (uint32_t(1) << bits) - 1) >> bits;
        }
    }

    // The header of a VP8L bitstream: the signature 0x2F, the size (14 bits
    // each, minus one), alpha_is_used, the version (0). Read from its first
    // 5 bytes; false when they are not one
    struct Vp8lHeader {
        uint32_t width = 0;
        uint32_t height = 0;
        bool alpha = false;

        bool parse(const uint8_t* p) noexcept {
            if (p[0] != 0x2F) {
                return false;
            }
            const uint32_t v = uint32_t(p[1]) | uint32_t(p[2]) << 8 | uint32_t(p[3]) << 16 | uint32_t(p[4]) << 24;
            width = (v & 0x3FFF) + 1;
            height = ((v >> 14) & 0x3FFF) + 1;
            alpha = (v >> 28) & 1;
            return (v >> 29) == 0;
        }
    };

    // The decoder of a VP8L image (RFC 9649 §3): the transforms, the
    // entropy-coded image (prefix codes, meta prefix codes, LZ77 back
    // references, the color cache), the inverse transforms last read first.
    // The pixels come out as ARGB words, in pixels(). Its buffers (the
    // pixels, the codes' tables, the transforms' images) are unmanaged and
    // kept from one image to the next (the frames of an animation).
    class Vp8lDecoder {
    public:
        // The image of a VP8L chunk of `size` bytes whose header (5 bytes)
        // is read already, the input standing after it: false with the
        // error in `err`
        template<class Input>
        bool decode(Input& in, uint64_t size, uint32_t width, uint32_t height, optional<error>& err, uint64_t at) {
            Vp8lBits<Input> b(in, size);
            _width = width;
            _height = height;
            _at = at;
            bool ok = _image(b, err);
            if (ok) {
                ok = b.finish();
            }
            if (!ok) {
                if (b.failed()) {
                    err = *in.failure;
                } else if (b.truncated() || b.overrun()) {
                    err = error(errc::unexpected_end, in.offset(), "webp: the VP8L data ends before the image");
                }
                return false;
            }
            return true;
        }

        // The pixels of the last image, width × height ARGB words
        const uint32_t* pixels() const noexcept {
            return _pixels.data();
        }

    private:
        struct Transform {
            unsigned type;
            unsigned bits;      // size_bits (0, 1), width_bits (3)
            uint32_t xsize;     // the width the transform was read at
            std::vector<uint32_t> data;
        };

        // A group of five codes: green (with lengths and the cache), red,
        // blue, alpha, distance; offsets into _tables
        struct Group {
            uint32_t code[5];
        };

        // An error of the bitstream, at the offset of its chunk
        template<class Bits>
        bool _fail(Bits& b, optional<error>& err, errc code, const char* what) {
            (void)b;
            err = error(code, _at, string(what));
            return false;
        }

        template<class Bits>
        bool _image(Bits& b, optional<error>& err) {
            uint32_t xsize = _width;
            unsigned seen = 0;
            _transforms_used = 0;
            while (b.bits(1)) {
                const unsigned type = b.bits(2);
                if (seen & (1u << type)) {
                    return _fail(b, err, errc::corrupt, "webp: a VP8L transform used twice");
                }
                seen |= 1u << type;
                Transform& t = _transforms[_transforms_used++];
                t.type = type;
                t.xsize = xsize;
                if (type == 0 || type == 1) {
                    t.bits = b.bits(3) + 2;
                    const uint32_t w = vp8l::div_round_up(xsize, t.bits), h = vp8l::div_round_up(_height, t.bits);
                    t.data.resize(size_t(w) * h);
                    if (!_stream(b, err, w, h, false, t.data.data())) {
                        return false;
                    }
                } else if (type == 3) {
                    const uint32_t colors = b.bits(8) + 1;
                    t.bits = colors > 16 ? 0 : colors > 4 ? 1 : colors > 2 ? 2 : 3;
                    t.data.assign(256, 0);
                    if (!_stream(b, err, colors, 1, false, t.data.data())) {
                        return false;
                    }
                    for (uint32_t i = 1; i < colors; ++i) {
                        t.data[i] = vp8l::add_pixels(t.data[i], t.data[i - 1]);
                    }
                    xsize = vp8l::div_round_up(xsize, t.bits);
                }
                if (b.overrun()) {
                    return _fail(b, err, errc::unexpected_end, "webp: the VP8L data ends in a transform");
                }
            }
            _pixels.resize(size_t(_width) * _height);
            if (!_stream(b, err, xsize, _height, true, _pixels.data())) {
                return false;
            }
            for (unsigned i = _transforms_used; i-- > 0;) {
                _inverse(_transforms[i]);
            }
            return true;
        }

        // The prefix codes of one entropy-coded image: `groups` groups, only
        // those `used` built (the others read and checked, then dropped)
        template<class Bits>
        bool _codes(Bits& b, optional<error>& err, uint32_t groups, unsigned cache_bits) {
            _tables.clear();
            _groups.clear();
            _groups.resize(groups);
            // room for every group used at once: five first-level tables and
            // some second-level ones each (one allocation for most images,
            // not one that grows with the groups; the code of code lengths
            // is built past them and dropped)
            const size_t used = _used.empty() ? groups : size_t(std::count(_used.begin(), _used.end(), uint8_t(1)));
            _tables.reserve(used * (5 * 256 + 128) + 512);
            const unsigned sizes[5] = {256 + 24 + (cache_bits ? 1u << cache_bits : 0u), 256, 256, 256, 40};
            for (uint32_t g = 0; g < groups; ++g) {
                const size_t mark = _tables.size();
                for (unsigned k = 0; k < 5; ++k) {
                    if (!_code(b, err, sizes[k], _groups[g].code[k])) {
                        return false;
                    }
                }
                if (!_used.empty() && !_used[g]) {
                    _tables.resize(mark);
                }
            }
            return true;
        }

        // One prefix code: simple (one or two symbols) or normal (its
        // lengths through the code of code lengths)
        template<class Bits>
        bool _code(Bits& b, optional<error>& err, unsigned alphabet, uint32_t& offset) {
            uint8_t lengths[256 + 24 + 2048] = {};
            if (b.bits(1)) {
                // a symbol past the alphabet (a distance's of 40..255) is
                // no symbol of the code, as libwebp reads it
                const unsigned count = b.bits(1) + 1;
                const unsigned first = b.bits(1 + 7 * b.bits(1));
                if (first < alphabet) {
                    lengths[first] = 1;
                }
                if (count == 2) {
                    const unsigned second = b.bits(8);
                    if (second < alphabet) {
                        lengths[second] = 1;
                    }
                }
            } else {
                uint8_t cl[19] = {};
                const unsigned n = 4 + b.bits(4);
                for (unsigned i = 0; i < n; ++i) {
                    cl[vp8l::CodeLengthOrder[i]] = uint8_t(b.bits(3));
                }
                const size_t mark = _tables.size();
                uint32_t cl_offset;
                if (!vp8l::build(cl, 19, _tables, cl_offset)) {
                    return _fail(b, err, errc::corrupt, "webp: a VP8L code of code lengths that is not a tree");
                }
                unsigned max_symbol = alphabet;
                if (b.bits(1)) {
                    const unsigned nbits = 2 + 2 * b.bits(3);
                    max_symbol = 2 + b.bits(nbits);
                    if (max_symbol > alphabet) {
                        return _fail(b, err, errc::corrupt, "webp: a VP8L max_symbol past its alphabet");
                    }
                }
                unsigned prev = 8;
                unsigned s = 0;
                while (s < alphabet) {
                    if (max_symbol-- == 0) {
                        break;
                    }
                    b.fill();
                    const unsigned c = vp8l::read_symbol(_tables.data() + cl_offset, b);
                    if (c < 16) {
                        lengths[s++] = uint8_t(c);
                        if (c) {
                            prev = c;
                        }
                        continue;
                    }
                    unsigned repeat;
                    uint8_t value = 0;
                    if (c == 16) {
                        repeat = 3 + b.bits(2);
                        value = uint8_t(prev);
                    } else if (c == 17) {
                        repeat = 3 + b.bits(3);
                    } else {
                        repeat = 11 + b.bits(7);
                    }
                    if (s + repeat > alphabet) {
                        return _fail(b, err, errc::corrupt, "webp: VP8L code lengths past the alphabet");
                    }
                    std::memset(lengths + s, value, repeat);
                    s += repeat;
                }
                _tables.resize(mark);
            }
            if (b.overrun()) {
                return _fail(b, err, errc::unexpected_end, "webp: the VP8L data ends in a prefix code");
            }
            if (!vp8l::build(lengths, alphabet, _tables, offset)) {
                return _fail(b, err, errc::corrupt, "webp: a VP8L prefix code that is not a tree");
            }
            return true;
        }

        // An entropy-coded image of xsize × ysize into dst: the color cache,
        // the meta prefix codes (the main image only), the codes, the pixels
        template<class Bits>
        bool _stream(Bits& b, optional<error>& err, uint32_t xsize, uint32_t ysize, bool main, uint32_t* dst) {
            unsigned cache_bits = 0;
            if (b.bits(1)) {
                cache_bits = b.bits(4);
                if (cache_bits < 1 || cache_bits > 11) {
                    return _fail(b, err, errc::corrupt, "webp: VP8L color cache bits outside 1..11");
                }
            }
            uint32_t groups = 1;
            unsigned prefix_bits = 0;
            uint32_t prefix_width = 0;
            _used.clear();
            if (main && b.bits(1)) {
                prefix_bits = b.bits(3) + 2;
                prefix_width = vp8l::div_round_up(xsize, prefix_bits);
                const uint32_t prefix_height = vp8l::div_round_up(ysize, prefix_bits);
                _entropy.resize(size_t(prefix_width) * prefix_height);
                if (!_stream(b, err, prefix_width, prefix_height, false, _entropy.data())) {
                    return false;
                }
                uint32_t most = 0;
                for (auto& e : _entropy) {
                    e = (e >> 8) & 0xffff;
                    most = std::max(most, e);
                }
                groups = most + 1;
                _used.assign(groups, 0);
                for (uint32_t e : _entropy) {
                    _used[e] = 1;
                }
            }
            if (!_codes(b, err, groups, cache_bits)) {
                return false;
            }
            _cache.assign(cache_bits ? size_t(1) << cache_bits : 0, 0);
            return _pixels_of(b, err, xsize, ysize, dst, prefix_bits, prefix_width, cache_bits);
        }

        template<class Bits>
        bool _pixels_of(Bits& b, optional<error>& err, uint32_t xsize, uint32_t ysize, uint32_t* dst, unsigned prefix_bits, uint32_t prefix_width, unsigned cache_bits) {
            const size_t total = size_t(xsize) * ysize;
            const uint32_t* t = _tables.data();
            uint32_t* cache = _cache.data();
            const unsigned cache_shift = 32 - cache_bits;
            const uint32_t mask = prefix_bits ? (uint32_t(1) << prefix_bits) - 1 : 0xffffffffu;
            auto group_at = [&](uint32_t x, uint32_t y) -> const Group& {
                return prefix_bits ? _groups[_entropy[size_t(y >> prefix_bits) * prefix_width + (x >> prefix_bits)]] : _groups[0];
            };
            const Group* g = &group_at(0, 0);
            size_t pos = 0;
            size_t cached = 0;   // the pixels before it are in the cache
            uint32_t x = 0, y = 0;
            auto to_cache = [&](size_t upto) {
                if (cache_bits) {
                    for (; cached < upto; ++cached) {
                        const uint32_t v = dst[cached];
                        cache[(0x1e35a7bdu * v) >> cache_shift] = v;
                    }
                }
            };
            while (pos < total) {
                if ((x & mask) == 0) {
                    g = &group_at(x, y);
                }
                b.fill();
                const uint32_t s = vp8l::read_symbol(t + g->code[0], b);
                if (s < 256) {
                    const uint32_t red = vp8l::read_symbol(t + g->code[1], b);
                    b.fill();
                    const uint32_t blue = vp8l::read_symbol(t + g->code[2], b);
                    const uint32_t alpha = vp8l::read_symbol(t + g->code[3], b);
                    dst[pos++] = alpha << 24 | red << 16 | s << 8 | blue;
                    if (++x == xsize) {
                        x = 0;
                        ++y;
                        if (b.overrun()) {
                            return _fail(b, err, errc::unexpected_end, "webp: the VP8L data ends before the image");
                        }
                    }
                } else if (s < 256 + 24) {
                    const uint32_t length = _prefix_value(b, s - 256);
                    b.fill();
                    const uint32_t code = vp8l::read_symbol(t + g->code[4], b);
                    uint32_t d = _prefix_value(b, code);
                    size_t distance;
                    if (d > 120) {
                        distance = d - 120;
                    } else {
                        const auto& m = vp8l::DistanceMap[d - 1];
                        const int64_t v = int64_t(m[0]) + int64_t(m[1]) * xsize;
                        distance = v < 1 ? 1 : size_t(v);
                    }
                    if (distance > pos || length > total - pos) {
                        return _fail(b, err, errc::corrupt, "webp: a VP8L back reference outside the image");
                    }
                    to_cache(pos);
                    for (uint32_t i = 0; i < length; ++i) {
                        dst[pos + i] = dst[pos + i - distance];
                    }
                    pos += length;
                    x += length;
                    while (x >= xsize) {
                        x -= xsize;
                        ++y;
                    }
                    if (b.overrun()) {
                        return _fail(b, err, errc::unexpected_end, "webp: the VP8L data ends before the image");
                    }
                    if (pos < total) {
                        g = &group_at(x, y);
                    }
                } else {
                    to_cache(pos);
                    dst[pos++] = cache[s - 280];
                    if (++x == xsize) {
                        x = 0;
                        ++y;
                        if (b.overrun()) {
                            return _fail(b, err, errc::unexpected_end, "webp: the VP8L data ends before the image");
                        }
                    }
                }
            }
            if (b.overrun()) {
                return _fail(b, err, errc::unexpected_end, "webp: the VP8L data ends before the image");
            }
            return true;
        }

        // A length or a distance from its prefix code and extra bits
        template<class Bits>
        static uint32_t _prefix_value(Bits& b, uint32_t code) {
            if (code < 4) {
                return code + 1;
            }
            const unsigned extra = (code - 2) >> 1;
            const uint32_t offset = (2 + (code & 1)) << extra;
            return offset + b.bits(extra) + 1;
        }

        void _inverse(const Transform& t) {
            uint32_t* p = _pixels.data();
            const uint32_t w = t.xsize;
            switch (t.type) {
                case 0:
                    _predict(t, p, w);
                    break;
                case 1: {
                    const uint32_t tw = vp8l::div_round_up(w, t.bits);
                    const uint32_t block = uint32_t(1) << t.bits;
                    for (uint32_t y = 0; y < _height; ++y) {
                        const uint32_t* row = t.data.data() + size_t(y >> t.bits) * tw;
                        uint32_t* line = p + size_t(y) * w;
                        for (uint32_t x = 0, k = 0; x < w; x += block, ++k) {
                            vp8l::color_transform(line + x, std::min(block, w - x), row[k]);
                        }
                    }
                    break;
                }
                case 2:
                    vp8l::add_green(p, size_t(w) * _height);
                    break;
                default:
                    _unpack(t, p);
                    break;
            }
        }

        // The predictor transform, row by row in place: the top row from
        // the left, the left column from above, the rest by the block's mode
        void _predict(const Transform& t, uint32_t* p, uint32_t w) {
            const uint32_t tw = vp8l::div_round_up(w, t.bits);
            const uint32_t block = uint32_t(1) << t.bits;
            p[0] = vp8l::add_pixels(p[0], 0xff000000u);
            for (uint32_t x = 1; x < w; ++x) {
                p[x] = vp8l::add_pixels(p[x], p[x - 1]);
            }
            for (uint32_t y = 1; y < _height; ++y) {
                uint32_t* cur = p + size_t(y) * w;
                const uint32_t* top = cur - w;
                const uint32_t* modes = t.data.data() + size_t(y >> t.bits) * tw;
                cur[0] = vp8l::add_pixels(cur[0], top[0]);
                for (uint32_t x0 = 0, k = 0; x0 < w; x0 += block, ++k) {
                    const uint32_t from = std::max<uint32_t>(x0, 1);
                    const uint32_t to = std::min(x0 + block, w);
                    if (from >= to) {
                        continue;
                    }
                    unsigned mode = (modes[k] >> 8) & 0xf;
                    if (mode >= 14) {
                        mode = 0;
                    }
                    // TR of the last pixel is the row's first pixel: the
                    // vector loop stops before it
                    const uint32_t vend = to == w ? to - 1 : to;
                    if (mode == 0 || mode == 2 || mode == 3 || mode == 4 || mode == 8 || mode == 9) {
                        if (vend > from) {
                            vp8l::predict_top(mode, cur + from, top + from, vend - from);
                        }
                        if (vend < to) {
                            _predict_one(mode, cur, top, vend, w);
                        }
                    } else {
                        // the modes that read L: a run by the mode (vp8l_simd.h)
                        if (vend > from) {
                            vp8l::predict_left_run(mode, cur + from, top + from, vend - from);
                        }
                        if (vend < to) {
                            _predict_one(mode, cur, top, vend, w);
                        }
                    }
                }
            }
        }

        static void _predict_one(unsigned mode, uint32_t* cur, const uint32_t* top, uint32_t x, uint32_t w) {
            const uint32_t L = cur[x - 1];
            const uint32_t T = top[x];
            const uint32_t TL = top[x - 1];
            const uint32_t TR = x + 1 < w ? top[x + 1] : cur[0];
            uint32_t pred;
            switch (mode) {
                case 1: pred = L; break;
                case 2: pred = T; break;
                case 3: pred = TR; break;
                case 4: pred = TL; break;
                case 5: pred = vp8l::average2(vp8l::average2(L, TR), T); break;
                case 6: pred = vp8l::average2(L, TL); break;
                case 7: pred = vp8l::average2(L, T); break;
                case 8: pred = vp8l::average2(TL, T); break;
                case 9: pred = vp8l::average2(T, TR); break;
                case 10: pred = vp8l::average2(vp8l::average2(L, TL), vp8l::average2(T, TR)); break;
                case 11: pred = vp8l::select_plain(L, T, TL); break;
                case 12: pred = vp8l::clamp_full_plain(L, T, TL); break;
                case 13: pred = vp8l::clamp_half_plain(vp8l::average2(L, T), TL); break;
                default: pred = 0xff000000u; break;
            }
            cur[x] = vp8l::add_pixels(cur[x], pred);
        }

        // The color indexing transform: each index (bundled 2, 4 or 8 to a
        // pixel's green when the table is small) replaced by its color, the
        // rows unpacked from the last, right to left, in the same buffer
        void _unpack(const Transform& t, uint32_t* p) {
            const uint32_t* table = t.data.data();
            const uint32_t packed = vp8l::div_round_up(_width, t.bits);
            if (t.bits == 0) {
                const size_t n = size_t(_width) * _height;
                for (size_t i = 0; i < n; ++i) {
                    p[i] = table[(p[i] >> 8) & 0xff];
                }
                return;
            }
            const unsigned per = 1u << t.bits;
            const unsigned width = 8 >> t.bits;
            const uint32_t m = (1u << width) - 1;
            for (uint32_t y = _height; y-- > 0;) {
                const uint32_t* src = p + size_t(y) * packed;
                uint32_t* dst = p + size_t(y) * _width;
                for (uint32_t x = _width; x-- > 0;) {
                    const uint32_t g = (src[x >> t.bits] >> 8) & 0xff;
                    dst[x] = table[(g >> ((x & (per - 1)) * width)) & m];
                }
            }
        }

        uint32_t _width = 0;
        uint32_t _height = 0;
        uint64_t _at = 0;
        Transform _transforms[4];
        unsigned _transforms_used = 0;
        std::vector<uint32_t> _pixels;
        std::vector<uint32_t> _entropy;
        std::vector<uint8_t> _used;
        std::vector<uint32_t> _tables;
        std::vector<Group> _groups;
        std::vector<uint32_t> _cache;
    };
}
