//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/dynamic_array.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "currency.h"
#include "locale.h"
#include "number.h"
#include "plural.h"
#include "stencil.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// A message of ICU's MessageFormat, version 1 — the syntax ICU4C and ICU4J
// read and the translation tools write (Crowdin, Lokalise, Weblate,
// FormatJS) — read once for a locale and written many times:
//
//     txt::message_format m(u8"{n, plural, one {# plik} few {# pliki} other {# plików}} w {dir}",
//                           txt::locale("pl"));
//     m.format(txt::object{{"n", 5}, {"dir", "src"}});      // "5 plików w src"
//
// The arguments are a txt::value: an object for names, a list for numbers
// ({0} {1}). An argument is written by its type: none (a number in the
// locale's way, anything else as text), number (integer, percent, currency,
// a pattern of DecimalFormat such as "#,##0.00", or a number skeleton such
// as "::compact-short"), date and time (short, medium, long, full, a
// skeleton "::yMMMd" or a pattern; the value milliseconds since 1970 in
// UTC, or RFC 3339 text with its offset; written by sgcl/time, which
// registers itself when it is included, and as the value's text when it is
// not), plural and selectordinal (offset:, =n, the CLDR categories, # for
// the number), and select. Apostrophes as ICU's default reads them: '' is
// one, an apostrophe before { } (and # inside a plural) quotes up to the
// next one, any other is a letter.
//
// What ICU answers is what this answers, ICU 78 the oracle of the tests.
// Where it does not follow: the types that need ICU's rule-based number
// formats (spellout, ordinal, duration) and the deprecated choice are
// refused by parse; a value of the wrong type is written as its text rather
// than failing the call; an argument nobody gave is written {name}, as ICU
// writes it.
namespace sgcl::txt {
    class message_format;

    namespace detail {
        struct MessageData;
        struct MessageParser;

        // How sgcl/time writes the date and time arguments: a pattern
        // resolved once from the style (0 short ... 3 full, -1 a skeleton
        // or a pattern of the text), then a value written in it
        struct MessageDates {
            std::string (*resolve)(const locale& l, bool time, int style, std::string_view text);
            bool (*write)(std::string& out, const value& v, const locale& l, std::string_view pattern);
        };

        inline std::atomic<const MessageDates*>& message_dates() noexcept {
            static std::atomic<const MessageDates*> dates{nullptr};
            return dates;
        }
    }

    // Where a message stopped being one: the byte, and why
    class message_error {
    public:
        SGCL_INLINE_HOT message_error(size_t offset, const char* reason) noexcept
        : _offset(offset)
        , _reason(reason) {
        }

        SGCL_INLINE_HOT size_t offset() const noexcept {
            return _offset;
        }

        SGCL_INLINE_HOT string message() const noexcept {
            return string(_reason);
        }

    private:
        size_t _offset;
        const char* _reason;
    };

    class message_format {
    public:
        // The empty message: it writes nothing
        message_format() noexcept = default;

        // A message read from outside the program (a catalog, a file): the
        // message, or where and why it is not one
        static expected<message_format, message_error> parse(const string& pattern, const locale& l) noexcept;

        // The message a literal of the program spells: parse's value, or
        // bad_expected_access<message_error> (DESIGN 234)
        explicit message_format(const string& pattern, const locale& l)
        : message_format(parse(pattern, l).value()) {
        }

        // The message with the arguments: an object (by name) or a list
        // (by number)
        string format(const value& args) const noexcept;

        locale where() const noexcept;

        string pattern() const noexcept;

    private:
        tracked_ptr<const detail::MessageData> _data;

        friend struct detail::MessageParser;
    };

    // The one-line form: read and write once
    expected<string, message_error> format_message(const string& pattern, const locale& l, const value& args) noexcept;

    namespace detail {
        enum MfOp : uint8_t {
            MfText,    // a: offset, b: length into the text
            MfPound,   // # of the nearest plural
            MfArg,
        };

        enum MfType : uint8_t {
            MtNone, MtNumber, MtDate, MtTime, MtPlural, MtOrdinal, MtSelect,
        };

        struct MfNode {
            uint8_t op = MfText;
            uint8_t type = MtNone;
            uint32_t a = 0, b = 0;           // text, or the argument's name (offset, length)
            int64_t number = -1;             // a numbered argument
            uint32_t name = 0;               // the index of its name among the names
            uint32_t first = 0, count = 0;   // plural, select: the cases; number: its format; date: its pattern
            double offset = 0;
        };

        struct MfCase {
            bool exact = false;              // =value
            double value = 0;
            uint32_t key = 0, key_size = 0;  // the keyword, in the text
            uint32_t first = 0, count = 0;   // its message: nodes
        };

        // A number argument's format: the locale's way through number_format,
        // or a pattern of its own (prefix, suffix and the core in number_format)
        struct MfNumber {
            number_format format;
            number_format symbols;           // a pattern with ¤: the currency's format, for its symbol
            int scale = 0;                   // a power of ten the value is multiplied by
            bool own = false;                // a DecimalFormat pattern: the affixes below
            bool own_negative = false;
            std::string prefix, suffix, negative_prefix, negative_suffix;   // \1 minus \2 plus \3 percent \4 per mille \5 currency
            // a pattern with an exponent: the mantissa's integer digits
            // (fewest, most: more than the fewest groups the exponent by
            // the most), its fraction digits, the exponent's digits
            bool scientific = false;
            bool exponent_plus = false;
            int min_int = 1, max_int = 1, min_frac = 0, max_frac = 0, exponent_digits = 1;
        };

        // x rounded half to even to sig significant digits
        inline void mf_round(cldr::Decimal& x, int sig) noexcept {
            if (sig < 1) {
                sig = 1;
            }
            if (x.n <= sig) {
                return;
            }
            char next = x.d[sig];
            bool rest = x.sticky;
            for (int i = sig + 1; i < x.n && !rest; ++i) {
                rest = x.d[i] != '0';
            }
            bool up = next > '5' || (next == '5' && (rest || ((x.d[sig - 1] - '0') & 1)));
            x.n = sig;
            x.sticky = false;
            if (up) {
                int i = sig - 1;
                while (i >= 0 && x.d[i] == '9') {
                    x.d[i] = '0';
                    --i;
                }
                if (i < 0) {
                    x.d[0] = '1';
                    x.n = 1;
                    x.point += 1;
                } else {
                    ++x.d[i];
                }
            }
            while (x.n > 0 && x.d[x.n - 1] == '0') {
                --x.n;
            }
        }

