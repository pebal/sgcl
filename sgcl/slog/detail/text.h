//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/config.h"
#include "../../core/detail/bytes.h"
#include "../../core/detail/os.h"
#include "../../core/unicode.h"
#include "../../core/utf8.h"
#include "../../txt/properties.h"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>

// The pieces of a line of slog: a growing buffer of plain memory and the
// texts of the values, each as Go's log/slog writes it, byte for byte (the
// oracle is tools/slog_oracle.go): the quoting of the text handler
// (strconv.Quote), the escaping of the JSON handler and of a value
// json.Marshal writes, the numbers of %v and of JSON, a duration, a time.
namespace sgcl::slog::detail {
    // Bytes grown by doubling, in plain memory: the line a record is
    // assembled in, the attributes a logger renders once. Never holds a
    // tracked word, so it may lie in a thread_local (DESIGN 283, the
    // auditor's condition 2).
    class Buf {
    public:
        Buf() noexcept = default;
        Buf(const Buf&) = delete;
        Buf& operator=(const Buf&) = delete;

        SGCL_INLINE_HOT ~Buf() {
            std::free(_p);
        }

        SGCL_INLINE_HOT char* data() noexcept {
            return _p;
        }

        SGCL_INLINE_HOT const char* data() const noexcept {
            return _p;
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _n;
        }

        SGCL_INLINE_HOT size_t capacity() const noexcept {
            return _cap;
        }

        SGCL_INLINE_HOT std::string_view view() const noexcept {
            return std::string_view(_p, _n);
        }

        SGCL_INLINE_HOT void clear() noexcept {
            _n = 0;
        }

        // Back to the first n bytes (a group that came out empty)
        SGCL_INLINE_HOT void resize_down(size_t n) noexcept {
            _n = n;
        }

        // Room for k more bytes, at the end: the caller writes there and
        // then says how many it wrote (commit). The members that grow
        // cannot throw: a failed realloc ends the program (os::memory_refused)
        SGCL_INLINE_HOT char* reserve(size_t k) noexcept {
            if (_cap - _n < k) [[unlikely]] {
                _grow(_n + k);
            }
            return _p + _n;
        }

        SGCL_INLINE_HOT void commit(size_t k) noexcept {
            _n += k;
        }

        SGCL_INLINE_HOT void put(char c) noexcept {
            if (_n == _cap) [[unlikely]] {
                _grow(_n + 1);
            }
            _p[_n++] = c;
        }

        SGCL_INLINE_HOT void put(const char* s, size_t k) noexcept {
            if (_cap - _n < k) [[unlikely]] {
                _grow(_n + k);
            }
            if (k) {
                sgcl::detail::copy_bytes(_p + _n, s, k);
            }
            _n += k;
        }

        SGCL_INLINE_HOT void put(std::string_view s) noexcept {
            put(s.data(), s.size());
        }

        SGCL_INLINE_HOT char back() const noexcept {
            return _p[_n - 1];
        }

    private:
        SGCL_NOINLINE void _grow(size_t need) noexcept {
            size_t cap = _cap ? _cap * 2 : 256;
            while (cap < need) {
                cap *= 2;
            }
            char* p = static_cast<char*>(std::realloc(_p, cap));
            if (!p) [[unlikely]] {
                sgcl::detail::os::memory_refused("a log line", cap);
            }
            _p = p;
            _cap = cap;
        }

        char* _p = nullptr;
        size_t _n = 0;
        size_t _cap = 0;
    };

    inline constexpr char LowerHex[] = "0123456789abcdef";

    // A byte of text as the quoting of the text handler sees it: 0 needs
    // no quoting, 1 needs it (a space, '=', '"', a control), 2 is the
    // start of a sequence of more bytes, which is decoded. The backslash
    // and DEL alone are safe (slog asks JSON's set of safe bytes, which
    // has both): each is escaped only once a text is quoted for another
    // reason, as slog does
    struct QuoteTable {
        uint8_t v[256];

        constexpr QuoteTable() noexcept : v() {
            for (int c = 0; c < 256; ++c) {
                uint8_t k = 0;
                if (c >= 0x80) {
                    k = 2;
                } else if (c < 0x20 || c == ' ' || c == '=' || c == '"') {
                    k = 1;
                }
                v[c] = k;
            }
        }
    };

    inline constexpr QuoteTable QuoteKinds{};

