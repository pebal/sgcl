//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The readers and writers of the locale data on any input: a locale tag, an
// Accept-Language header, decimal text and its plural form, a currency code,
// a list, a relative time, a date pattern and a skeleton. The first byte picks
// the path (and a locale of a short list); the rest is the input. What must
// hold:
//   - a locale read from any text writes a tag that reads back as the same
//     locale; maximize is idempotent and minimize maximizes to the same;
//     the chain of parents ends at root within six steps; best_match of any
//     header answers one of the supported locales;
//   - number_format of any text answers nullopt or a text that formats again
//     to itself under exact options (root locale, no grouping, nothing
//     dropped), and format_to's size is format's; plural_of of any text is
//     one of the six categories;
//   - currency::parse accepts exactly three ASCII letters, and its code reads
//     back the same;
//   - format_list of any items holds each item, in order;
//   - format_relative of any value, and a date pattern or a skeleton of any
//     text over a fixed time, write valid UTF-8 and never throw;
//   - the display names (pl, de-CH and en included): a region, a script or a
//     locale read from any text is named in valid UTF-8, its code where it has
//     no name, and a code that parses names itself back.
// Every reading is of libFuzzer's own buffer, or of a malloc'd copy of
// exactly its size, through the detail entries the public calls wrap: never
// of a copy on the managed heap, where ASan does not see a read past the end.
// Built with libFuzzer (tests/fuzz/run.sh tests/txt/fuzz/cldr_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/time.h"
#include "sgcl/txt/names/de-CH.h"
#include "sgcl/txt/names/en.h"
#include "sgcl/txt/names/pl.h"
#include "sgcl/txt/txt.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    const char* const Locales[] = {"en", "pl", "de-CH", "ar-EG", "hi", "ja", "sw", "he", "es", "fr", "zh-Hant", "ru"};

    txt::locale pick(uint8_t mode) {
        return txt::locale(string(Locales[(mode >> 3) % std::size(Locales)]));
    }

    void valid(const string& s) {
        check(utf8::valid(s.view()));
    }

    // text of our own read again from memory of exactly its size
    struct Exact {
        char* p;
        size_t n;

        explicit Exact(std::string_view s)
        : p(static_cast<char*>(std::malloc(s.size() ? s.size() : 1)))
        , n(s.size()) {
            std::copy(s.begin(), s.end(), p);
        }

        ~Exact() {
            std::free(p);
        }

        std::string_view view() const {
            return std::string_view(p, n);
        }
    };

    // number_format::format of decimal text, read where it lies
    optional<std::string> format_text(const txt::number_format& f, std::string_view text) {
        txt::detail::cldr::Decimal x;
        if (!txt::detail::cldr::decimal_of_text(text, x)) {
            return nullopt;
        }
        std::string out;
        txt::detail::cldr::NumberAccess::append(f, x, out);
        return out;
    }

    void locale_path(std::string_view in) {
        txt::locale l = txt::detail::cldr::LocaleAccess::parse(in);
        txt::locale again{l.to_string()};
        check(l == again || l == txt::locale());
        txt::locale max = l.maximize();
        check(max.maximize() == max);
        check(l.minimize().maximize() == max);
        txt::locale p = l;
        int steps = 0;
        while (p != txt::locale() && steps < 8) {
            p = p.parent();
            ++steps;
        }
        check(steps <= 6);
        valid(l.autonym());
        txt::locale supported[] = {txt::locale(string("en")), txt::locale(string("de")), txt::locale(string("pt-PT"))};
        txt::locale best = txt::detail::cldr::best_match_text(in, supported);
        check(best == supported[0] || best == supported[1] || best == supported[2]);
        check(txt::best_match(slice<const txt::locale>(&l, 1), supported) != txt::locale());
    }

    void number_path(std::string_view in, const txt::locale& l) {
        txt::number_options exact{.max_fraction = 1000, .grouping = false, .mode = sgcl::rounding::unnecessary};
        txt::number_format root(txt::locale(), exact);
        auto once = format_text(root, in);
        if (once) {
            Exact again(*once);
            auto twice = format_text(root, again.view());
            check(twice && *twice == *once);
        }
        for (int style = 0; style < 9; ++style) {
            txt::number_format f(l, {.style = txt::number_style(style)});
            auto r = format_text(f, in);
            check(r.has_value() == once.has_value());
            if (r) {
                check(utf8::valid(*r));
            }
        }
        txt::plural p = txt::detail::cldr::cardinal(txt::detail::cldr::operands_of(in), l);
        check(int(p) <= 5);
        // a double made of the bytes, and format_to's size
        double d = 0;
        if (in.size() >= sizeof d) {
            std::memcpy(&d, in.data(), sizeof d);
        }
        txt::number_format f(l, {.style = txt::number_style(in.empty() ? 0 : uint8_t(in[0]) % 9)});
        string s = f.format(d);
        valid(s);
        char room[16];
        check(f.format_to(slice<char>(room, sizeof room), d) == s.size());
        check(int(txt::plural_of(d, l)) <= 5);
    }

    void currency_path(std::string_view in, const txt::locale& l) {
        auto c = txt::detail::cldr::CurrencyAccess::parse(in);
        bool letters = in.size() == 3;
        for (char ch : in) {
            letters = letters && ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'));
        }
        check(c.has_value() == letters);
        if (c) {
            check(txt::currency(c->code()) == *c);
            valid(c->symbol(l));
            valid(txt::format_currency(1234.5, *c, l));
        } else {
            check(c.error().offset() <= in.size());
        }
    }

    void list_path(std::string_view in, const txt::locale& l, uint8_t mode) {
        vector<string> items;
        size_t start = 0;
        while (start <= in.size() && items.size() < 40) {
            size_t nul = in.find('\0', start);
            if (nul == std::string_view::npos) {
                nul = in.size();
            }
            items.push_back(string(in.substr(start, nul - start)));
            start = nul + 1;
        }
        string out = txt::format_list(items, l, txt::list_type(mode % 3), txt::width((mode >> 1) % 3));
        size_t at = 0;
        for (const string& item : items) {
            size_t found = out.view().find(item.view(), at);
            check(found != std::string_view::npos);
            at = found + item.size();
        }
    }

    void names_path(std::string_view in, const txt::locale& l) {
        auto r = txt::detail::cldr::LocaleAccess::parse_region(in);
        if (r) {
            check(txt::region(r->code()) == *r);
            valid(r->display_name(l));
        }
        auto sc = txt::detail::cldr::LocaleAccess::parse_script(in);
        if (sc) {
            check(txt::script_code(sc->code()) == *sc);
            valid(sc->display_name(l));
        }
        txt::locale named = txt::detail::cldr::LocaleAccess::parse(in);
        valid(named.display_name(l));
        valid(l.display_name(named));
        auto c = txt::detail::cldr::CurrencyAccess::parse(in);
        if (c) {
            valid(c->display_name(l));
            for (int p = 0; p < 6; ++p) {
                valid(c->display_name(l, txt::plural(p)));
            }
            valid(txt::format_number(1.5, l, {.style = txt::number_style::currency, .currency = *c,
                                              .display = txt::currency_display::name}));
        }
    }

    void time_path(std::string_view in, const txt::locale& l, uint8_t mode) {
        // the literal text of a pattern is copied: valid UTF-8 out of valid in
        bool utf = utf8::valid(in);
        auto valid = [&](const string& s) {
            check(!utf || utf8::valid(s.view()));
        };
        double v = 0;
        if (in.size() >= sizeof v) {
            std::memcpy(&v, in.data(), sizeof v);
        }
        valid(txt::format_relative(v, txt::time_unit(mode % 8), l, {.width = txt::width((mode >> 3) % 3), .numeric = (mode & 64) != 0}));
        auto t = time::date(2026, 9, 24).at(14, 5, 9, time::zone::utc());
        namespace cldr = time::detail::cldr;
        cldr::Calendar cal = cldr::calendar_of(l);
        string zone_name = t.zone().name();
        valid(cldr::render(cal, cldr::fields_of(t, zone_name), in));
        string pattern = cldr::pattern_of_skeleton(in, cal);
        valid(pattern);
        auto f = time::date_format::from_pattern(l, pattern);
        valid(f.format(t));
        valid(f.format(time::date(1, 1, 1)));
        auto later = t + duration(std::chrono::hours(int(mode) * 13));
        valid(cldr::interval(cal, cldr::fields_of(t, zone_name), cldr::fields_of(later, zone_name), in));
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 4096) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    txt::locale l = pick(mode);
    switch (mode % 7) {
        case 0: locale_path(rest); break;
        case 1: number_path(rest, l); break;
        case 2: currency_path(rest, l); break;
        case 3: list_path(rest, l, mode); break;
        case 4:
        case 5: time_path(rest, l, mode); break;
        case 6: names_path(rest, l); break;
    }
    return 0;
}
