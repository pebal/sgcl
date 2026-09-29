//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// WebP files made in the tests: a VP8L writer of the plainest kind (every
// symbol of every code 8 bits long, no back references, no color cache), so
// that a test can say which transforms, which predictor modes and which
// pixels a file has, and the RIFF container around it (simple, VP8X, ANIM,
// ANMF, any chunk in any order). What the module makes of such a file is
// checked against libwebp, not against this writer.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace codec_test::webp {
    class BitWriter {
    public:
        void put(uint32_t value, unsigned n) {
            for (unsigned i = 0; i < n; ++i) {
                if (_bits == 0) {
                    _out.push_back(0);
                }
                if ((value >> i) & 1) {
                    _out.back() = char(uint8_t(_out.back()) | (1u << _bits));
                }
                _bits = (_bits + 1) & 7;
            }
        }

        // A code of prefix-code bits, its first bit the code's highest
        void put_code(uint32_t code, unsigned len) {
            for (unsigned i = len; i-- > 0;) {
                put((code >> i) & 1, 1);
            }
        }

        const std::string& bytes() const {
            return _out;
        }

    private:
        std::string _out;
        unsigned _bits = 0;
    };

    // A normal prefix code of `alphabet` symbols in which the first 256
    // are 8 bits long and the rest absent: the code of code lengths gives
    // symbol 0 and symbol 8 one bit each
    inline void put_flat_code(BitWriter& w, unsigned alphabet) {
        static const unsigned order[19] = {17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        w.put(0, 1);        // normal
        w.put(12 - 4, 4);   // 12 code lengths: up to symbol 8 in the order
        for (unsigned i = 0; i < 12; ++i) {
            w.put(order[i] == 0 || order[i] == 8 ? 1 : 0, 3);
        }
        w.put(0, 1);        // max_symbol: the alphabet
        for (unsigned s = 0; s < alphabet; ++s) {
            w.put_code(s < 256 ? 1 : 0, 1);   // canonical: 0 is symbol 0, 1 symbol 8
        }
    }

    // A simple code of one symbol (no bits read)
    inline void put_single(BitWriter& w, unsigned symbol) {
        w.put(1, 1);
        w.put(0, 1);   // one symbol
        w.put(1, 1);   // 8 bits
        w.put(symbol, 8);
    }

    // An entropy-coded image of ARGB words: no color cache, no meta codes
    // (for the main image the bit saying so), the five codes, the literals
    inline void put_image(BitWriter& w, const std::vector<uint32_t>& argb, bool main) {
        w.put(0, 1);   // no color cache
        if (main) {
            w.put(0, 1);   // no meta prefix codes
        }
        put_flat_code(w, 256 + 24);
        put_flat_code(w, 256);
        put_flat_code(w, 256);
        put_flat_code(w, 256);
        put_single(w, 0);   // distance: no back reference
        for (uint32_t v : argb) {
            w.put_code((v >> 8) & 0xff, 8);
            w.put_code((v >> 16) & 0xff, 8);
            w.put_code(v & 0xff, 8);
            w.put_code(v >> 24, 8);
        }
    }

    struct Transform {
        unsigned type;                   // 0 predictor, 1 color, 2 subtract green, 3 color indexing
        unsigned bits = 2;               // size_bits for 0 and 1
        std::vector<uint32_t> data;      // the sub-image, or the palette (not delta-coded: done here)
    };

    // A VP8L bitstream (the chunk's payload) of width × height whose main
    // image is `argb` (the residuals, after the transforms as the file
    // gives them)
    inline std::string vp8l(uint32_t width, uint32_t height, const std::vector<uint32_t>& argb, const std::vector<Transform>& transforms = {},
                            bool alpha = true) {
        BitWriter w;
        w.put(0x2F, 8);
        w.put(width - 1, 14);
        w.put(height - 1, 14);
        w.put(alpha ? 1 : 0, 1);
        w.put(0, 3);
        for (const auto& t : transforms) {
            w.put(1, 1);
            w.put(t.type, 2);
            if (t.type == 0 || t.type == 1) {
                w.put(t.bits - 2, 3);
                put_image(w, t.data, false);
            } else if (t.type == 3) {
                w.put(uint32_t(t.data.size() - 1), 8);
                std::vector<uint32_t> delta(t.data.size());
                for (size_t i = 0; i < t.data.size(); ++i) {
                    const uint32_t prev = i ? t.data[i - 1] : 0;
                    uint32_t d = 0;
                    for (unsigned s = 0; s < 32; s += 8) {
                        d |= (((t.data[i] >> s) - (prev >> s)) & 0xff) << s;
                    }
                    delta[i] = d;
                }
                put_image(w, delta, false);
            }
        }
        w.put(0, 1);
        put_image(w, argb, true);
        return w.bytes();
    }

    inline std::string le32(uint32_t v) {
        return std::string{char(v), char(v >> 8), char(v >> 16), char(v >> 24)};
    }

    inline std::string le24(uint32_t v) {
        return std::string{char(v), char(v >> 8), char(v >> 16)};
    }

    // A chunk: its tag, size and payload, a byte of padding when odd
    inline std::string chunk(const std::string& tag, const std::string& payload) {
        std::string c = tag + le32(uint32_t(payload.size())) + payload;
        if (payload.size() & 1) {
            c += '\0';
        }
        return c;
    }

    inline std::string riff(const std::string& chunks) {
        return "RIFF" + le32(uint32_t(4 + chunks.size())) + "WEBP" + chunks;
    }

    // VP8X: its flags (0x02 animation, 0x04 XMP, 0x08 EXIF, 0x10 alpha,
    // 0x20 ICC) and the canvas
    inline std::string vp8x(uint8_t flags, uint32_t width, uint32_t height) {
        return chunk("VP8X", std::string{char(flags), 0, 0, 0} + le24(width - 1) + le24(height - 1));
    }

    inline std::string anim(uint32_t loops, uint32_t background = 0) {
        return chunk("ANIM", le32(background) + std::string{char(loops), char(loops >> 8)});
    }

    // ANMF of a frame at (x, y) (even), its duration, blending (true:
    // alpha-blend) and disposal (true: to the background), over its chunks
    inline std::string anmf(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t duration, bool blend, bool dispose, const std::string& chunks) {
        const char flags = char((blend ? 0 : 2) | (dispose ? 1 : 0));
        return chunk("ANMF", le24(x / 2) + le24(y / 2) + le24(w - 1) + le24(h - 1) + le24(duration) + std::string{flags} + chunks);
    }

    // A pattern of ARGB words, with alpha when asked
    inline std::vector<uint32_t> pattern(uint32_t w, uint32_t h, unsigned seed, bool alpha) {
        std::vector<uint32_t> v(size_t(w) * h);
        for (size_t i = 0; i < v.size(); ++i) {
            const uint32_t x = uint32_t(i % w), y = uint32_t(i / w);
            const uint32_t r = (x * 37 + seed * 11) & 0xff, g = (y * 23 + seed * 5) & 0xff, b = (x * y + seed) & 0xff;
            const uint32_t a = alpha ? ((x * 53 + y * 29 + seed * 7) & 0xff) : 0xff;
            v[i] = a << 24 | r << 16 | g << 8 | b;
        }
        return v;
    }
}
