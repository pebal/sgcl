//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
//     split("") a code point a piece; a backward search from any byte of
//     a valid text starts with the code point that begins at or before it;
//     the code points of UTF-16 units (rune_count, split(u"")) a surrogate
//     pair one, a lone surrogate one;
//   - to_utf16 and to_utf32 give those code points, and from_utf16 and
//     from_utf32 give back the text with every bad byte U+FFFD (the text
//     itself when it was valid); any units given to from_utf16 make valid
//     UTF-8, which to_utf16 takes back to the units when they were
//     well-formed UTF-16;
//   - the case of any UTF-16 units (to_lower, to_upper, equal_fold) is the
//     case of their code points in a u32string, a surrogate pair one code
//     point and a lone surrogate its own value, the size kept; of
//     well-formed units the case of their UTF-8;
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
        // the text interface (mixin::text) on the string and on a slice of
        // the input itself, a buffer of its own size outside the managed
        // heap, where ASan sees a read past its end (a string's would not
        // show: tests/fuzz/input.h)
        auto shared = [&](const auto& t) {
            check(t.rune_count() == points.size());
            check(t.is_valid_utf8() == valid);
            size_t k = 0;
            for (auto it = t.runes().begin(); it != t.runes().end(); ++it) {
                check(k < points.size() && *it == points[k]);
                ++k;
            }
            check(k == points.size());
            // equal_fold: a text equals itself and its own case changes; an
            // ill-formed byte equals only itself, so a text with one byte
            // changed into another ill-formed one is not equal
            check(t.equal_fold(s));
            check(t.equal_fold(text.to_lower().view()) && t.equal_fold(text.to_upper().view()));
            for (size_t i = 0; i < s.size(); i += utf8::decode(s, i).second) {
                if (utf8::decode(s, i) == pair<char32_t, size_t>(utf8::replacement, 1)) {
                    std::string other(s);
                    other[i] = uint8_t(other[i]) == 0xFF ? char(0xFE) : char(0xFF);   // ill-formed anywhere
                    check(!t.equal_fold(other));
                    break;
                }
            }
            // a value that is no code point is in no text, U+FFFD included
            for (char32_t bad : {char32_t(0xD800), char32_t(0xDFFF), char32_t(0x110000)}) {
                check(t.find(bad) == npos && !t.contains(bad) && t.rfind(bad) == npos);
            }
            if (!points.empty()) {
                // a backward search from any byte starts with the code point
                // that begins at or before it, a forward one with the first
                // that begins at or after it: against the starts walked forward,
                // in any text, ill-formed bytes being code points of their own
                std::vector<size_t> starts;
                for (size_t i = 0; i < s.size(); i += utf8::decode(s, i).second) {
                    starts.push_back(i);
                }
                char32_t target = points[points.size() / 2];
                std::u32string_view set(&target, 1);
                for (size_t pos = 0; pos < s.size(); pos += 1 + pos / 8) {
                    size_t expected_of = npos, expected_not = npos;
                    for (size_t j = 0; j < starts.size() && starts[j] <= pos; ++j) {
                        (points[j] == target ? expected_of : expected_not) = starts[j];
                    }
                    check(t.find_last_of(set, pos) == expected_of);
                    check(t.find_last_not_of(set, pos) == expected_not);
                    size_t first_of = npos, first_not = npos;
                    for (size_t j = starts.size(); j-- > 0 && starts[j] >= pos;) {
                        (points[j] == target ? first_of : first_not) = starts[j];
                    }
                    check(t.find_first_of(set, pos) == first_of);
                    check(t.find_first_not_of(set, pos) == first_not);
                }
            }
        };
        shared(text);
        shared(slice<const char>(s.data(), s.size()));
        size_t k = 0;
        // split by nothing: a code point a piece, the pieces the text again
        size_t pieces = 0, covered = 0;
        for (auto piece : text.split("")) {
            check(piece.data() == text.data() + covered && piece.size() >= 1 && piece.size() <= 4);
            covered += piece.size();
            ++pieces;
        }
        check(pieces == points.size() && covered == s.size());
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

    // The code points of UTF-16 units as the case functions take them: a
    // surrogate pair one, any other unit (a lone surrogate too) its value
    std::u32string points_of(std::u16string_view units) {
        std::u32string out;
        for (size_t i = 0; i < units.size(); ++i) {
            char32_t u = units[i];
            if (u >= 0xD800 && u < 0xDC00 && i + 1 < units.size() && units[i + 1] >= 0xDC00 && units[i + 1] < 0xE000) {
                out.push_back(0x10000 + ((u - 0xD800) << 10) + (char32_t(units[i + 1]) - 0xDC00));
                ++i;
            } else {
                out.push_back(u);
            }
        }
        return out;
    }

    // The case of any UTF-16 units against a u32string of their code
    // points: to_lower and to_upper give the same code points, the size
    // kept, the same object exactly when nothing changes; a text
    // equal_folds its own case changes, and against the other case of
    // those and against units with one bit of one unit flipped answers as
    // the code points do
    void utf16_case(const u16string& wide, char16_t seed) {
        std::u32string points = points_of(wide.view());
        u32string w32{std::u32string_view(points)};
        for (int upper = 0; upper < 2; ++upper) {
            u16string m = upper ? wide.to_upper() : wide.to_lower();
            u32string m32 = upper ? w32.to_upper() : w32.to_lower();
            check(m.size() == wide.size());
            check(points_of(m.view()) == m32.view());
            check((m.object() == wide.object()) == (m32.object() == w32.object()));
            check(wide.equal_fold(m) && m.equal_fold(wide));
            check((upper ? m.to_lower() : m.to_upper()).equal_fold(wide) == (upper ? m32.to_lower() : m32.to_upper()).equal_fold(w32));
        }
        if (!wide.empty()) {
            std::u16string other(wide.view());
            other[seed % other.size()] ^= char16_t(1u << (seed >> 12));
            u32string other32{std::u32string_view(points_of(other))};
            check(wide.equal_fold(other) == w32.equal_fold(other32));
            check(u16string(other).equal_fold(wide) == other32.equal_fold(w32));
        }
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
        // the conversion reads the units from a buffer of their own size
        // (raw, which units equals), where ASan sees an overread
        string text = txt::from_utf16(slice<const char16_t>(raw.data(), raw.size()));
        check(utf8::valid(text.view()));
        // the code points of the units: a surrogate pair is one, a lone
        // surrogate one; split by nothing gives them a piece each
        u16string wide{std::u16string_view(units.data(), units.size())};
        size_t steps = 0;
        for (size_t i = 0; i < units.size(); ++i, ++steps) {
            if (units[i] >= 0xD800 && units[i] < 0xDC00 && i + 1 < units.size() && units[i + 1] >= 0xDC00 && units[i + 1] < 0xE000) {
                ++i;
            }
        }
        check(wide.rune_count() == steps);
        check(slice<const char16_t>(raw.data(), raw.size()).rune_count() == steps);
        size_t pieces = 0, covered = 0;
        for (auto piece : wide.split(u"")) {
            check(piece.size() == 1 || piece.size() == 2);
            covered += piece.size();
            ++pieces;
        }
        check(pieces == steps && covered == units.size());
        utf16_case(wide, raw.empty() ? 0 : raw[0]);
        auto back = txt::to_utf16(text);
        if (well_formed) {
            // the case of well-formed UTF-16 is the case of its UTF-8
            check(txt::from_utf16(wide.to_lower()) == text.to_lower());
            check(txt::from_utf16(wide.to_upper()) == text.to_upper());
        }
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
        string t32 = txt::from_utf32(slice<const char32_t>(raw32.data(), raw32.size()));   // points, in a buffer of its size
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