        inline int mf_floor_div(int a, int b) noexcept {
            return a >= 0 ? a / b : -((-a + b - 1) / b);
        }

        struct MessageData {
            locale where;
            string pattern;
            std::string text;
            std::vector<MfNode> nodes;
            std::vector<MfCase> cases;
            std::vector<MfNumber> numbers;
            std::vector<std::string> dates;
            dynamic_array<string> names;     // the names of the arguments, for value::find
            uint32_t first = 0, count = 0;   // the message's own nodes
            number_format plain;             // the locale's decimal format: {n}, #
            number_format operands;          // the plain digits a plural rule reads (root, no grouping)
        };

        // Pattern_White_Space and Pattern_Syntax (UAX #31): what ends a name
        inline bool mf_white(char32_t c) noexcept {
            return (c >= 9 && c <= 13) || c == 0x20 || c == 0x85 || c == 0x200E || c == 0x200F || c == 0x2028
                || c == 0x2029;
        }

        inline bool mf_syntax(char32_t c) noexcept {
            if (c < 0x80) {
                return (c >= 0x21 && c <= 0x2F) || (c >= 0x3A && c <= 0x40) || (c >= 0x5B && c <= 0x5E) || c == 0x60
                    || (c >= 0x7B && c <= 0x7E);
            }
            return (c >= 0xA1 && c <= 0xA7) || c == 0xA9 || c == 0xAB || c == 0xAC || c == 0xAE || c == 0xB0
                || c == 0xB1 || c == 0xB6 || c == 0xBB || c == 0xBF || c == 0xD7 || c == 0xF7
                || (c >= 0x2010 && c <= 0x2027) || (c >= 0x2030 && c <= 0x203E) || (c >= 0x2041 && c <= 0x2053)
                || (c >= 0x2055 && c <= 0x205E) || (c >= 0x2190 && c <= 0x245F) || (c >= 0x2500 && c <= 0x2775)
                || (c >= 0x2794 && c <= 0x2BFF) || (c >= 0x2E00 && c <= 0x2E7F) || (c >= 0x3001 && c <= 0x3003)
                || (c >= 0x3008 && c <= 0x3020) || c == 0x3030 || c == 0xFD3E || c == 0xFD3F || c == 0xFE45
                || c == 0xFE46;
        }

        // the code point at i and its length (a bad byte is one of its own)
        inline char32_t mf_decode(std::string_view s, size_t i, size_t& len) noexcept {
            unsigned char c = (unsigned char)s[i];
            if (c < 0x80) {
                len = 1;
                return c;
            }
            int n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
            if (n == 1 || i + n > s.size()) {
                len = 1;
                return 0xFFFD;
            }
            char32_t v = c & (0x7F >> n);
            for (int k = 1; k < n; ++k) {
                unsigned char d = (unsigned char)s[i + k];
                if ((d & 0xC0) != 0x80) {
                    len = 1;
                    return 0xFFFD;
                }
                v = v << 6 | (d & 0x3F);
            }
            len = size_t(n);
            return v;
        }

        inline bool mf_same_word(std::string_view a, const char* b) noexcept {
            size_t n = std::char_traits<char>::length(b);
            if (a.size() != n) {
                return false;
            }
            for (size_t i = 0; i < n; ++i) {
                char c = a[i];
                if (c >= 'A' && c <= 'Z') {
                    c = char(c + 32);
                }
                if (c != b[i]) {
                    return false;
                }
            }
            return true;
        }

        // A number skeleton of ICU ("::currency/EUR compact-short .00"),
        // the stems that number_options holds; false on any other
        bool mf_skeleton(std::string_view text, const locale& l, MfNumber& out) noexcept;

        // A pattern of DecimalFormat ("#,##0.00", "0.###E0", "#%", "¤#,##0.00;(¤#)")
        bool mf_decimal_pattern(std::string_view text, const locale& l, MfNumber& out) noexcept;

        struct MessageParser {
            std::string_view s;
            const locale& l;
            MessageData& d;
            std::vector<std::string> names;
            size_t at = 0;
            size_t fail_at = 0;
            const char* fail = nullptr;

            static constexpr int MaxDepth = 64;

            static expected<message_format, message_error> make(std::string_view text, const string& keep,
                                                                const locale& l) noexcept;

            bool error(size_t where, const char* why) noexcept {
                if (!fail) {
                    fail_at = where;
                    fail = why;
                }
                return false;
            }

            void skip_white() noexcept {
                while (at < s.size()) {
                    size_t len;
                    char32_t c = mf_decode(s, at, len);
                    if (!mf_white(c)) {
                        break;
                    }
                    at += len;
                }
            }

            // an identifier: no white space, no syntax
            std::string_view identifier() noexcept {
                size_t b = at;
                while (at < s.size()) {
                    size_t len;
                    char32_t c = mf_decode(s, at, len);
                    if (mf_white(c) || mf_syntax(c)) {
                        break;
                    }
                    at += len;
                }
                return s.substr(b, at - b);
            }

            uint32_t add_text(std::string_view t) {
                uint32_t off = uint32_t(d.text.size());
                d.text.append(t);
                return off;
            }

            // literal text run into out, with the apostrophe rules
            void literal(std::string& out, char c) {
                out.push_back(c);
            }

            void flush(std::vector<MfNode>& nodes, std::string& run) {
                if (!run.empty()) {
                    MfNode n;
                    n.op = MfText;
                    n.a = add_text(run);
                    n.b = uint32_t(run.size());
                    nodes.push_back(n);
                    run.clear();
                }
            }