    // Whether the text handler writes s in quotes: empty, or a byte or a
    // code point above that is not safe as it is — an ASCII control, a
    // space, '=', '"', an invalid sequence of UTF-8 (and the replacement
    // character itself), a white space of Unicode, a code point Go's
    // unicode.IsPrint refuses
    inline bool text_needs_quoting(const char* s, size_t n) noexcept {
        if (n == 0) {
            return true;
        }
        const std::string_view text(s, n);
        for (size_t i = 0; i < n;) {
            uint8_t k = QuoteKinds.v[(unsigned char)s[i]];
            if (k == 0) [[likely]] {
                ++i;
                continue;
            }
            if (k == 1) {
                return true;
            }
            auto [c, w] = utf8::decode(text, i);
            if (c == utf8::replacement || unicode::is_space(c) || !txt::is_printable(c)) {
                return true;
            }
            i += w;
        }
        return false;
    }

    // The inside of Go's strconv.Quote: '"' and '\\' escaped, a printable
    // code point as it is, \a \b \f \n \r \t \v, an ASCII control and DEL
    // as \xHH, a byte of invalid UTF-8 as \xHH, any other code point as
    // \uHHHH or \UHHHHHHHH
    inline void text_escaped(Buf& b, const char* s, size_t n) noexcept {
        const std::string_view text(s, n);
        for (size_t i = 0; i < n;) {
            unsigned char u = (unsigned char)s[i];
            if (u < 0x80) {
                size_t j = i;
                while (j < n && (unsigned char)s[j] >= 0x20 && (unsigned char)s[j] < 0x7f && s[j] != '"' && s[j] != '\\') {
                    ++j;
                }
                if (j != i) {
                    b.put(s + i, j - i);
                    i = j;
                    continue;
                }
                char* o = b.reserve(4);
                switch (u) {
                    case '"': o[0] = '\\'; o[1] = '"'; b.commit(2); break;
                    case '\\': o[0] = '\\'; o[1] = '\\'; b.commit(2); break;
                    case '\a': o[0] = '\\'; o[1] = 'a'; b.commit(2); break;
                    case '\b': o[0] = '\\'; o[1] = 'b'; b.commit(2); break;
                    case '\f': o[0] = '\\'; o[1] = 'f'; b.commit(2); break;
                    case '\n': o[0] = '\\'; o[1] = 'n'; b.commit(2); break;
                    case '\r': o[0] = '\\'; o[1] = 'r'; b.commit(2); break;
                    case '\t': o[0] = '\\'; o[1] = 't'; b.commit(2); break;
                    case '\v': o[0] = '\\'; o[1] = 'v'; b.commit(2); break;
                    default:
                        o[0] = '\\';
                        o[1] = 'x';
                        o[2] = LowerHex[u >> 4];
                        o[3] = LowerHex[u & 15];
                        b.commit(4);
                }
                ++i;
                continue;
            }
            auto [c, w] = utf8::decode(text, i);
            if (c == utf8::replacement && w == 1) {
                char* o = b.reserve(4);
                o[0] = '\\';
                o[1] = 'x';
                o[2] = LowerHex[u >> 4];
                o[3] = LowerHex[u & 15];
                b.commit(4);
                i += 1;
                continue;
            }
            if (txt::is_printable(c)) {
                b.put(s + i, w);
            } else if (c < 0x10000) {
                char* o = b.reserve(6);
                o[0] = '\\';
                o[1] = 'u';
                for (int k = 0; k < 4; ++k) {
                    o[2 + k] = LowerHex[(c >> (12 - 4 * k)) & 15];
                }
                b.commit(6);
            } else {
                char* o = b.reserve(10);
                o[0] = '\\';
                o[1] = 'U';
                for (int k = 0; k < 8; ++k) {
                    o[2 + k] = LowerHex[(c >> (28 - 4 * k)) & 15];
                }
                b.commit(10);
            }
            i += w;
        }
    }

    // A text as the text handler writes it: as it is, or in quotes
    SGCL_INLINE_HOT void text_string(Buf& b, const char* s, size_t n) noexcept {
        if (!text_needs_quoting(s, n)) {
            b.put(s, n);
            return;
        }
        b.put('"');
        text_escaped(b, s, n);
        b.put('"');
    }

