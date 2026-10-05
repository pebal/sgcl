//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../../core/aliases.h"
#include "../../../core/detail/bytes.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The grammar of RFC 9051 §9 both sides share: the classes of characters,
// a lexer over a response or a command whose literals are already in the
// buffer ("{n}\r\n" and the n bytes after it, as they came), the writing
// of a string as an atom, a quoted string or a literal, the modified
// UTF-7 of mailbox names (RFC 3501 §5.1.3), the date-time of APPEND and
// INTERNALDATE, the date of SEARCH, and the case-insensitive comparisons
// of atoms. Nothing here allocates on the managed heap: the scratch is
// std::string, made a string by the callers once.
namespace sgcl::net::imap::detail {
    // atom-specials: "(" ")" "{" SP CTL "%" "*" DQUOTE "\" "]"; the bytes
    // past 127 are not atom characters either (UTF-8 goes quoted)
    SGCL_INLINE_HOT constexpr bool atom_char(unsigned char c) noexcept {
        return c > 0x20 && c < 0x7f && c != '(' && c != ')' && c != '{' && c != '%' && c != '*' && c != '"' && c != '\\' && c != ']';
    }

    SGCL_INLINE_HOT constexpr bool astring_char(unsigned char c) noexcept {
        return atom_char(c) || c == ']';
    }

    // list-mailbox: an atom character, a wildcard or "]"
    SGCL_INLINE_HOT constexpr bool list_char(unsigned char c) noexcept {
        return astring_char(c) || c == '%' || c == '*';
    }

    SGCL_INLINE_HOT constexpr bool tag_char(unsigned char c) noexcept {
        return astring_char(c) && c != '+';
    }

    SGCL_INLINE_HOT constexpr char upper(char c) noexcept {
        return c >= 'a' && c <= 'z' ? char(c - 32) : c;
    }

    SGCL_INLINE_HOT constexpr char lower(char c) noexcept {
        return c >= 'A' && c <= 'Z' ? char(c + 32) : c;
    }

    inline bool iequal(std::string_view a, std::string_view b) noexcept {
        if (a.size() != b.size()) {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i) {
            if (upper(a[i]) != upper(b[i])) {
                return false;
            }
        }
        return true;
    }

    inline bool istarts(std::string_view s, std::string_view prefix) noexcept {
        return s.size() >= prefix.size() && iequal(s.substr(0, prefix.size()), prefix);
    }

    inline std::string to_upper(std::string_view s) {
        std::string out(s);
        for (auto& c : out) {
            c = upper(c);
        }
        return out;
    }

    inline std::string to_lower(std::string_view s) {
        std::string out(s);
        for (auto& c : out) {
            c = lower(c);
        }
        return out;
    }

    // INBOX in any case is INBOX (RFC 9051 §5.1); "INBOX/x" keeps the case
    // of the rest
    inline std::string canonical_mailbox(std::string_view name, char delimiter) {
        if (iequal(name, "INBOX")) {
            return "INBOX";
        }
        if (name.size() > 5 && iequal(name.substr(0, 5), "INBOX") && name[5] == delimiter) {
            return "INBOX" + std::string(name.substr(5));
        }
        return std::string(name);
    }