            // A message up to its closing brace (depth > 0) or the end; its
            // nodes appended after those of the messages inside it
            bool message(uint32_t& first, uint32_t& count, int depth, uint8_t parent) {
                if (depth > MaxDepth) {
                    return error(at, "messages nested too deep");
                }
                std::vector<MfNode> nodes;
                std::string run;
                bool plural_parent = parent == MtPlural || parent == MtOrdinal;
                while (at < s.size()) {
                    char c = s[at++];
                    if (c == '\'') {
                        if (at == s.size()) {
                            run.push_back('\'');
                        } else if (s[at] == '\'') {
                            run.push_back('\'');
                            ++at;
                        } else if (s[at] == '{' || s[at] == '}' || (plural_parent && s[at] == '#')) {
                            // quoted literal text up to the next lone apostrophe
                            while (true) {
                                size_t q = s.find('\'', at);
                                if (q == std::string_view::npos) {
                                    run.append(s.substr(at));
                                    at = s.size();
                                    break;
                                }
                                run.append(s.substr(at, q - at));
                                if (q + 1 < s.size() && s[q + 1] == '\'') {
                                    run.push_back('\'');
                                    at = q + 2;
                                } else {
                                    at = q + 1;
                                    break;
                                }
                            }
                        } else {
                            run.push_back('\'');
                        }
                    } else if (plural_parent && c == '#') {
                        flush(nodes, run);
                        MfNode n;
                        n.op = MfPound;
                        nodes.push_back(n);
                    } else if (c == '{') {
                        flush(nodes, run);
                        MfNode n;
                        if (!argument(n, at - 1, depth)) {
                            return false;
                        }
                        nodes.push_back(n);
                    } else if (c == '}' && depth > 0) {
                        flush(nodes, run);
                        first = uint32_t(d.nodes.size());
                        count = uint32_t(nodes.size());
                        d.nodes.insert(d.nodes.end(), nodes.begin(), nodes.end());
                        --at;   // the caller takes the brace
                        return true;
                    } else {
                        run.push_back(c);
                    }
                }
                if (depth > 0) {
                    return error(s.size(), "a message without its closing brace");
                }
                flush(nodes, run);
                first = uint32_t(d.nodes.size());
                count = uint32_t(nodes.size());
                d.nodes.insert(d.nodes.end(), nodes.begin(), nodes.end());
                return true;
            }

            // {name}, {name, type}, {name, type, style}; at is past the brace
            bool argument(MfNode& n, size_t open, int depth) {
                n.op = MfArg;
                skip_white();
                size_t name_at = at;
                if (at < s.size() && s[at] >= '0' && s[at] <= '9') {
                    int64_t v = 0;
                    size_t b = at;
                    while (at < s.size() && s[at] >= '0' && s[at] <= '9') {
                        v = v * 10 + (s[at] - '0');
                        if (v > INT32_MAX) {
                            return error(b, "an argument number too large");
                        }
                        ++at;
                    }
                    if (at - b > 1 && s[b] == '0') {
                        return error(b, "an argument number with a leading zero");
                    }
                    n.number = v;
                    if (at < s.size()) {
                        size_t len;
                        char32_t c = mf_decode(s, at, len);
                        if (!mf_white(c) && !mf_syntax(c)) {
                            return error(b, "an argument name that is not one");
                        }
                    }
                } else {
                    std::string_view id = identifier();
                    if (id.empty()) {
                        return error(name_at, "an argument without a name");
                    }
                }
                std::string_view name = s.substr(name_at, at - name_at);
                n.a = add_text(name);
                n.b = uint32_t(name.size());
                n.name = uint32_t(names.size());
                names.emplace_back(name);
                skip_white();
                if (at >= s.size()) {
                    return error(open, "an argument without its closing brace");
                }
                if (s[at] == '}') {
                    ++at;
                    n.type = MtNone;
                    return true;
                }
                if (s[at] != ',') {
                    return error(at, "a comma or a closing brace expected after the argument's name");
                }
                ++at;
                skip_white();
                size_t type_at = at;
                while (at < s.size() && ((s[at] >= 'a' && s[at] <= 'z') || (s[at] >= 'A' && s[at] <= 'Z'))) {
                    ++at;
                }
                std::string_view type = s.substr(type_at, at - type_at);
                skip_white();
                if (at >= s.size()) {
                    return error(open, "an argument without its closing brace");
                }
                if (type.empty()) {
                    return error(type_at, "an argument type expected");
                }
                if (mf_same_word(type, "plural") || mf_same_word(type, "selectordinal") || mf_same_word(type, "select")) {
                    n.type = mf_same_word(type, "plural") ? MtPlural : mf_same_word(type, "select") ? MtSelect
                                                                                                     : MtOrdinal;
                    if (s[at] != ',') {
                        return error(at, "a plural or select argument without its cases");
                    }
                    ++at;
                    return cases(n, depth);
                }
                bool number = mf_same_word(type, "number"), date = mf_same_word(type, "date"),
                     time = mf_same_word(type, "time");
                if (!number && !date && !time) {
                    if (mf_same_word(type, "spellout") || mf_same_word(type, "ordinal") || mf_same_word(type, "duration")
                        || mf_same_word(type, "choice")) {
                        return error(type_at, "an argument type this formatter does not have (spellout, ordinal, "
                                              "duration, choice)");
                    }
                    return error(type_at, "not an argument type");
                }
                n.type = number ? MtNumber : date ? MtDate : MtTime;
                std::string_view style;
                if (s[at] == ',') {
                    ++at;
                    // the style: up to the brace that closes the argument,
                    // braces nested and apostrophes quoting
                    size_t b = at;
                    int nest = 0;
                    while (true) {
                        if (at >= s.size()) {
                            return error(open, "an argument without its closing brace");
                        }
                        char c = s[at];
                        if (c == '\'') {
                            size_t q = s.find('\'', at + 1);
                            if (q == std::string_view::npos) {
                                return error(at, "a quoted style without its closing apostrophe");
                            }
                            at = q + 1;
                            continue;
                        }
                        if (c == '{') {
                            ++nest;
                        } else if (c == '}') {
                            if (nest == 0) {
                                break;
                            }
                            --nest;
                        }
                        ++at;
                    }
                    style = s.substr(b, at - b);
                } else if (s[at] != '}') {
                    return error(at, "a comma or a closing brace expected after the argument's type");
                }
                ++at;   // the closing brace
                if (number) {
                    return number_style(n, style, type_at);
                }
                return date_style(n, style, time, type_at);
            }