    // The inside of a JSON string. slog's own escaping for the strings of
    // its attributes (short: \" \\ \n \r \t; every other control \u00hh),
    // json.Marshal's for a string inside a value it writes (`marshal`:
    // \b and \f short as well). Both write an invalid byte of UTF-8 as
    // the replacement character, U+2028 and U+2029 as escapes, DEL and
    // everything else as it is, and neither escapes <, > or &
    inline void json_escaped(Buf& b, const char* s, size_t n, bool marshal = false) noexcept {
        const std::string_view text(s, n);
        size_t i = 0;
        size_t start = 0;
        while (i < n) {
            unsigned char u = (unsigned char)s[i];
            if (u >= 0x20 && u != '"' && u != '\\' && u < 0x80) {
                ++i;
                continue;
            }
            if (u >= 0x80) {
                auto [c, w] = utf8::decode(text, i);
                if (c == utf8::replacement && w == 1) {
                    b.put(s + start, i - start);
                    b.put("\xEF\xBF\xBD", 3);
                    i += 1;
                    start = i;
                    continue;
                }
                if (c == 0x2028 || c == 0x2029) {
                    b.put(s + start, i - start);
                    b.put(c == 0x2028 ? "\\u2028" : "\\u2029", 6);
                    i += w;
                    start = i;
                    continue;
                }
                i += w;
                continue;
            }
            b.put(s + start, i - start);
            char* o = b.reserve(6);
            switch (u) {
                case '"': o[0] = '\\'; o[1] = '"'; b.commit(2); break;
                case '\\': o[0] = '\\'; o[1] = '\\'; b.commit(2); break;
                case '\n': o[0] = '\\'; o[1] = 'n'; b.commit(2); break;
                case '\r': o[0] = '\\'; o[1] = 'r'; b.commit(2); break;
                case '\t': o[0] = '\\'; o[1] = 't'; b.commit(2); break;
                default:
                    if (marshal && (u == '\b' || u == '\f')) {
                        o[0] = '\\';
                        o[1] = u == '\b' ? 'b' : 'f';
                        b.commit(2);
                    } else {
                        o[0] = '\\';
                        o[1] = 'u';
                        o[2] = '0';
                        o[3] = '0';
                        o[4] = LowerHex[u >> 4];
                        o[5] = LowerHex[u & 15];
                        b.commit(6);
                    }
            }
            ++i;
            start = i;
        }
        b.put(s + start, i - start);
    }

    SGCL_INLINE_HOT void json_string(Buf& b, const char* s, size_t n, bool marshal = false) noexcept {
        b.put('"');
        json_escaped(b, s, n, marshal);
        b.put('"');
    }

    inline void put_uint(Buf& b, uint64_t v) noexcept {
        char* o = b.reserve(20);
        char digits[20];
        int k = 0;
        do {
            digits[k++] = char('0' + v % 10);
            v /= 10;
        } while (v);
        for (int i = 0; i < k; ++i) {
            o[i] = digits[k - 1 - i];
        }
        b.commit(size_t(k));
    }

    SGCL_INLINE_HOT void put_int(Buf& b, int64_t v) noexcept {
        if (v < 0) {
            b.put('-');
            put_uint(b, uint64_t(0) - uint64_t(v));
        } else {
            put_uint(b, uint64_t(v));
        }
    }

    // The shortest digits that read back as x (x finite, above zero) and
    // the decimal exponent of the first: x = 0.d1d2... * 10^point
    struct Digits {
        char d[20];
        int count = 0;
        int point = 0;
    };

    inline Digits shortest_digits(double x) noexcept {
        Digits r;
        char buf[40];
        auto res = std::to_chars(buf, buf + sizeof buf, x, std::chars_format::scientific);
        const char* q = buf;
        for (; q != res.ptr && *q != 'e'; ++q) {
            if (*q != '.') {
                r.d[r.count++] = *q;
            }
        }
        int e10 = 0;
        if (q != res.ptr) {
            ++q;
            bool minus = *q == '-';
            if (*q == '-' || *q == '+') {
                ++q;
            }
            for (; q != res.ptr; ++q) {
                e10 = e10 * 10 + (*q - '0');
            }
            if (minus) {
                e10 = -e10;
            }
        }
        while (r.count > 1 && r.d[r.count - 1] == '0') {
            --r.count;
        }
        r.point = e10 + 1;
        return r;
    }

    // %e of the shortest digits: d.ddde+XX, the exponent of two digits at least
    inline char* put_exponent_form(char* o, const Digits& g, bool pad_exponent) noexcept {
        *o++ = g.d[0];
        if (g.count > 1) {
            *o++ = '.';
            sgcl::detail::copy_bytes(o, g.d + 1, size_t(g.count - 1));
            o += g.count - 1;
        }
        int e = g.point - 1;
        *o++ = 'e';
        *o++ = e < 0 ? '-' : '+';
        unsigned a = unsigned(e < 0 ? -e : e);
        if (a >= 100) {
            *o++ = char('0' + a / 100);
            *o++ = char('0' + a / 10 % 10);
            *o++ = char('0' + a % 10);
        } else if (a >= 10 || pad_exponent) {
            *o++ = char('0' + a / 10);
            *o++ = char('0' + a % 10);
        } else {
            *o++ = char('0' + a);
        }
        return o;
    }

