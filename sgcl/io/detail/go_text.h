//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/utf8.h"
#include "../../txt/properties.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>

// The text of values as Go's strconv reads and writes it, for io::flags,
// whose command line and messages are Go's flag package's: integers with
// a base prefix and underscores (ParseInt and ParseUint with base 0),
// booleans (ParseBool), floating-point numbers (ParseFloat: decimal and
// hexadecimal, "inf", "nan", underscores), the shortest form of a float
// (FormatFloat 'g', -1) and a quoted string (Quote). Written from Go's
// documentation and tested against Go itself (tests/io/flags.cpp).
namespace sgcl::io::detail {
    // What a reading of a number came to: Go's ErrSyntax and ErrRange
    enum class GoNumber : uint8_t {
        ok,
        syntax,
        range
    };

    constexpr char go_lower(char c) noexcept {
        return char(c | 0x20);
    }

    // Underscores only between digits, or between a base prefix and a
    // digit (strconv's underscoreOK)
    inline bool go_underscores_ok(std::string_view s) noexcept {
        char saw = '^';
        size_t i = 0;
        if (!s.empty() && (s[0] == '-' || s[0] == '+')) {
            s.remove_prefix(1);
        }
        bool hex = false;
        if (s.size() >= 2 && s[0] == '0' && (go_lower(s[1]) == 'b' || go_lower(s[1]) == 'o' || go_lower(s[1]) == 'x')) {
            i = 2;
            saw = '0';
            hex = go_lower(s[1]) == 'x';
        }
        for (; i < s.size(); ++i) {
            char c = s[i];
            if ((c >= '0' && c <= '9') || (hex && go_lower(c) >= 'a' && go_lower(c) <= 'f')) {
                saw = '0';
                continue;
            }
            if (c == '_') {
                if (saw != '0') {
                    return false;
                }
                saw = '_';
                continue;
            }
            if (saw == '_') {
                return false;
            }
            saw = '!';
        }
        return saw != '_';
    }

    // ParseUint(s, 0, bits): the base from the prefix (0b, 0o, 0x, or a
    // leading 0 for octal), underscores allowed per Go's syntax; out is
    // the largest value on a range error, as Go's
    inline GoNumber go_parse_uint(std::string_view s, unsigned bits, uint64_t& out) noexcept {
        out = 0;
        if (s.empty()) {
            return GoNumber::syntax;
        }
        const std::string_view whole = s;
        uint64_t base = 10;
        if (s[0] == '0') {
            if (s.size() >= 3 && go_lower(s[1]) == 'b') {
                base = 2;
                s.remove_prefix(2);
            } else if (s.size() >= 3 && go_lower(s[1]) == 'o') {
                base = 8;
                s.remove_prefix(2);
            } else if (s.size() >= 3 && go_lower(s[1]) == 'x') {
                base = 16;
                s.remove_prefix(2);
            } else {
                base = 8;
                s.remove_prefix(1);
            }
        }
        const uint64_t max = bits >= 64 ? ~uint64_t(0) : (uint64_t(1) << bits) - 1;
        const uint64_t cutoff = ~uint64_t(0) / base + 1;
        uint64_t n = 0;
        bool underscores = false;
        for (char c : s) {
            uint64_t d;
            if (c == '_') {
                underscores = true;
                continue;
            } else if (c >= '0' && c <= '9') {
                d = uint64_t(c - '0');
            } else if (go_lower(c) >= 'a' && go_lower(c) <= 'z') {
                d = uint64_t(go_lower(c) - 'a' + 10);
            } else {
                return GoNumber::syntax;
            }
            if (d >= base) {
                return GoNumber::syntax;
            }
            if (n >= cutoff) {
                out = max;
                return GoNumber::range;
            }
            n *= base;
            uint64_t n1 = n + d;
            if (n1 < n || n1 > max) {
                out = max;
                return GoNumber::range;
            }
            n = n1;
        }
        if (underscores && !go_underscores_ok(whole)) {
            return GoNumber::syntax;
        }
        out = n;
        return GoNumber::ok;
    }