            // the style without the white space at its ends: what a
            // keyword or a skeleton is matched in (a pattern keeps it all,
            // its blanks being text, as ICU keeps them)
            static std::string_view trimmed(std::string_view t) noexcept {
                auto white = [](std::string_view v, size_t i, size_t& len) {
                    return mf_white(mf_decode(v, i, len));
                };
                size_t len;
                while (!t.empty() && white(t, 0, len)) {
                    t.remove_prefix(len);
                }
                while (!t.empty()) {
                    size_t k = t.size() - 1;
                    while (k > 0 && (static_cast<unsigned char>(t[k]) & 0xC0) == 0x80) {
                        --k;
                    }
                    if (!white(t, k, len)) {
                        break;
                    }
                    t.remove_suffix(t.size() - k);
                }
                return t;
            }

            bool number_style(MfNode& n, std::string_view raw, size_t where) {
                MfNumber f;
                std::string_view style = trimmed(raw);
                if (style.empty()) {
                    f.format = number_format(l);
                } else if (mf_same_word(style, "integer")) {
                    f.format = number_format(l, {.max_fraction = 0});
                } else if (mf_same_word(style, "percent")) {
                    f.format = number_format(l, {.style = number_style::percent});
                } else if (mf_same_word(style, "currency")) {
                    f.format = number_format(l, {.style = number_style::currency});
                } else if (style.size() >= 2 && style.substr(0, 2) == "::") {
                    if (!mf_skeleton(style.substr(2), l, f)) {
                        return error(where, "a number skeleton this formatter does not read");
                    }
                } else if (!mf_decimal_pattern(raw, l, f)) {
                    return error(where, "a number pattern this formatter does not read");
                }
                n.first = uint32_t(d.numbers.size());
                d.numbers.push_back(std::move(f));
                return true;
            }

            bool date_style(MfNode& n, std::string_view raw, bool time, size_t where) {
                std::string_view style = trimmed(raw);
                int k = style.empty() ? 1 : mf_same_word(style, "short") ? 0 : mf_same_word(style, "medium") ? 1
                    : mf_same_word(style, "long") ? 2 : mf_same_word(style, "full") ? 3 : -1;
                std::string text(k < 0 && style.substr(0, 2) == "::" ? style : raw);

                const MessageDates* dates = message_dates().load(std::memory_order_acquire);
                n.first = uint32_t(d.dates.size());
                if (dates) {
                    d.dates.push_back(dates->resolve(l, time, k, text));
                } else {
                    d.dates.emplace_back();
                }
                return true;
            }

            // the cases of a plural, selectordinal or select, up to the
            // argument's closing brace
            bool cases(MfNode& n, int depth) {
                bool plural = n.type != MtSelect;
                bool other = false, any = false;
                std::vector<MfCase> list;
                skip_white();
                if (plural && s.substr(at, 7) == "offset:") {
                    at += 7;
                    skip_white();
                    size_t b = at;
                    if (!read_number(n.offset)) {
                        return error(b, "offset: without a number");
                    }
                    skip_white();
                }
                while (true) {
                    skip_white();
                    if (at >= s.size()) {
                        return error(s.size(), "a plural or select argument without its closing brace");
                    }
                    if (s[at] == '}') {
                        if (!any) {
                            return error(at, "a plural or select argument without cases");
                        }
                        ++at;
                        break;
                    }
                    MfCase c;
                    size_t key_at = at;
                    if (plural && s[at] == '=') {
                        ++at;
                        size_t b = at;
                        if (!read_number(c.value)) {
                            return error(b, "= without a number");
                        }
                        c.exact = true;
                    } else {
                        if (plural && s.substr(at, 7) == "offset:") {
                            return error(at, "offset: after a case");
                        }
                        std::string_view key = identifier();
                        if (key.empty()) {
                            return error(key_at, "a case without its keyword");
                        }
                        other = other || key == "other";
                    }
                    std::string_view key = s.substr(key_at, at - key_at);
                    c.key = add_text(key);
                    c.key_size = uint32_t(key.size());
                    skip_white();
                    if (at >= s.size() || s[at] != '{') {
                        return error(at, "a case without its message in braces");
                    }
                    ++at;
                    if (!message(c.first, c.count, depth + 1, n.type)) {
                        return false;
                    }
                    ++at;   // the case's closing brace
                    list.push_back(c);
                    any = true;
                }
                if (!other) {
                    return error(at - 1, "a plural or select argument without its other case");
                }
                n.first = uint32_t(d.cases.size());
                n.count = uint32_t(list.size());
                d.cases.insert(d.cases.end(), list.begin(), list.end());
                return true;
            }

            // [+-]digits[.digits]: ICU's numbers of offset: and =
            bool read_number(double& out) noexcept {
                size_t b = at;
                if (at < s.size() && (s[at] == '-' || s[at] == '+')) {
                    ++at;
                }
                size_t digits = at;
                while (at < s.size() && s[at] >= '0' && s[at] <= '9') {
                    ++at;
                }
                if (at == digits) {
                    at = b;
                    return false;
                }
                if (at < s.size() && s[at] == '.') {
                    size_t f = ++at;
                    while (at < s.size() && s[at] >= '0' && s[at] <= '9') {
                        ++at;
                    }
                    if (at == f) {
                        at = b;
                        return false;
                    }
                }
                std::string t(s.substr(b, at - b));
                out = std::strtod(t.c_str(), nullptr);
                return true;
            }
        };

        //----------------------------------------------------------------
        // Writing
        //----------------------------------------------------------------
        struct MessageWriter {
            const MessageData& d;
            std::string& out;