    // Whether the bytes are UTF-8 (no overlong forms, no surrogates)
    inline bool valid_utf8(std::string_view s) noexcept {
        size_t i = 0;
        const size_t n = s.size();
        while (i < n) {
            const unsigned char c = (unsigned char)s[i];
            if (c < 0x80) {
                ++i;
                continue;
            }
            size_t len;
            uint32_t cp;
            if ((c & 0xE0) == 0xC0) {
                len = 2;
                cp = c & 0x1F;
            } else if ((c & 0xF0) == 0xE0) {
                len = 3;
                cp = c & 0x0F;
            } else if ((c & 0xF8) == 0xF0) {
                len = 4;
                cp = c & 0x07;
            } else {
                return false;
            }
            if (i + len > n) {
                return false;
            }
            for (size_t k = 1; k < len; ++k) {
                const unsigned char d = (unsigned char)s[i + k];
                if ((d & 0xC0) != 0x80) {
                    return false;
                }
                cp = (cp << 6) | (d & 0x3F);
            }
            if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF)) {
                return false;
            }
            i += len;
        }
        return true;
    }

    // --- modified UTF-7 (RFC 3501 §5.1.3) -------------------------------

    inline constexpr char Utf7Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,";

    SGCL_INLINE_HOT constexpr int utf7_value(char c) noexcept {
        if (c >= 'A' && c <= 'Z') {
            return c - 'A';
        }
        if (c >= 'a' && c <= 'z') {
            return c - 'a' + 26;
        }
        if (c >= '0' && c <= '9') {
            return c - '0' + 52;
        }
        if (c == '+') {
            return 62;
        }
        if (c == ',') {
            return 63;
        }
        return -1;
    }

    // A UTF-8 name in modified UTF-7: printable ASCII as it is ("&" as
    // "&-"), every other run as "&" + base64 of its UTF-16BE + "-"; the
    // bytes of invalid UTF-8 taken as U+FFFD
    inline std::string utf7_encode(std::string_view s) {
        std::string out;
        out.reserve(s.size());
        size_t i = 0;
        const size_t n = s.size();
        while (i < n) {
            const unsigned char c = (unsigned char)s[i];
            if (c >= 0x20 && c <= 0x7e) {
                out += char(c);
                if (c == '&') {
                    out += '-';
                }
                ++i;
                continue;
            }
            // a run of characters outside printable ASCII
            std::vector<uint16_t> units;
            while (i < n) {
                const unsigned char d = (unsigned char)s[i];
                if (d >= 0x20 && d <= 0x7e) {
                    break;
                }
                uint32_t cp = 0xFFFD;
                size_t len = 1;
                if (d < 0x80) {
                    cp = d;
                } else if ((d & 0xE0) == 0xC0 && i + 1 < n) {
                    len = 2;
                } else if ((d & 0xF0) == 0xE0 && i + 2 < n) {
                    len = 3;
                } else if ((d & 0xF8) == 0xF0 && i + 3 < n) {
                    len = 4;
                }
                if (len > 1) {
                    if (valid_utf8(s.substr(i, len))) {
                        cp = d & (len == 2 ? 0x1F : len == 3 ? 0x0F : 0x07);
                        for (size_t k = 1; k < len; ++k) {
                            cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
                        }
                    } else {
                        len = 1;
                    }
                }
                i += len;
                if (cp >= 0x10000) {
                    cp -= 0x10000;
                    units.push_back(uint16_t(0xD800 + (cp >> 10)));
                    units.push_back(uint16_t(0xDC00 + (cp & 0x3FF)));
                } else {
                    units.push_back(uint16_t(cp));
                }
            }
            out += '&';
            uint32_t bits = 0;
            int nbits = 0;
            for (uint16_t u : units) {
                bits = (bits << 16) | u;
                nbits += 16;
                while (nbits >= 6) {
                    nbits -= 6;
                    out += Utf7Alphabet[(bits >> nbits) & 0x3F];
                }
            }
            if (nbits > 0) {
                out += Utf7Alphabet[(bits << (6 - nbits)) & 0x3F];
            }
            out += '-';
        }
        return out;
    }

    // A modified UTF-7 name as UTF-8; false for one RFC 3501 does not
    // allow (a printable character or "&" encoded, a run cut short, bits
    // left over, a lone surrogate, a byte outside printable ASCII)
    inline bool utf7_decode(std::string_view s, std::string& out, bool lenient = false) {
        out.clear();
        out.reserve(s.size());
        size_t i = 0;
        const size_t n = s.size();
        size_t shift_end = SIZE_MAX;   // where the last base64 shift ended: another may not begin there (one form per name)
        while (i < n) {
            const unsigned char c = (unsigned char)s[i];
            if (c < 0x20 || c > 0x7e) {
                return false;
            }
            if (c != '&') {
                out += char(c);
                ++i;
                continue;
            }
            const bool after_shift = i == shift_end;
            ++i;
            if (i < n && s[i] == '-') {
                out += '&';
                ++i;
                continue;
            }
            if (after_shift && !lenient) {
                return false;
            }
            uint32_t bits = 0;
            int nbits = 0;
            uint32_t high = 0;
            bool any = false;
            for (;;) {
                if (i >= n) {
                    return false;
                }
                const char d = s[i++];
                if (d == '-') {
                    break;
                }
                const int v = utf7_value(d);
                if (v < 0) {
                    return false;
                }
                bits = (bits << 6) | uint32_t(v);
                nbits += 6;
                if (nbits >= 16) {
                    nbits -= 16;
                    const uint32_t u = (bits >> nbits) & 0xFFFF;
                    bits &= (1u << nbits) - 1;
                    any = true;
                    uint32_t cp;
                    if (high) {
                        if (u < 0xDC00 || u > 0xDFFF) {
                            return false;
                        }
                        cp = 0x10000 + ((high - 0xD800) << 10) + (u - 0xDC00);
                        high = 0;
                    } else if (u >= 0xD800 && u <= 0xDBFF) {
                        high = u;
                        continue;
                    } else if (u >= 0xDC00 && u <= 0xDFFF) {
                        return false;
                    } else {
                        cp = u;
                    }
                    if (cp >= 0x20 && cp <= 0x7e && !lenient) {
                        return false;   // printable ASCII is never encoded
                    }
                    if (cp < 0x80) {
                        out += char(cp);
                    } else if (cp < 0x800) {
                        out += char(0xC0 | (cp >> 6));
                        out += char(0x80 | (cp & 0x3F));
                    } else if (cp < 0x10000) {
                        out += char(0xE0 | (cp >> 12));
                        out += char(0x80 | ((cp >> 6) & 0x3F));
                        out += char(0x80 | (cp & 0x3F));
                    } else {
                        out += char(0xF0 | (cp >> 18));
                        out += char(0x80 | ((cp >> 12) & 0x3F));
                        out += char(0x80 | ((cp >> 6) & 0x3F));
                        out += char(0x80 | (cp & 0x3F));
                    }
                }
            }
            if (!any || high || nbits >= 6 || bits != 0) {
                return false;
            }
            shift_end = i;
        }
        return true;
    }

    // --- dates ------------------------------------------------------------

    inline constexpr const char* MonthNames[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    SGCL_INLINE_HOT constexpr int64_t days_from_civil(int64_t y, unsigned m, unsigned d) noexcept {
        y -= m <= 2;
        const int64_t era = (y >= 0 ? y : y - 399) / 400;
        const unsigned yoe = unsigned(y - era * 400);
        const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + int64_t(doe) - 719468;
    }

    struct Civil {
        int64_t year;
        unsigned month;
        unsigned day;
    };

    SGCL_INLINE_HOT constexpr Civil civil_from_days(int64_t z) noexcept {
        z += 719468;
        const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        const unsigned doe = unsigned(z - era * 146097);
        const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const int64_t y = int64_t(yoe) + era * 400;
        const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const unsigned mp = (5 * doy + 2) / 153;
        const unsigned d = doy - (153 * mp + 2) / 5 + 1;
        const unsigned m = mp + (mp < 10 ? 3 : -9);
        return Civil{y + (m <= 2), m, d};
    }

    inline int month_of(std::string_view s) noexcept {
        if (s.size() != 3) {
            return 0;
        }
        for (int i = 0; i < 12; ++i) {
            if (iequal(s, MonthNames[i])) {
                return i + 1;
            }
        }
        return 0;
    }

    // An instant and the offset of its zone in minutes
    struct DateTime {
        int64_t unix = 0;
        int offset = 0;   // minutes east of UTC
    };

    // date-time: DQUOTE date-day-fixed "-" date-month "-" date-year SP
    // time SP zone DQUOTE, written without the quotes
    inline std::string format_date_time(DateTime t) {
        const int64_t local = t.unix + int64_t(t.offset) * 60;
        int64_t days = local / 86400;
        int64_t secs = local % 86400;
        if (secs < 0) {
            secs += 86400;
            --days;
        }
        const Civil c = civil_from_days(days);
        char buf[40];
        const int off = t.offset < 0 ? -t.offset : t.offset;
        const int year = int(c.year < 0 ? 0 : c.year > 9999 ? 9999 : c.year);
        std::snprintf(buf, sizeof(buf), "%2u-%s-%04d %02d:%02d:%02d %c%02d%02d", c.day, MonthNames[c.month - 1], year, int(secs / 3600), int(secs / 60 % 60),
                      int(secs % 60), t.offset < 0 ? '-' : '+', off / 60, off % 60);
        return buf;
    }

    // The text of a date-time without its quotes ("17-Jul-1996 02:44:25
    // -0700", the day's leading space or digit either way); false for one
    // that is not
    inline bool parse_date_time(std::string_view s, DateTime& out) noexcept {
        auto digits = [&](size_t at, size_t n, int& v) {
            if (at + n > s.size()) {
                return false;
            }
            v = 0;
            for (size_t i = 0; i < n; ++i) {
                const char c = s[at + i];
                if (c < '0' || c > '9') {
                    return false;
                }
                v = v * 10 + (c - '0');
            }
            return true;
        };
        size_t p = 0;
        int day = 0;
        if (s.size() >= 2 && s[0] == ' ') {
            if (!digits(1, 1, day)) {
                return false;
            }
            p = 2;
        } else if (s.size() >= 2 && s[1] == '-') {
            if (!digits(0, 1, day)) {
                return false;
            }
            p = 1;
        } else {
            if (!digits(0, 2, day)) {
                return false;
            }
            p = 2;
        }
        if (p + 5 > s.size() || s[p] != '-' || s[p + 4] != '-') {
            return false;
        }
        const int month = month_of(s.substr(p + 1, 3));
        p += 5;
        int year, hh, mm, ss, zh, zm;
        if (!month || !digits(p, 4, year) || p + 4 >= s.size() || s[p + 4] != ' ') {
            return false;
        }
        p += 5;
        if (!digits(p, 2, hh) || s.size() < p + 8 || s[p + 2] != ':' || !digits(p + 3, 2, mm) || s[p + 5] != ':' || !digits(p + 6, 2, ss)) {
            return false;
        }
        p += 8;
        if (s.size() != p + 6 || s[p] != ' ' || (s[p + 1] != '+' && s[p + 1] != '-') || !digits(p + 2, 2, zh) || !digits(p + 4, 2, zm)) {
            return false;
        }
        if (day < 1 || day > 31 || hh > 23 || mm > 59 || ss > 60 || zh > 23 || zm > 59) {
            return false;   // a zone of a day or more is no zone (time::zone::fixed refuses it)
        }
        const int offset = (zh * 60 + zm) * (s[p + 1] == '-' ? -1 : 1);
        out.offset = offset;
        out.unix = days_from_civil(year, unsigned(month), unsigned(day)) * 86400 + hh * 3600 + mm * 60 + (ss > 59 ? 59 : ss) - int64_t(offset) * 60;
        return true;
    }

    // date (SEARCH): date-day "-" date-month "-" date-year, the day of one
    // or two digits; the days since 1970
    inline bool parse_date(std::string_view s, int64_t& days) noexcept {
        const size_t a = s.find('-');
        if (a == std::string_view::npos || a == 0 || a > 2 || a + 4 >= s.size() || s[a + 4] != '-' || s.size() != a + 9) {
            return false;
        }
        int day = 0;
        for (size_t i = 0; i < a; ++i) {
            if (s[i] < '0' || s[i] > '9') {
                return false;
            }
            day = day * 10 + (s[i] - '0');
        }
        const int month = month_of(s.substr(a + 1, 3));
        int year = 0;
        for (size_t i = a + 5; i < s.size(); ++i) {
            if (s[i] < '0' || s[i] > '9') {
                return false;
            }
            year = year * 10 + (s[i] - '0');
        }
        if (!month || day < 1 || day > 31) {
            return false;
        }
        days = days_from_civil(year, unsigned(month), unsigned(day));
        return true;
    }

    inline std::string format_date(int64_t days) {
        const Civil c = civil_from_days(days);
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%u-%s-%04d", c.day, MonthNames[c.month - 1], int(c.year < 0 ? 0 : c.year > 9999 ? 9999 : c.year));
        return buf;
    }

    // --- writing ----------------------------------------------------------

    // How a string goes out: an atom, a quoted string, or a literal (the
    // last for CR, LF, NUL, or 8-bit text the peer has not allowed)
    enum class StringForm : uint8_t { atom, quoted, literal };

    inline StringForm form_of(std::string_view s, bool utf8, bool allow_atom) noexcept {
        if (s.size() > 4000) {
            return StringForm::literal;
        }
        bool atom = allow_atom && !s.empty() && !iequal(s, "NIL");
        bool eight = false;
        for (unsigned char c : s) {
            if (c == 0 || c == '\r' || c == '\n') {
                return StringForm::literal;
            }
            if (c >= 0x80) {
                eight = true;
                atom = false;
            } else if (!astring_char(c)) {
                atom = false;
            }
        }
        if (eight && (!utf8 || !valid_utf8(s))) {
            return StringForm::literal;
        }
        return atom ? StringForm::atom : StringForm::quoted;
    }

    inline void put_quoted(std::string& out, std::string_view s) {
        out += '"';
        for (char c : s) {
            if (c == '"' || c == '\\') {
                out += '\\';
            }
            out += c;
        }
        out += '"';
    }

    // A literal's head and its bytes: "{n}\r\n" (or "{n+}\r\n" when the
    // peer takes it without a continuation, "~{n}\r\n" for literal8)
    inline void put_literal(std::string& out, std::string_view s, bool plus, bool eight = false) {
        if (eight) {
            out += '~';
        }
        out += '{';
        out += std::to_string(s.size());
        if (plus) {
            out += '+';
        }
        out += "}\r\n";
        out.append(s.data(), s.size());
    }

    // A server's string (nstring, string): quoted when it can be, else a
    // literal, which a server never waits for
    inline void put_string(std::string& out, std::string_view s, bool utf8) {
        if (form_of(s, utf8, false) == StringForm::literal) {
            put_literal(out, s, false);
        } else {
            put_quoted(out, s);
        }
    }

    inline void put_nstring(std::string& out, const std::string* s, bool utf8) {
        if (!s) {
            out += "NIL";
        } else {
            put_string(out, *s, utf8);
        }
    }

    inline void put_astring(std::string& out, std::string_view s, bool utf8) {
        switch (form_of(s, utf8, true)) {
            case StringForm::atom: out.append(s.data(), s.size()); break;
            case StringForm::quoted: put_quoted(out, s); break;
            case StringForm::literal: put_literal(out, s, false); break;
        }
    }

    inline void put_number(std::string& out, uint64_t n) {
        char buf[24];
        char* e = buf + sizeof(buf);
        char* p = e;
        do {
            *--p = char('0' + n % 10);
            n /= 10;
        } while (n);
        out.append(p, size_t(e - p));
    }

    // --- the lexer --------------------------------------------------------

    // Over one response or command whose literals are inline: "{n}\r\n"
    // followed by the n bytes (as the reader assembled it, CRLF of the
    // last line taken off). Each function returns false, leaving the
    // position anywhere, for input of another shape: the caller gives up
    // on the whole response or command.
    class Lexer {
    public:
        SGCL_INLINE_HOT explicit Lexer(std::string_view s) noexcept
        : _s(s) {
        }

        SGCL_INLINE_HOT bool at_end() const noexcept {
            return _p >= _s.size();
        }

        SGCL_INLINE_HOT size_t position() const noexcept {
            return _p;
        }

        SGCL_INLINE_HOT void seek(size_t p) noexcept {
            _p = p;
        }

        SGCL_INLINE_HOT std::string_view rest() const noexcept {
            return _p < _s.size() ? _s.substr(_p) : std::string_view();
        }

        SGCL_INLINE_HOT std::string_view source() const noexcept {
            return _s;
        }

        SGCL_INLINE_HOT char peek() const noexcept {
            return _p < _s.size() ? _s[_p] : '\0';
        }

        // Whether a quoted string or a literal (literal8's "~{") begins here
        SGCL_INLINE_HOT bool at_string() const noexcept {
            const char c = peek();
            return c == '"' || c == '{' || (c == '~' && _p + 1 < _s.size() && _s[_p + 1] == '{');
        }

        SGCL_INLINE_HOT bool eat(char c) noexcept {
            if (_p < _s.size() && _s[_p] == c) {
                ++_p;
                return true;
            }
            return false;
        }

        // One space; a lenient reader (a client's) takes several
        SGCL_INLINE_HOT bool sp() noexcept {
            if (!eat(' ')) {
                return false;
            }
            if (_lenient) {
                while (eat(' ')) {
                }
            }
            return true;
        }

        SGCL_INLINE_HOT void set_lenient(bool on) noexcept {
            _lenient = on;
        }

        // A word, in any case, ending where an atom does
        bool word(std::string_view w) noexcept {
            if (_s.size() - _p < w.size() || !iequal(_s.substr(_p, w.size()), w)) {
                return false;
            }
            if (_p + w.size() < _s.size() && atom_char((unsigned char)_s[_p + w.size()]) && atom_char((unsigned char)w.back())) {
                return false;
            }
            _p += w.size();
            return true;
        }

        // A run of characters taken by the predicate (at least one)
        template<class Pred>
        std::string_view run(Pred pred) noexcept {
            const size_t start = _p;
            while (_p < _s.size() && pred((unsigned char)_s[_p])) {
                ++_p;
            }
            return _s.substr(start, _p - start);
        }

        std::string_view atom() noexcept {
            return run(atom_char);
        }

        // An atom that may hold "]" (astring's, a flag's "\Seen" is an atom
        // with a backslash, flag() below)
        std::string_view astring_atom() noexcept {
            return run(astring_char);
        }

        // A flag: "\" atom or atom (and "\*" in PERMANENTFLAGS)
        bool flag(std::string_view& out) noexcept {
            const size_t start = _p;
            if (eat('\\')) {
                if (eat('*')) {
                    out = _s.substr(start, 2);
                    return true;
                }
            }
            if (atom().empty()) {
                return false;
            }
            out = _s.substr(start, _p - start);
            return true;
        }

        bool number(uint64_t& n, uint64_t max = UINT64_MAX) noexcept {
            const size_t start = _p;
            n = 0;
            while (_p < _s.size() && _s[_p] >= '0' && _s[_p] <= '9') {
                const uint64_t d = uint64_t(_s[_p] - '0');
                if (n > (max - d) / 10) {
                    return false;
                }
                n = n * 10 + d;
                ++_p;
            }
            return _p > start;
        }

        bool number32(uint32_t& n) noexcept {
            uint64_t v;
            if (!number(v, UINT32_MAX)) {
                return false;
            }
            n = uint32_t(v);
            return true;
        }

        bool nz_number(uint32_t& n) noexcept {
            return peek() != '0' && number32(n) && n != 0;
        }

        // A quoted string's or a literal's bytes; a quoted string's escapes
        // copied out, a literal's left in place (out points at them)
        bool string(std::string_view& out, std::string& scratch, bool literal8 = false) noexcept {
            if (eat('"')) {
                const size_t start = _p;
                bool escaped = false;
                while (_p < _s.size()) {
                    const char c = _s[_p];
                    if (c == '"') {
                        if (!escaped) {
                            out = _s.substr(start, _p - start);
                            ++_p;
                            return true;
                        }
                        // an escape inside: the slow road
                        break;
                    }
                    if (c == '\\' || c == '\r' || c == '\n' || c == '\0') {
                        escaped = true;
                        break;
                    }
                    ++_p;
                }
                _p = start;
                scratch.clear();
                while (_p < _s.size()) {
                    char c = _s[_p++];
                    if (c == '"') {
                        out = scratch;
                        return true;
                    }
                    if (c == '\r' || c == '\n' || c == '\0') {
                        return false;
                    }
                    if (c == '\\') {
                        if (_p >= _s.size()) {
                            return false;
                        }
                        c = _s[_p++];
                        if (c != '"' && c != '\\') {
                            return false;
                        }
                    }
                    scratch += c;
                }
                return false;
            }
            return literal(out, literal8);
        }

        // "{n}\r\n" or "{n+}\r\n" (or "~{n}" where literal8 is taken), and
        // the n bytes
        bool literal(std::string_view& out, bool literal8 = false) noexcept {
            const size_t start = _p;
            if (literal8 && peek() == '~') {
                ++_p;
            }
            if (!eat('{')) {
                _p = start;
                return false;
            }
            uint64_t n;
            if (!number(n, UINT32_MAX)) {
                return false;
            }
            eat('+');
            if (!eat('}') || !eat('\r') || !eat('\n') || _s.size() - _p < n) {
                return false;
            }
            out = _s.substr(_p, size_t(n));
            _p += size_t(n);
            return true;
        }

        // astring: an atom with "]" or a string
        bool astring(std::string_view& out, std::string& scratch) noexcept {
            if (at_string()) {
                return string(out, scratch, peek() == '~');
            }
            out = astring_atom();
            return !out.empty();
        }

        // nstring: NIL (has false) or a string
        bool nstring(std::string_view& out, bool& has, std::string& scratch) noexcept {
            if (word("NIL")) {
                has = false;
                out = {};
                return true;
            }
            has = true;
            return string(out, scratch, true);
        }

        // A mailbox name of a LIST pattern: list-mailbox chars or a string
        bool list_mailbox(std::string_view& out, std::string& scratch) noexcept {
            if (at_string()) {
                return string(out, scratch);
            }
            out = run(list_char);
            return !out.empty();
        }

        // Past a parenthesized list or a string or an atom, whatever it
        // holds (an extension's data the reader does not take)
        bool skip_value(int depth = 0) noexcept {
            if (depth > 64) {
                return false;
            }
            std::string scratch;
            std::string_view v;
            const char c = peek();
            if (c == '(') {
                ++_p;
                bool first = true;
                while (!eat(')')) {
                    if (!first && !sp()) {
                        return false;
                    }
                    first = false;
                    if (peek() == ')') {
                        continue;
                    }
                    if (!skip_value(depth + 1)) {
                        return false;
                    }
                }
                return true;
            }
            if (at_string()) {
                return string(v, scratch, true);
            }
            if (c == '\\') {
                return flag(v);
            }
            // an atom, a number, a sequence set, "[...]" inside a section
            const size_t start = _p;
            int brackets = 0;
            while (_p < _s.size()) {
                const unsigned char d = (unsigned char)_s[_p];
                if (d == '[') {
                    ++brackets;
                } else if (d == ']') {
                    if (brackets == 0) {
                        break;
                    }
                    --brackets;
                } else if (brackets == 0 && (d == ' ' || d == '(' || d == ')')) {
                    break;
                } else if (d < 0x20 && d != '\t') {
                    break;
                }
                ++_p;
            }
            return _p > start;
        }

    private:
        std::string_view _s;
        size_t _p = 0;
        bool _lenient = false;
    };

    // The end of a literal's head at the end of a line ("{n}", "{n+}",
    // "~{n}"): its size and whether it is synchronizing; found false when
    // the line ends otherwise
    struct LiteralHead {
        uint64_t size = 0;
        bool sync = true;
        bool eight = false;
        bool found = false;
        bool overflow = false;
    };

    inline LiteralHead literal_at_end(std::string_view line) noexcept {
        LiteralHead h;
        if (line.empty() || line.back() != '}') {
            return h;
        }
        size_t i = line.size() - 1;
        if (i == 0) {
            return h;
        }
        --i;
        if (line[i] == '+') {
            h.sync = false;
            if (i == 0) {
                return h;
            }
            --i;
        }
        const size_t end = i + 1;
        while (i > 0 && line[i] >= '0' && line[i] <= '9') {
            --i;
        }
        if (line[i] != '{' || i + 1 == end) {
            return h;
        }
        uint64_t n = 0;
        for (size_t k = i + 1; k < end; ++k) {
            if (n > (UINT64_MAX - 9) / 10) {
                h.overflow = true;
                h.found = true;
                return h;
            }
            n = n * 10 + uint64_t(line[k] - '0');
        }
        h.size = n;
        h.found = true;
        h.eight = i > 0 && line[i - 1] == '~';
        return h;
    }
}
