//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The display names of sgcl/txt/names/ (DESIGN 493, 494): the headers of
// tests/txt/cldr_names_set.h included here (pl.h also in names_second.cpp, to
// see it registered once), the names held against ICU 78 through the vectors
// of tools/cldr_vectors.py, and the fallback on the codes where a locale's
// header is not included.
#include "tests/types.h"
#include "tests/time/cldr_answer.h"
#include "tests/txt/cldr_answer.h"
#include "tests/txt/cldr_names_set.h"
#include "tests/txt/cldr_names_vectors.h"

#include <string>

namespace {
    std::string text(const char8_t* s) {
        return std::string(reinterpret_cast<const char*>(s));
    }

    txt::locale loc(const char* tag) {
        return txt::locale(string(tag));
    }
}

TEST(Names_Tests, Vectors) {
    size_t bad = 0;
    for (const auto& v : NamesVectors) {
        std::string q = text(v.question);
        auto f = cldr_answer::fields(q);
        std::string got = cldr_answer::text_answer(f);
        if (got == "?") {
            got = cldr_answer::date_answer(f);
        }
        if (got != text(v.answer) && ++bad <= 20) {
            ADD_FAILURE() << q << "\n  got " << got << "\n  want " << text(v.answer);
        }
    }
    EXPECT_EQ(bad, 0u) << "of " << std::size(NamesVectors);
}

TEST(Names_Tests, Methods) {
    auto pl = loc("pl");
    EXPECT_EQ(txt::region(string("DE")).display_name(pl), string("Niemcy"));
    EXPECT_EQ(txt::region(string("pl")).display_name(loc("de-AT")), string("Polen"));        // de-AT reads de's
    EXPECT_EQ(txt::region(string("419")).display_name(loc("en")), string("Latin America"));
    EXPECT_EQ(txt::script_code(string("latn")).display_name(pl), string("\xC5\x82" "aci\xC5\x84skie"));
    EXPECT_EQ(txt::script_code(string("Hans")).display_name(loc("en")), string("Simplified Han"));
    EXPECT_EQ(txt::locale(string("de-CH")).display_name(pl), string("niemiecki (Szwajcaria)"));
    EXPECT_EQ(txt::locale(string("zh-Hant-TW")).display_name(loc("en")), string("Chinese (Traditional, Taiwan)"));
    EXPECT_EQ(txt::locale(string("en")).display_name(loc("ja")), string("\xE8\x8B\xB1\xE8\xAA\x9E"));
    EXPECT_EQ(txt::locale().display_name(loc("en")), string("Unknown language"));
    EXPECT_EQ(txt::locale(string("qaa-XK")).display_name(loc("en")), string("qaa (Kosovo)"));   // no name: the code
    auto pln = txt::currency(string("PLN"));
    EXPECT_EQ(pln.display_name(pl), string("z\xC5\x82oty polski"));
    EXPECT_EQ(pln.display_name(pl, txt::plural::few), string("z\xC5\x82ote polskie"));
    EXPECT_EQ(pln.display_name(pl, txt::plural::two), string("z\xC5\x82otego polskiego"));   // no form two: other
    EXPECT_EQ(txt::format_number(5, pl, {.style = txt::number_style::currency, .currency = pln,
                                         .display = txt::currency_display::name}),
              string("5,00 z\xC5\x82otego polskiego"));
    EXPECT_EQ(txt::format_number(5, pl, {.style = txt::number_style::currency, .max_fraction = 0, .currency = pln,
                                         .display = txt::currency_display::name}),
              string("5 z\xC5\x82otych polskich"));
    EXPECT_EQ(txt::format_number(1, loc("en"), {.style = txt::number_style::currency, .max_fraction = 0,
                                                .currency = txt::currency(string("USD")),
                                                .display = txt::currency_display::name}),
              string("1 US dollar"));
    EXPECT_TRUE(pl.has_names());
    EXPECT_TRUE(loc("de-AT").has_names());      // through de
    EXPECT_TRUE(loc("es-AR").has_names());      // through es-419, included by es-MX
    EXPECT_FALSE(loc("it").has_names());
    EXPECT_FALSE(txt::locale().has_names());
}

// A locale whose header is not included: the codes (and, in a Debug build,
// one line on stderr naming the header)
TEST(Names_Tests, NotIncluded) {
    auto it = loc("it");
    EXPECT_EQ(txt::region(string("DE")).display_name(it), string("DE"));
    EXPECT_EQ(txt::script_code(string("Latn")).display_name(it), string("Latn"));
    EXPECT_EQ(txt::currency(string("EUR")).display_name(it), string("EUR"));
    EXPECT_EQ(txt::locale(string("de-CH")).display_name(it), string("de (CH)"));
    EXPECT_EQ(txt::format_number(5, it, {.style = txt::number_style::currency, .currency = txt::currency(string("EUR")),
                                         .display = txt::currency_display::name}),
              string("5,00 EUR"));
    EXPECT_EQ(txt::region().code(), string());
    EXPECT_EQ(txt::region().display_name(loc("pl")), string());
}

TEST(Names_Tests, Zones) {
    auto t = time::date(2026, 9, 24).at(14, 5, *time::zone::load(string("Europe/Warsaw")));
    auto f = [&](const char* tag, const char* pattern) {
        return time::date_format::from_pattern(loc(tag), string(pattern)).format(t);
    };
    EXPECT_EQ(f("pl", "zzzz"), string("czas \xC5\x9Brodkowoeuropejski letni"));
    EXPECT_EQ(f("pl", "VVV | VVVV"), string("Warszawa | czas: Polska"));
    EXPECT_EQ(f("en", "z | zzzz | v | vvvv"), string("GMT+2 | Central European Summer Time | Poland Time | Central European Time"));
    EXPECT_EQ(t.format(loc("pl"), time::style::none, time::style::detailed), string("14:05:00 CEST"));
    auto tokyo = t.in(*time::zone::load(string("Asia/Tokyo")));
    EXPECT_EQ(time::date_format::from_pattern(loc("en"), string("vvvv")).format(tokyo), string("Japan Standard Time"));
    auto utc = t.utc();
    EXPECT_EQ(time::date_format::from_pattern(loc("en"), string("vvvv | VVV")).format(utc), string("GMT | Unknown City"));
    EXPECT_EQ(time::date_format::from_pattern(loc("it"), string("zzzz")).format(t), string("GMT+02:00"));   // no names
}

TEST(Names_Tests, Registry) {
    // one table a locale, however many translation units include its header
    size_t pl = 0, all = 0;
    uint64_t key = txt::detail::cldr::normalized_key(loc("pl"));
    for (auto* n = txt::detail::names::head().load(); n; n = n->next) {
        ++all;
        pl += n->table->locale == key;
    }
    EXPECT_EQ(pl, 1u);
    EXPECT_GE(all, std::size(NamesLocales));
    // a variant holds its differences and reaches its parent's
    auto* ch = txt::detail::names::table_of(txt::detail::cldr::normalized_key(loc("de-CH")));
    ASSERT_NE(ch, nullptr);
    EXPECT_EQ(ch->parent, txt::detail::cldr::normalized_key(loc("de")));
    EXPECT_LT(ch->regions, 50u);
}