            // the number a value holds: as decimal digits; false when none
            static bool decimal_of(const value& v, cldr::Decimal& x, double& as_double) noexcept {
                if (const long long* i = value_reach::integer_of(v)) {
                    x = cldr::decimal_of_integer(*i);
                    as_double = double(*i);
                    return true;
                }
                if (const double* r = value_reach::real_of(v)) {
                    x = cldr::decimal_of_double(*r);
                    as_double = *r;
                    return true;
                }
                if (const string* t = v.text()) {
                    if (cldr::decimal_of_text(t->view(), x)) {
                        as_double = std::strtod(std::string(t->view()).c_str(), nullptr);
                        return true;
                    }
                }
                return false;
            }

            void put_value_text(const value& v) {
                string t = v.to_string();
                out.append(t.view());
            }

            void put_missing(const MfNode& n) {
                out.push_back('{');
                out.append(d.text, n.a, n.b);
                out.push_back('}');
            }

            const value* find(const MfNode& n, const value& args) const noexcept {
                if (n.number >= 0 && args.kind() == value_kind::list) {
                    return args.at(size_t(n.number));
                }
                return args.find(d.names[n.name]);
            }

            void put_number(const MfNumber& f, cldr::Decimal x) {
                if (x.nan || x.infinite || !f.own) {
                    if (f.scale && !x.zero()) {
                        x.point += f.scale;
                    }
                    cldr::NumberAccess::append(f.format, x, out);
                    return;
                }
                bool negative = x.negative;
                x.negative = false;
                if (f.scale && !x.zero()) {
                    x.point += f.scale;
                }
                // a negative number is written with the negative pattern,
                // or the minus sign in front of the positive one
                const std::string& pre = negative && f.own_negative ? f.negative_prefix : f.prefix;
                const std::string& suf = negative && f.own_negative ? f.negative_suffix : f.suffix;
                if (negative && !f.own_negative) {
                    out.append(cldr::NumberAccess::symbol(f.format, cldr::SysMinus));
                }
                put_affix(f, pre, true);
                if (f.scientific) {
                    put_scientific(f, x);
                } else {
                    cldr::NumberAccess::append(f.format, x, out);
                }
                put_affix(f, suf, false);
            }

            // the mantissa and the exponent of a pattern's own, as
            // DecimalFormat writes them
            void put_scientific(const MfNumber& f, cldr::Decimal x) {
                int exponent = 0, min_shown = f.min_frac, int_digits = f.min_int < 1 ? 1 : f.min_int;
                bool grouped = f.max_int > f.min_int && f.max_int > 1;
                if (!x.zero()) {
                    if (grouped) {
                        mf_round(x, f.min_int + f.max_frac);
                        exponent = mf_floor_div(x.point - 1, f.max_int) * f.max_int;
                        int shown = x.point - exponent;
                        min_shown = f.min_int + f.min_frac - shown;
                        int_digits = 1;
                    } else {
                        mf_round(x, int_digits + f.max_frac);
                        exponent = x.point - int_digits;
                    }
                    x.point -= exponent;
                }
                number_format m(d.where, {.min_integer = int_digits, .min_fraction = min_shown < 0 ? 0 : min_shown,
                                          .max_fraction = 100, .grouping = false, .sign = sign_display::never});
                cldr::NumberAccess::append(m, x, out);
                out.append(cldr::NumberAccess::symbol(f.format, cldr::SysExponential));
                if (exponent < 0) {
                    out.append(cldr::NumberAccess::symbol(f.format, cldr::SysMinus));
                    exponent = -exponent;
                } else if (f.exponent_plus) {
                    out.append(cldr::NumberAccess::symbol(f.format, cldr::SysPlus));
                }
                number_format e(d.where, {.min_integer = f.exponent_digits, .grouping = false});
                cldr::NumberAccess::append(e, cldr::decimal_of_integer(int64_t(exponent)), out);
            }

            void put_affix(const MfNumber& f, const std::string& a, bool prefix) {
                for (size_t i = 0; i < a.size(); ++i) {
                    char c = a[i];
                    switch (c) {
                        case '\x01': out.append(cldr::NumberAccess::symbol(f.format, cldr::SysMinus)); break;
                        case '\x02': out.append(cldr::NumberAccess::symbol(f.format, cldr::SysPlus)); break;
                        case '\x03': out.append(cldr::NumberAccess::symbol(f.format, cldr::SysPercent)); break;
                        case '\x04': out.append(cldr::NumberAccess::symbol(f.format, cldr::SysPerMille)); break;
                        case '\x05':
                            cldr::NumberAccess::currency_symbol(f.symbols, out, prefix && i + 1 == a.size(),
                                                                !prefix && i == 0);
                            break;
                        default: out.push_back(c);
                    }
                }
            }

            // the plural category of a number minus the offset, read from
            // its digits as the locale's default format leaves them
            plural category(const cldr::Decimal& x, bool ordinal) const {
                std::string digits;
                cldr::NumberAccess::append(d.operands, x, digits);
                auto o = cldr::operands_of(std::string_view(digits));
                return ordinal ? cldr::ordinal(o, d.where) : cldr::cardinal(o, d.where);
            }

            static cldr::Decimal minus(const cldr::Decimal& x, double as_double, double offset) noexcept {
                if (offset == 0) {
                    return x;
                }
                return cldr::decimal_of_double(as_double - offset);
            }