    // %f of the shortest digits, no more places than they need
    inline char* put_fixed_form(char* o, const Digits& g) noexcept {
        if (g.point <= 0) {
            *o++ = '0';
            *o++ = '.';
            for (int z = 0; z < -g.point; ++z) {
                *o++ = '0';
            }
            sgcl::detail::copy_bytes(o, g.d, size_t(g.count));
            return o + g.count;
        }
        if (g.point >= g.count) {
            sgcl::detail::copy_bytes(o, g.d, size_t(g.count));
            o += g.count;
            for (int z = g.count; z < g.point; ++z) {
                *o++ = '0';
            }
            return o;
        }
        sgcl::detail::copy_bytes(o, g.d, size_t(g.point));
        o += g.point;
        *o++ = '.';
        sgcl::detail::copy_bytes(o, g.d + g.point, size_t(g.count - g.point));
        return o + (g.count - g.point);
    }

    // The most bytes a number of the two forms below takes: a sign, 17
    // digits, a point and an exponent, or the 308 zeros of 1.8e308 fixed
    inline constexpr size_t FloatTextSize = 330;

    // x as Go's %v writes a float64 (strconv 'g', shortest): the exponent
    // form below 1e-4 and from 1e+06 up (1e+06, 1.234567e+06), fixed
    // between; NaN, +Inf, -Inf, -0
    inline size_t float_text(char* out, double x) noexcept {
        char* o = out;
        if (std::isnan(x)) {
            std::memcpy(o, "NaN", 3);
            return 3;
        }
        if (std::isinf(x)) {
            std::memcpy(o, x < 0 ? "-Inf" : "+Inf", 4);
            return 4;
        }
        if (std::signbit(x)) {
            *o++ = '-';
            x = -x;
        }
        if (x == 0) {
            *o++ = '0';
            return size_t(o - out);
        }
        Digits g = shortest_digits(x);
        int e = g.point - 1;
        if (e < -4 || e >= 6) {
            o = put_exponent_form(o, g, true);
        } else {
            o = put_fixed_form(o, g);
        }
        return size_t(o - out);
    }

    // x as json.Marshal writes a float64 (x finite): fixed from 1e-6 to
    // below 1e21, the exponent form outside it with the exponent's
    // leading zero dropped (1e-7, 1e+21)
    inline size_t float_json(char* out, double x) noexcept {
        char* o = out;
        if (std::signbit(x)) {
            *o++ = '-';
            x = -x;
        }
        if (x == 0) {
            *o++ = '0';
            return size_t(o - out);
        }
        Digits g = shortest_digits(x);
        if (x < 1e-6 || x >= 1e21) {
            o = put_exponent_form(o, g, false);
        } else {
            o = put_fixed_form(o, g);
        }
        return size_t(o - out);
    }

    // The integer u / 10^digits and its fraction of `digits` places, the
    // trailing zeros of the fraction dropped (and the point, when nothing
    // is left of it)
    inline char* put_decimal(char* o, uint64_t u, int digits) noexcept {
        uint64_t scale = 1;
        for (int i = 0; i < digits; ++i) {
            scale *= 10;
        }
        uint64_t whole = u / scale;
        uint64_t fraction = u % scale;
        char buf[20];
        int k = 0;
        do {
            buf[k++] = char('0' + whole % 10);
            whole /= 10;
        } while (whole);
        while (k) {
            *o++ = buf[--k];
        }
        if (fraction) {
            while (fraction % 10 == 0) {
                fraction /= 10;
                --digits;
            }
            *o++ = '.';
            for (int i = digits - 1; i >= 0; --i) {
                o[i] = char('0' + fraction % 10);
                fraction /= 10;
            }
            o += digits;
        }
        return o;
    }

    inline constexpr size_t DurationTextSize = 32;

