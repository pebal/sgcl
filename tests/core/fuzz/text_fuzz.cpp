//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The text primitives of core on any bytes: UTF-8 (utf8::valid, decode,
// decode_last, count, ascii_run, runes, rune_count), the conversions to and
// from UTF-16 and UTF-32 (txt/encoding.h) and parse<T> of the numbers. The
// first byte picks the path. What must hold:
//   - decoding walks the bytes as a decoder written from RFC 3629 here
//     walks them: a well-formed sequence is its code point, anything else
//     one byte and U+FFFD (as Go); valid() is "no U+FFFD from a bad byte",
//     count() and rune_count() the number of steps, runes() the same
//     code points; a valid text walked from its end gives them reversed;
//   - to_utf16 and to_utf32 give those code points, and from_utf16 and
//     from_utf32 give back the text with every bad byte U+FFFD (the text
//     itself when it was valid); any units given to from_utf16 make valid
//     UTF-8, which to_utf16 takes back to the units when they were
//     well-formed UTF-16;
//   - parse<T>(to_string(n)) == n for every integer type; parse of any
//     text either fails, with an offset inside the text, or reads all of
//     it; any base, including one no number is written in, is an error
//     and not a crash.
// Built with libFuzzer (tests/fuzz/run.sh tests/core/fuzz/text_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/core/string.h"
#include "sgcl/txt/encoding.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // RFC 3629 §4, table 3-7 of the Unicode standard: the well-formed sequences
    struct Step {
        char32_t c;
        size_t n;
        bool bad;
    };

    Step reference(std::string_view s, size_t i) {
        auto b = [&](size_t k) { return i + k < s.size() ? uint8_t(s[i + k]) : 0; };
        auto in = [](uint8_t x, uint8_t lo, uint8_t hi) { return x >= lo && x <= hi; };
        uint8_t b0 = b(0);
        if (b0 < 0x80) {
            return {b0, 1, false};
        }
        if (in(b0, 0xC2, 0xDF) && in(b(1), 0x80, 0xBF)) {
            return {char32_t((b0 & 0x1F) << 6 | (b(1) & 0x3F)), 2, false};
        }
        if (b0 >= 0xE0 && b0 <= 0xEF) {
            uint8_t lo = b0 == 0xE0 ? 0xA0 : 0x80;
            uint8_t hi = b0 == 0xED ? 0x9F : 0xBF;
            if (in(b(1), lo, hi) && in(b(2), 0x80, 0xBF)) {
                return {char32_t((b0 & 0x0F) << 12 | (b(1) & 0x3F) << 6 | (b(2) & 0x3F)), 3, false};
            }
        }
        if (b0 >= 0xF0 && b0 <= 0xF4) {
            uint8_t lo = b0 == 0xF0 ? 0x90 : 0x80;
            uint8_t hi = b0 == 0xF4 ? 0x8F : 0xBF;
            if (in(b(1), lo, hi) && in(b(2), 0x80, 0xBF) && in(b(3), 0x80, 0xBF)) {
                return {char32_t((b0 & 0x07) << 18 | (b(1) & 0x3F) << 12 | (b(2) & 0x3F) << 6 | (b(3) & 0x3F)), 4, false};
            }
        }
        return {0xFFFD, 1, true};
    }

    void utf8_walk(std::string_view s) {
        std::vector<char32_t> points;
        std::string repaired;
        bool valid = true;
        for (size_t i = 0; i < s.size();) {
            Step r = reference(s, i);
            auto [c, n] = utf8::decode(s, i);
            check(c == r.c && n == r.n);
            valid = valid && !r.bad;
            char buf[4];
            size_t w = utf8::encode(c, buf);
            check(w == utf8::width(c) && w >= 1 && w <= 4);
            if (!r.bad) {
                check(std::string_view(buf, w) == s.substr(i, n));
            }
            repaired.append(buf, w);
            size_t run = utf8::ascii_run(s, i);
            check(run == 0 ? uint8_t(s[i]) >= 0x80 : uint8_t(s[i]) < 0x80);
            check(i + run <= s.size());
            points.push_back(c);
            i += n;
        }
        check(utf8::valid(s) == valid);
        check(utf8::count(s) == points.size());
        check(utf8::all_ascii(s) == (utf8::ascii_run(s) == s.size()));
        string text(s);
        check(text.rune_count() == points.size());
        check(text.is_valid_utf8() == valid);
        size_t k = 0;
        for (auto it = text.runes().begin(); it != text.runes().end(); ++it) {
            check(k < points.size() && *it == points[k]);
            ++k;
        }
        check(k == points.size());
        if (valid) {
            // from the end, the same code points reversed
            size_t end = s.size();
            k = points.size();
            while (end > 0) {
                auto [c, n] = utf8::decode_last(s, end);
                check(k > 0 && c == points[--k] && n >= 1 && n <= end);
                end -= n;
            }
            check(k == 0);
        } else {
            size_t end = s.size();
            while (end > 0) {
                auto [c, n] = utf8::decode_last(s, end);
                check(n >= 1 && n <= end && n <= 4);
                check(utf8::valid(c));
                end -= n;
            }
        }
        // the conversions: the same code points, and back the repaired text
        auto u16 = txt::to_utf16(text);
        auto u32 = txt::to_utf32(text);
        check(u32.size() == points.size());
        for (size_t j = 0; j < points.size(); ++j) {
            check(u32[j] == points[j]);
        }
        check(txt::from_utf32(u32).view() == repaired);
        check(txt::from_utf16(u16).view() == repaired);
        check(txt::to_utf16(txt::from_utf16(u16)) == u16);
    }

    void utf16_units(std::string_view bytes) {
        std::vector<char16_t> raw(bytes.size() / 2);
        std::memcpy(raw.data(), bytes.data(), raw.size() * 2);
        vector<char16_t> units;
        bool well_formed = true;
        for (size_t i = 0; i < raw.size(); ++i) {
            units.push_back(raw[i]);
            char16_t u = raw[i];
            if (u >= 0xD800 && u < 0xDC00 && i + 1 < raw.size() && raw[i + 1] >= 0xDC00 && raw[i + 1] < 0xE000) {
                units.push_back(raw[++i]);
            } else if (u >= 0xD800 && u < 0xE000) {
                well_formed = false;
            }
        }
        string text = txt::from_utf16(units);
        check(utf8::valid(text.view()));
        auto back = txt::to_utf16(text);
        if (well_formed) {
            check(back == units);
        }
        check(txt::from_utf16(back) == text);
        std::vector<char32_t> raw32(bytes.size() / 4);
        std::memcpy(raw32.data(), bytes.data(), raw32.size() * 4);
        vector<char32_t> points;
        for (auto c : raw32) {
            points.push_back(c);
        }
        string t32 = txt::from_utf32(points);
        check(utf8::valid(t32.view()));
        check(txt::to_utf32(t32).size() == points.size());
    }

    template<class T>
    void integer(std::string_view bytes, std::string_view text, int base) {
        T n = 0;
        std::memcpy(&n, bytes.data(), std::min(sizeof(T), bytes.size()));
        auto back = parse<T>(to_string(n).view());
        check(back.has_value() && *back == n);
        auto r = parse<T>(text, base);
        if (r) {
            check(!text.empty());
            if (base == 10) {
                // what is read writes the same number
                check(*parse<T>(to_string(*r).view()) == *r);
            }
        } else {
            check(r.error().offset() <= text.size());
        }
    }

    template<class T>
    void floating(std::string_view text) {
        auto r = parse<T>(text);
        if (!r) {
            check(r.error().offset() <= text.size());
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 16384) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    switch (mode % 3) {
        case 0: utf8_walk(rest); break;
        case 1: utf16_units(rest); break;
        case 2: {
            // the base from the second byte: 2 to 36 mostly, and the rest
            // (0, 1, 37 and up) now and then
            int base = rest.empty() ? 10 : uint8_t(rest[0]);
            base = base < 200 ? 2 + base % 35 : base - 200;
            if ((mode >> 2) & 1) {
                base = 10;
            }
            std::string_view text = rest.empty() ? rest : rest.substr(1);
            switch ((mode >> 3) % 10) {
                case 0: integer<int>(text, text, base); break;
                case 1: integer<unsigned>(text, text, base); break;
                case 2: integer<int64_t>(text, text, base); break;
                case 3: integer<uint64_t>(text, text, base); break;
                case 4: integer<int8_t>(text, text, base); break;
                case 5: integer<uint8_t>(text, text, base); break;
                case 6: integer<int16_t>(text, text, base); break;
                case 7: integer<uint16_t>(text, text, base); break;
                case 8: floating<double>(text); break;
                case 9: floating<float>(text); break;
            }
            break;
        }
    }
    return 0;
}