            void write(uint32_t first, uint32_t count, const value& args, const cldr::Decimal* pound, int depth) {
                for (uint32_t i = first; i < first + count; ++i) {
                    const MfNode& n = d.nodes[i];
                    if (n.op == MfText) {
                        out.append(d.text, n.a, n.b);
                        continue;
                    }
                    if (n.op == MfPound) {
                        if (pound) {
                            cldr::NumberAccess::append(d.plain, *pound, out);
                        } else {
                            out.push_back('#');
                        }
                        continue;
                    }
                    const value* v = find(n, args);
                    if (!v) {
                        put_missing(n);
                        continue;
                    }
                    cldr::Decimal x;
                    double as_double = 0;
                    switch (n.type) {
                        case MtNone:
                            if (v->kind() == value_kind::integer || v->kind() == value_kind::real) {
                                decimal_of(*v, x, as_double);
                                cldr::NumberAccess::append(d.plain, x, out);
                            } else {
                                put_value_text(*v);
                            }
                            break;
                        case MtNumber:
                            if (decimal_of(*v, x, as_double)) {
                                put_number(d.numbers[n.first], x);
                            } else {
                                put_value_text(*v);
                            }
                            break;
                        case MtDate:
                        case MtTime: {
                            const MessageDates* dates = message_dates().load(std::memory_order_acquire);
                            if (!dates || d.dates[n.first].empty() || !dates->write(out, *v, d.where, d.dates[n.first])) {
                                put_value_text(*v);
                            }
                            break;
                        }
                        case MtSelect: {
                            string key = v->to_string();
                            const MfCase* chosen = nullptr;
                            const MfCase* other = nullptr;
                            for (uint32_t c = n.first; c < n.first + n.count; ++c) {
                                const MfCase& k = d.cases[c];
                                std::string_view kw(d.text.data() + k.key, k.key_size);
                                if (!chosen && kw == key.view()) {
                                    chosen = &k;
                                }
                                if (!other && kw == "other") {
                                    other = &k;
                                }
                            }
                            if (!chosen) {
                                chosen = other;
                            }
                            if (chosen && depth < 64) {
                                write(chosen->first, chosen->count, args, pound, depth + 1);
                            }
                            break;
                        }
                        default: {   // plural, selectordinal
                            bool number = decimal_of(*v, x, as_double);
                            const MfCase* chosen = nullptr;
                            const MfCase* other = nullptr;
                            cldr::Decimal shifted;
                            const char* keyword = "other";
                            if (number) {
                                shifted = minus(x, as_double, n.offset);
                                if (!x.nan) {
                                    keyword = plural_name(category(shifted, n.type == MtOrdinal));
                                }
                            }
                            for (uint32_t c = n.first; c < n.first + n.count; ++c) {
                                const MfCase& k = d.cases[c];
                                if (k.exact) {
                                    if (number && !chosen && k.value == as_double) {
                                        chosen = &k;
                                    }
                                    continue;
                                }
                                std::string_view kw(d.text.data() + k.key, k.key_size);
                                if (kw == "other" && !other) {
                                    other = &k;
                                }
                            }
                            if (!chosen) {
                                for (uint32_t c = n.first; c < n.first + n.count && !chosen; ++c) {
                                    const MfCase& k = d.cases[c];
                                    std::string_view kw(d.text.data() + k.key, k.key_size);
                                    if (!k.exact && kw == keyword) {
                                        chosen = &k;
                                    }
                                }
                            }
                            if (!chosen) {
                                chosen = other;
                            }
                            if (chosen && depth < 64) {
                                write(chosen->first, chosen->count, args, number ? &shifted : nullptr, depth + 1);
                            }
                            break;
                        }
                    }
                }
            }

            static const char* plural_name(plural p) noexcept {
                switch (p) {
                    case plural::zero: return "zero";
                    case plural::one: return "one";
                    case plural::two: return "two";
                    case plural::few: return "few";
                    case plural::many: return "many";
                    default: return "other";
                }
            }
        };

        //----------------------------------------------------------------
        // Number skeletons and DecimalFormat patterns
        //----------------------------------------------------------------

        // .00 .0# .## .00+ (fraction digits), @@@ @@# @@+ (significant digits)
        inline bool mf_precision(std::string_view t, number_options& o) noexcept {
            if (t.empty()) {
                return false;
            }
            if (t[0] == '.') {
                int min = 0, max = 0;
                size_t i = 1;
                while (i < t.size() && t[i] == '0') {
                    ++min;
                    ++i;
                }
                max = min;
                if (i < t.size() && t[i] == '+') {
                    max = 100;
                    ++i;
                } else {
                    while (i < t.size() && t[i] == '#') {
                        ++max;
                        ++i;
                    }
                }
                if (i != t.size()) {
                    return false;
                }
                o.min_fraction = min;
                o.max_fraction = max;
                return true;
            }
            if (t[0] == '@') {
                int min = 0, max = 0;
                size_t i = 0;
                while (i < t.size() && t[i] == '@') {
                    ++min;
                    ++i;
                }
                max = min;
                if (i < t.size() && t[i] == '+') {
                    max = 200;
                    ++i;
                } else {
                    while (i < t.size() && t[i] == '#') {
                        ++max;
                        ++i;
                    }
                }
                if (i != t.size()) {
                    return false;
                }
                o.min_significant = min;
                o.max_significant = max;
                return true;
            }
            return false;
        }