    // A duration as Go's Duration.String writes it: "0s", "1ns", "1.5µs",
    // "1.5ms", "1.5s", "1h30m0s"
    inline size_t duration_text(char* out, int64_t ns) noexcept {
        char* o = out;
        if (ns == 0) {
            o[0] = '0';
            o[1] = 's';
            return 2;
        }
        if (ns < 0) {
            *o++ = '-';
        }
        uint64_t u = ns < 0 ? uint64_t(0) - uint64_t(ns) : uint64_t(ns);
        if (u < 1000) {
            o = put_decimal(o, u, 0);
            *o++ = 'n';
        } else if (u < 1000000) {
            o = put_decimal(o, u, 3);
            *o++ = '\xC2';
            *o++ = '\xB5';
        } else if (u < 1000000000) {
            o = put_decimal(o, u, 6);
            *o++ = 'm';
        } else {
            uint64_t seconds = u / 1000000000;
            uint64_t hours = seconds / 3600;
            uint64_t minutes = seconds / 60 % 60;
            if (hours) {
                o = put_decimal(o, hours, 0);
                *o++ = 'h';
            }
            if (hours || minutes) {
                o = put_decimal(o, minutes, 0);
                *o++ = 'm';
            }
            o = put_decimal(o, (seconds % 60) * 1000000000 + u % 1000000000, 9);
        }
        *o++ = 's';
        return size_t(o - out);
    }

    inline void put_digits(char* o, uint64_t v, int width) noexcept {
        for (int i = width - 1; i >= 0; --i) {
            o[i] = char('0' + v % 10);
            v /= 10;
        }
    }

    // The civil date of a day counted from 1970-01-01 (the proleptic
    // Gregorian calendar, by eras of 400 years)
    inline void civil_of(int64_t days, int& year, unsigned& month, unsigned& day) noexcept {
        days += 719468;
        const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
        const unsigned doe = unsigned(days - era * 146097);
        const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const unsigned mp = (5 * doy + 2) / 153;
        day = doy - (153 * mp + 2) / 5 + 1;
        month = mp < 10 ? mp + 3 : mp - 9;
        year = int(int64_t(yoe) + era * 400 + (month <= 2));
    }

    SGCL_INLINE_HOT int64_t floor_div(int64_t a, int64_t b) noexcept {
        int64_t q = a / b;
        return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
    }

    // "2026-09-28T14:05:01" of the wall clock at `offset` seconds east of
    // UTC: 19 bytes. A datetime of the library lies between 1677 and 2262,
    // so the year has four digits
    inline void put_wall(char* o, int64_t ns, int32_t offset) noexcept {
        int64_t s = floor_div(ns, 1000000000) + offset;
        int64_t days = floor_div(s, 86400);
        int64_t sod = s - days * 86400;
        int year;
        unsigned month, day;
        civil_of(days, year, month, day);
        put_digits(o, uint64_t(year), 4);
        o[4] = '-';
        put_digits(o + 5, month, 2);
        o[7] = '-';
        put_digits(o + 8, day, 2);
        o[10] = 'T';
        put_digits(o + 11, uint64_t(sod / 3600), 2);
        o[13] = ':';
        put_digits(o + 14, uint64_t(sod / 60 % 60), 2);
        o[16] = ':';
        put_digits(o + 17, uint64_t(sod % 60), 2);
    }

    // "Z", or "+02:00"
    inline char* put_offset(char* o, int32_t offset) noexcept {
        if (offset == 0) {
            *o++ = 'Z';
            return o;
        }
        *o++ = offset < 0 ? '-' : '+';
        int32_t a = offset < 0 ? -offset : offset;
        put_digits(o, uint64_t(a / 3600), 2);
        o[2] = ':';
        put_digits(o + 3, uint64_t(a / 60 % 60), 2);
        return o + 5;
    }

    inline constexpr size_t TimeTextSize = 40;

    // The time of the text handler: RFC 3339 with exactly three places
    // of the second, truncated ("2026-09-28T14:05:01.123+02:00")
    inline size_t time_text(char* out, int64_t ns, int32_t offset) noexcept {
        put_wall(out, ns, offset);
        char* o = out + 19;
        int64_t fraction = ns - floor_div(ns, 1000000000) * 1000000000;
        *o++ = '.';
        put_digits(o, uint64_t(fraction / 1000000), 3);
        o += 3;
        o = put_offset(o, offset);
        return size_t(o - out);
    }

    // The time of the JSON handler: RFC 3339 with the nanoseconds, the
    // trailing zeros dropped (Go's RFC3339Nano)
    inline size_t time_json(char* out, int64_t ns, int32_t offset) noexcept {
        put_wall(out, ns, offset);
        char* o = out + 19;
        int64_t fraction = ns - floor_div(ns, 1000000000) * 1000000000;
        if (fraction) {
            int digits = 9;
            while (fraction % 10 == 0) {
                fraction /= 10;
                --digits;
            }
            *o++ = '.';
            put_digits(o, uint64_t(fraction), digits);
            o += digits;
        }
        o = put_offset(o, offset);
        return size_t(o - out);
    }
}