    // ParseInt(s, 0, bits): a sign, then ParseUint, then the signed range
    inline GoNumber go_parse_int(std::string_view s, unsigned bits, int64_t& out) noexcept {
        out = 0;
        if (s.empty()) {
            return GoNumber::syntax;
        }
        bool neg = false;
        if (s[0] == '+') {
            s.remove_prefix(1);
        } else if (s[0] == '-') {
            neg = true;
            s.remove_prefix(1);
        }
        uint64_t un;
        GoNumber r = go_parse_uint(s, bits, un);
        if (r == GoNumber::syntax) {
            return r;
        }
        const uint64_t cutoff = uint64_t(1) << (bits - 1);
        if (!neg && un >= cutoff) {
            out = int64_t(cutoff - 1);
            return GoNumber::range;
        }
        if (neg && un > cutoff) {
            out = int64_t(0) - int64_t(cutoff - 1) - 1;
            return GoNumber::range;
        }
        out = neg ? int64_t(0 - un) : int64_t(un);
        return GoNumber::ok;
    }

    // ParseBool: 1, t, T, TRUE, true, True and their false
    inline GoNumber go_parse_bool(std::string_view s, bool& out) noexcept {
        if (s == "1" || s == "t" || s == "T" || s == "TRUE" || s == "true" || s == "True") {
            out = true;
            return GoNumber::ok;
        }
        if (s == "0" || s == "f" || s == "F" || s == "FALSE" || s == "false" || s == "False") {
            out = false;
            return GoNumber::ok;
        }
        return GoNumber::syntax;
    }

    // The length of the longest common prefix of s and a lower-case word,
    // the case of s ignored
    inline size_t go_prefix_ignoring_case(std::string_view s, std::string_view word) noexcept {
        size_t n = 0;
        while (n < s.size() && n < word.size() && go_lower(s[n]) == word[n]) {
            ++n;
        }
        return n;
    }