        inline bool mf_skeleton(std::string_view text, const locale& l, MfNumber& out) noexcept {
            number_options o;
            bool precision = false, scaled = false;
            int scale = 0;
            size_t i = 0;
            while (i < text.size()) {
                while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r')) {
                    ++i;
                }
                size_t b = i;
                while (i < text.size() && text[i] != ' ' && text[i] != '\t' && text[i] != '\n' && text[i] != '\r') {
                    ++i;
                }
                std::string_view stem = text.substr(b, i - b);
                if (stem.empty()) {
                    continue;
                }
                if (stem == "percent" || stem == "%") {
                    o.style = number_style::percent;
                    scale -= 2;   // a unit, not a scale: our percent multiplies
                } else if (stem == "%x100") {
                    o.style = number_style::percent;
                } else if (stem == "permille") {
                    o.style = number_style::permille;
                    scale -= 3;
                } else if (stem == "compact-short" || stem == "K") {
                    o.style = o.style == number_style::currency ? number_style::currency_compact : number_style::compact;
                } else if (stem == "compact-long" || stem == "KK") {
                    o.style = number_style::compact_long;
                } else if (stem == "scientific" || stem == "E0") {
                    o.style = number_style::scientific;
                } else if (stem.substr(0, 9) == "currency/" && stem.size() == 12) {
                    auto c = currency::parse(string(stem.substr(9)));
                    if (!c) {
                        return false;
                    }
                    o.currency = *c;
                    o.style = o.style == number_style::compact ? number_style::currency_compact : number_style::currency;
                } else if (stem == "unit-width-iso-code") {
                    o.display = currency_display::code;
                } else if (stem == "unit-width-narrow") {
                    o.display = currency_display::narrow_symbol;
                } else if (stem == "unit-width-full-name") {
                    o.display = currency_display::name;
                } else if (stem == "unit-width-short") {
                    o.display = currency_display::symbol;
                } else if (stem == "precision-integer" || stem == ".") {
                    o.min_fraction = 0;
                    o.max_fraction = 0;
                    precision = true;
                } else if (stem == "precision-unlimited") {
                    o.min_fraction = 0;
                    o.max_fraction = 100;
                    precision = true;
                } else if (stem == "precision-currency-standard") {
                    precision = true;
                } else if (stem == "precision-currency-cash") {
                    o.cash = true;
                    precision = true;
                } else if (mf_precision(stem, o)) {
                    precision = true;
                } else if (stem.substr(0, 14) == "integer-width/") {
                    std::string_view w = stem.substr(14);
                    if (!w.empty() && (w[0] == '+' || w[0] == '*')) {
                        w.remove_prefix(1);
                    }
                    int zeros = 0;
                    for (char c : w) {
                        if (c != '0') {
                            return false;
                        }
                        ++zeros;
                    }
                    o.min_integer = zeros;
                } else if (stem == "group-off" || stem == ",_") {
                    o.grouping = false;
                } else if (stem == "group-auto") {
                    o.grouping = true;
                } else if (stem == "sign-auto") {
                    o.sign = sign_display::automatic;
                } else if (stem == "sign-always" || stem == "+!") {
                    o.sign = sign_display::always;
                } else if (stem == "sign-never" || stem == "+_") {
                    o.sign = sign_display::never;
                } else if (stem == "sign-except-zero" || stem == "+?") {
                    o.sign = sign_display::except_zero;
                } else if (stem == "sign-negative" || stem == "+-") {
                    o.sign = sign_display::negative;
                } else if (stem == "sign-accounting" || stem == "()") {
                    o.style = number_style::accounting;
                } else if (stem.substr(0, 6) == "scale/") {
                    std::string_view k = stem.substr(6);
                    // a power of ten only
                    if (k.empty() || k[0] != '1') {
                        return false;
                    }
                    size_t z = 1;
                    while (z < k.size() && k[z] == '0') {
                        ++z;
                    }
                    if (z != k.size()) {
                        return false;
                    }
                    scale += int(k.size()) - 1;
                    scaled = true;
                } else if (stem.substr(0, 14) == "rounding-mode-") {
                    std::string_view m = stem.substr(14);
                    if (m == "ceiling") {
                        o.mode = sgcl::rounding::ceiling;
                    } else if (m == "floor") {
                        o.mode = sgcl::rounding::floor;
                    } else if (m == "down") {
                        o.mode = sgcl::rounding::down;
                    } else if (m == "up") {
                        o.mode = sgcl::rounding::up;
                    } else if (m == "half-even") {
                        o.mode = sgcl::rounding::half_even;
                    } else if (m == "half-down") {
                        o.mode = sgcl::rounding::half_down;
                    } else if (m == "half-up") {
                        o.mode = sgcl::rounding::half_up;
                    } else if (m == "unnecessary") {
                        o.mode = sgcl::rounding::unnecessary;
                    } else {
                        return false;
                    }
                } else {
                    return false;
                }
            }
            (void)scaled;
            // NumberFormatter's default precision: six fraction digits
            // (a currency's own, the compact forms their own)
            bool currency_like = o.style == number_style::currency || o.style == number_style::accounting
                || o.style == number_style::currency_compact;
            bool compact = o.style == number_style::compact || o.style == number_style::compact_long
                || o.style == number_style::currency_compact;
            if (!precision && !currency_like && !compact) {
                o.min_fraction = 0;
                o.max_fraction = 6;
            }
            out.format = number_format(l, o);
            out.scale = scale;
            return true;
        }

