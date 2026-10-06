//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The locale formatting of CLDR's data, one value at a time, a formatter made
// once (the way ICU's unum/udat/uplrules objects are opened once):
//   cldr <number|compact|currency|plural|list|relative|date|skeleton> [locale=pl]
//   number:   number_format::format of a double, the locale's decimal format
//   compact:  the short compact form of an integer ("1,2 mln")
//   currency: an amount of PLN in the locale's currency pattern
//   plural:   plural_of of an integer
//   list:     format_list of three items
//   relative: format_relative of a number of days
//   date:     datetime::format in the locale's full date and short time
//   skeleton: date_format::from_skeleton("yMMMd") and its format
//   names:    the display name of a locale with a region, in the locale (pl.h included)
//   zone:     datetime::format in the full time style, the zone's long name
// Prints nanoseconds per value. The references are ICU 78 (a C program over
// the same values) and Go's golang.org/x/text for number and plural, which
// the comparison runs in a scratch module: neither is in the tree.
#include "benchmarks/common.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"
#include "sgcl/txt/names/pl.h"

#include <cstdio>
#include <cstring>

namespace {
    using namespace sgcl;

    template<class F>
    double per_op(size_t n, F&& f) {
        size_t sink = 0;
        for (size_t i = 0; i < n / 10; ++i) {   // warm the caches and the allocator
            sink += f(i);
        }
        auto t0 = bench::Clock::now();
        for (size_t i = 0; i < n; ++i) {
            sink += f(i);
        }
        double s = bench::seconds_since(t0);
        if (sink == 42) {
            std::printf(" ");
        }
        return s * 1e9 / double(n);
    }

    double run(const char* variant, const txt::locale& l) {
        constexpr size_t N = 400000;
        if (!std::strcmp(variant, "number")) {
            txt::number_format f(l);
            return per_op(N, [&](size_t i) { return f.format(1234567.891 + double(i)).size(); });
        }
        if (!std::strcmp(variant, "compact")) {
            txt::number_format f(l, {.style = txt::number_style::compact});
            return per_op(N, [&](size_t i) { return f.format(int64_t(1234567 + i * 7919)).size(); });
        }
        if (!std::strcmp(variant, "currency")) {
            txt::number_format f(l, {.style = txt::number_style::currency, .currency = txt::currency(string("PLN"))});
            return per_op(N, [&](size_t i) { return f.format(12.5 + double(i)).size(); });
        }
        if (!std::strcmp(variant, "plural")) {
            return per_op(N * 10, [&](size_t i) { return size_t(txt::plural_of(int64_t(i), l)); });
        }
        if (!std::strcmp(variant, "list")) {
            string a("Ala"), b("Ola"), c("Ela");
            string items[] = {a, b, c};
            return per_op(N, [&](size_t) { return txt::format_list(items, l).size(); });
        }
        if (!std::strcmp(variant, "relative")) {
            return per_op(N, [&](size_t i) { return txt::format_relative(double(i % 50) - 25, txt::time_unit::day, l).size(); });
        }
        if (!std::strcmp(variant, "date")) {
            time::date_format f(l, time::style::full, time::style::brief);
            auto t = time::date(2026, 9, 24).at(14, 5, time::zone::utc());
            return per_op(N, [&](size_t i) { return f.format(t + duration(std::chrono::minutes(int64_t(i)))).size(); });
        }
        if (!std::strcmp(variant, "names")) {
            txt::locale tags[] = {txt::locale(string("de-CH")), txt::locale(string("pt-BR")), txt::locale(string("sr-Latn-ME"))};
            return per_op(N, [&](size_t i) { return tags[i % 3].display_name(l).size(); });
        }
        if (!std::strcmp(variant, "zone")) {
            time::date_format f(l, time::style::none, time::style::full);
            auto t = time::date(2026, 9, 24).at(14, 5, *time::zone::load(string("Europe/Warsaw")));
            return per_op(N, [&](size_t i) { return f.format(t + duration(std::chrono::minutes(int64_t(i)))).size(); });
        }
        time::date_format f = time::date_format::from_skeleton(l, string("yMMMd"));
        auto t = time::date(2026, 9, 24).at(14, 5, time::zone::utc());
        return per_op(N, [&](size_t i) { return f.format(t + duration(std::chrono::hours(int64_t(i)))).size(); });
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "number";
    if (!bench::has_variant(variant, {"number", "compact", "currency", "plural", "list", "relative", "date", "skeleton", "names", "zone"})) {
        std::fprintf(stderr, "usage: cldr <number|compact|currency|plural|list|relative|date|skeleton|names|zone> [locale]\n");
        return 2;
    }
    const char* tag = argc > 2 ? argv[2] : "pl";
    double ns = run(variant, txt::locale(string(tag)));
    std::printf("%s locale=%s ns/op=%.1f\n", variant, tag, ns);
    return 0;
}