    // ParseFloat(s, 64): "inf", "infinity", "nan" in any case (a sign on
    // the infinities), decimal and hexadecimal (0x, a 'p' exponent
    // required) with underscores per Go's syntax; the whole text or a
    // syntax error; an overflow is a range error, an underflow is not
    inline GoNumber go_parse_float(std::string_view s, double& out) noexcept {
        out = 0;
        if (s.empty()) {
            return GoNumber::syntax;
        }
        {   // the specials
            size_t at = 0;
            double sign = 1;
            if (s[0] == '+' || s[0] == '-') {
                sign = s[0] == '-' ? -1 : 1;
                at = 1;
            }
            std::string_view rest = s.substr(at);
            if (!rest.empty() && (rest[0] == 'i' || rest[0] == 'I')) {
                size_t n = go_prefix_ignoring_case(rest, "infinity");
                if (n > 3 && n < 8) {
                    n = 3;
                }
                if ((n == 3 || n == 8) && n == rest.size()) {
                    out = sign * std::numeric_limits<double>::infinity();
                    return GoNumber::ok;
                }
                if (n == 3 || n == 8) {
                    return GoNumber::syntax;
                }
            } else if (at == 0 && (s[0] == 'n' || s[0] == 'N')) {
                if (go_prefix_ignoring_case(s, "nan") == 3 && s.size() == 3) {
                    out = std::numeric_limits<double>::quiet_NaN();
                    return GoNumber::ok;
                }
            }
        }
        // readFloat's syntax
        size_t i = 0;
        bool neg = false;
        if (s[i] == '+') {
            ++i;
        } else if (s[i] == '-') {
            neg = true;
            ++i;
        }
        bool hex = false;
        char exp_char = 'e';
        if (i + 2 < s.size() && s[i] == '0' && go_lower(s[i + 1]) == 'x') {
            hex = true;
            exp_char = 'p';
            i += 2;
        }
        const size_t digits_at = i;
        bool underscores = false;
        bool saw_dot = false;
        bool saw_digits = false;
        for (; i < s.size(); ++i) {
            char c = s[i];
            if (c == '_') {
                underscores = true;
                continue;
            }
            if (c == '.') {
                if (saw_dot) {
                    break;
                }
                saw_dot = true;
                continue;
            }
            if ((c >= '0' && c <= '9') || (hex && go_lower(c) >= 'a' && go_lower(c) <= 'f')) {
                saw_digits = true;
                continue;
            }
            break;
        }
        if (!saw_digits) {
            return GoNumber::syntax;
        }
        if (i < s.size() && go_lower(s[i]) == exp_char) {
            ++i;
            if (i >= s.size()) {
                return GoNumber::syntax;
            }
            if (s[i] == '+' || s[i] == '-') {
                ++i;
            }
            if (i >= s.size() || s[i] < '0' || s[i] > '9') {
                return GoNumber::syntax;
            }
            for (; i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '_'); ++i) {
                if (s[i] == '_') {
                    underscores = true;
                }
            }
        } else if (hex) {
            return GoNumber::syntax;   // a hexadecimal mantissa needs its exponent
        }
        if (i != s.size()) {
            return GoNumber::syntax;
        }
        if (underscores && !go_underscores_ok(s)) {
            return GoNumber::syntax;
        }
        // the value: the text without its underscores, its sign and its prefix
        std::string clean;
        clean.reserve(s.size());
        for (size_t k = digits_at; k < s.size(); ++k) {
            if (s[k] != '_') {
                clean += s[k];
            }
        }
        double v = 0;
        auto [end, ec] = std::from_chars(clean.data(), clean.data() + clean.size(), v, hex ? std::chars_format::hex : std::chars_format::general);
        if (ec == std::errc::result_out_of_range) {
            // an overflow or an underflow: strtod tells them apart
            std::string c = hex ? "0x" + clean : clean;
            v = std::strtod(c.c_str(), nullptr);
            if (std::isinf(v)) {
                out = neg ? -v : v;
                return GoNumber::range;
            }
        } else if (ec != std::errc() || end != clean.data() + clean.size()) {
            return GoNumber::syntax;
        }
        out = neg ? -v : v;
        return GoNumber::ok;
    }

    // FormatFloat(f, 'g', -1, 64): the shortest digits that read back as
    // f, in the exponent form when the exponent is under -4 or 21 and
    // over (6 and over for the shortest form, Go's rule); "+Inf", "-Inf",
    // "NaN"
    inline std::string go_format_float(double f) noexcept {
        if (std::isnan(f)) {
            return "NaN";
        }
        if (std::isinf(f)) {
            return f > 0 ? "+Inf" : "-Inf";
        }
        char sci[64];
        auto r = std::to_chars(sci, sci + sizeof sci, f, std::chars_format::scientific);
        std::string_view e(sci, size_t(r.ptr - sci));
        int exp = 0;
        {
            auto at = e.find('e');
            std::from_chars(e.data() + at + 1 + (e[at + 1] == '+' ? 1 : 0), e.data() + e.size(), exp);
        }
        if (exp < -4 || exp >= 6) {
            return std::string(e);
        }
        char fixed[64];
        auto q = std::to_chars(fixed, fixed + sizeof fixed, f, std::chars_format::fixed);
        return std::string(fixed, size_t(q.ptr - fixed));
    }

    // Go's unicode.IsPrint: the letters, marks, numbers, punctuation and
    // symbols, and the ASCII space
    inline bool go_is_print(char32_t r) noexcept {
        if (r == U' ') {
            return true;
        }
        if (r < 0x80) {
            return r > 0x20 && r < 0x7f;
        }
        auto c = txt::detail::category_of_fn(r);
        return c >= txt::category::uppercase_letter && c <= txt::category::other_symbol;
    }

    // strconv.Quote: in double quotes, a byte of no valid sequence as
    // \xhh, what IsPrint refuses escaped (\a \b \f \n \r \t \v, \xhh
    // under the space and for DEL, \uhhhh, \Uhhhhhhhh)
    inline void go_quote(std::string& out, std::string_view s) noexcept {
        static constexpr char Hex[] = "0123456789abcdef";
        out += '"';
        size_t i = 0;
        while (i < s.size()) {
            auto [r, n] = utf8::decode(s, i);
            if (n == 1 && static_cast<unsigned char>(s[i]) >= 0x80) {
                unsigned char b = static_cast<unsigned char>(s[i]);
                out += "\\x";
                out += Hex[b >> 4];
                out += Hex[b & 0xF];
                ++i;
                continue;
            }
            if (r == U'"' || r == U'\\') {
                out += '\\';
                out += char(r);
            } else if (go_is_print(r)) {
                out.append(s.substr(i, n));
            } else {
                switch (r) {
                    case U'\a': out += "\\a"; break;
                    case U'\b': out += "\\b"; break;
                    case U'\f': out += "\\f"; break;
                    case U'\n': out += "\\n"; break;
                    case U'\r': out += "\\r"; break;
                    case U'\t': out += "\\t"; break;
                    case U'\v': out += "\\v"; break;
                    default:
                        if (r < U' ' || r == 0x7f) {
                            out += "\\x";
                            out += Hex[(r >> 4) & 0xF];
                            out += Hex[r & 0xF];
                        } else if (r < 0x10000) {
                            out += "\\u";
                            for (int sh = 12; sh >= 0; sh -= 4) {
                                out += Hex[(r >> sh) & 0xF];
                            }
                        } else {
                            out += "\\U";
                            for (int sh = 28; sh >= 0; sh -= 4) {
                                out += Hex[(r >> sh) & 0xF];
                            }
                        }
                }
            }
            i += n;
        }
        out += '"';
    }
}