        inline bool mf_decimal_pattern(std::string_view text, const locale& l, MfNumber& out) noexcept {
            // the affixes and the number of each subpattern
            struct Sub {
                std::string prefix, suffix;
                int min_int = 0, hash_int = 0, min_frac = 0, max_frac = 0, min_sig = 0, max_sig = 0, exp_digits = 0;
                int group1 = 0, group2 = 0;   // the digits after the last separator, and between the last two
                int since = -1, before = -1;  // digits since the last separator, and in the group before it
                bool exp_plus = false;
                bool grouping = false, decimal = false, exponent = false, percent = false, permille = false,
                     currency = false, number = false;
            };
            auto read = [](std::string_view p, Sub& sub) noexcept -> size_t {
                size_t i = 0;
                bool after = false;   // past the number: the suffix
                bool in_fraction = false;
                std::string* affix = &sub.prefix;
                while (i < p.size()) {
                    char c = p[i];
                    if (c == ';') {
                        break;
                    }
                    if (c == '\'') {
                        size_t q = p.find('\'', i + 1);
                        if (q == i + 1) {
                            affix->push_back('\'');
                            i += 2;
                            continue;
                        }
                        if (q == std::string_view::npos) {
                            return std::string_view::npos;
                        }
                        affix->append(p.substr(i + 1, q - i - 1));
                        i = q + 1;
                        if (sub.number) {
                            after = true;
                            affix = &sub.suffix;
                        }
                        continue;
                    }
                    bool digit_char = c == '#' || c == '0' || c == ',' || c == '.' || c == '@'
                        || (c >= '1' && c <= '9');
                    if (digit_char && !after) {
                        if (c >= '1' && c <= '9') {
                            return std::string_view::npos;   // a rounding increment
                        }
                        sub.number = true;
                        affix = &sub.suffix;
                        if (c == '.') {
                            if (sub.decimal) {
                                return std::string_view::npos;
                            }
                            sub.decimal = true;
                            in_fraction = true;
                        } else if (c == ',') {
                            if (in_fraction) {
                                return std::string_view::npos;
                            }
                            sub.grouping = true;
                            sub.before = sub.since;
                            sub.since = 0;
                        } else if (c == '@') {
                            ++sub.min_sig;
                            ++sub.max_sig;
                        } else if (c == '0') {
                            if (in_fraction) {
                                ++sub.min_frac;
                                ++sub.max_frac;
                            } else if (sub.min_sig) {
                                return std::string_view::npos;
                            } else {
                                ++sub.min_int;
                            }
                        } else {   // '#'
                            if (in_fraction) {
                                ++sub.max_frac;
                            } else if (sub.min_sig) {
                                ++sub.max_sig;
                            } else {
                                ++sub.hash_int;
                            }
                        }
                        if (!in_fraction && c != ',' && c != '.' && sub.since >= 0) {
                            ++sub.since;
                        }
                        ++i;
                        continue;
                    }
                    if (c == 'E' && sub.number && !after) {
                        sub.exponent = true;
                        ++i;
                        if (i < p.size() && p[i] == '+') {
                            sub.exp_plus = true;
                            ++i;
                        }
                        while (i < p.size() && p[i] == '0') {
                            ++sub.exp_digits;
                            ++i;
                        }
                        if (!sub.exp_digits) {
                            return std::string_view::npos;
                        }
                        after = true;
                        continue;
                    }
                    if (sub.number) {
                        after = true;
                    }
                    size_t len;
                    char32_t cp = mf_decode(p, i, len);
                    if (c == '%') {
                        sub.percent = true;
                        affix->push_back('\x03');
                    } else if (cp == 0x2030) {
                        sub.permille = true;
                        affix->push_back('\x04');
                    } else if (cp == 0xA4) {
                        sub.currency = true;
                        affix->push_back('\x05');
                        while (i + len + 1 < p.size() && p.substr(i + len, 2) == "\xC2\xA4") {
                            len += 2;   // ¤¤ and ¤¤¤ read as the symbol
                        }
                    } else if (c == '-') {
                        affix->push_back('\x01');
                    } else if (c == '+') {
                        affix->push_back('\x02');
                    } else {
                        affix->append(p.substr(i, len));
                    }
                    i += len;
                }
                return sub.number ? i : std::string_view::npos;
            };
            Sub pos, neg;
            size_t end = read(text, pos);
            if (end == std::string_view::npos) {
                return false;
            }
            bool has_neg = false;
            if (end < text.size()) {
                if (read(text.substr(end + 1), neg) == std::string_view::npos) {
                    return false;
                }
                has_neg = true;
            }
            number_options o;
            o.sign = sign_display::never;
            o.grouping = pos.grouping;
            // DecimalFormat's rule: no integer digit and no fraction digit
            // asked for writes one integer digit ("#" writes 0), as does a
            // pattern of significant digits
            o.min_integer = pos.min_int == 0 && (pos.min_frac == 0 || pos.min_sig) ? 1 : pos.min_int;
            if (pos.min_sig) {
                o.min_significant = pos.min_sig;
                o.max_significant = pos.max_sig;
            } else {
                o.min_fraction = pos.min_frac;
                o.max_fraction = pos.max_frac;
            }
            if (pos.exponent) {
                if (pos.grouping || pos.min_sig) {
                    return false;   // DecimalFormat refuses grouping with an exponent
                }
                out.scientific = true;
                out.exponent_plus = pos.exp_plus;
                out.exponent_digits = pos.exp_digits;
                out.min_int = pos.min_int;
                out.max_int = pos.min_int + pos.hash_int;
                out.min_frac = pos.min_frac;
                out.max_frac = pos.max_frac;
            }
            int scale = pos.percent ? 2 : pos.permille ? 3 : 0;
            if (pos.currency) {
                // a pattern with ¤: the currency's digits and symbol
                number_format c(l, {.style = number_style::currency});
                int digits = cldr::NumberAccess::currency_digits(c);
                o.min_fraction = digits;
                o.max_fraction = digits;
                o.min_significant = o.max_significant = 0;
                out.symbols = c;
            }
            out.format = number_format(l, o);
            if (pos.grouping) {
                int g1 = pos.since > 0 ? pos.since : 3, g2 = pos.before > 0 ? pos.before : g1;
                cldr::NumberAccess::set_grouping(out.format, g1, g2);
            }
            out.own = true;
            out.scale = scale;
            out.prefix = pos.prefix;
            out.suffix = pos.suffix;
            out.own_negative = has_neg;
            out.negative_prefix = neg.prefix;
            out.negative_suffix = neg.suffix;
            return true;
        }
    }

    inline expected<message_format, message_error> message_format::parse(const string& pattern,
                                                                         const locale& l) noexcept {
        return detail::MessageParser::make(pattern.view(), pattern, l);
    }

    // the message of text read where it lies (a fuzzer's buffer), keep the
    // library's string of it the message holds
    inline expected<message_format, message_error> detail::MessageParser::make(std::string_view text,
                                                                               const string& keep,
                                                                               const locale& l) noexcept {
        auto d = make_tracked<detail::MessageData>();
        d->where = l;
        d->pattern = keep;
        d->plain = number_format(l);
        d->operands = number_format(locale(), {.grouping = false});
        detail::MessageParser p{text, l, *d, {}};
        if (!p.message(d->first, d->count, 0, detail::MtNone)) {
            return unexpected<message_error>(message_error(p.fail_at, p.fail));
        }
        d->names = dynamic_array<string>(p.names.size());
        for (size_t i = 0; i < p.names.size(); ++i) {
            d->names[i] = string(std::string_view(p.names[i]));
        }
        message_format m;
        m._data = tracked_ptr<const detail::MessageData>(std::move(d));
        return m;
    }

    inline string message_format::format(const value& args) const noexcept {
        const detail::MessageData* d = _data.get();
        if (!d) {
            return string();
        }
        std::string out;
        detail::MessageWriter w{*d, out};
        w.write(d->first, d->count, args, nullptr, 0);
        return string(std::string_view(out));
    }

    inline locale message_format::where() const noexcept {
        const detail::MessageData* d = _data.get();
        return d ? d->where : locale();
    }

    inline string message_format::pattern() const noexcept {
        const detail::MessageData* d = _data.get();
        return d ? d->pattern : string();
    }

    inline expected<string, message_error> format_message(const string& pattern, const locale& l,
                                                          const value& args) noexcept {
        auto m = message_format::parse(pattern, l);
        if (!m) {
            return unexpected<message_error>(m.error());
        }
        return m->format(args);
    }
}
