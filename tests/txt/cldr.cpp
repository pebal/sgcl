//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The locale data of CLDR 46 and what formats with it: txt::locale (the
// tag, the likely subtags, the parents, matching, the autonyms), the plural
// rules, numbers and currencies, lists and relative time. Two oracles: the
// plural rules' own samples (every one of CLDR's @integer and @decimal
// samples, every locale of its rule set), and ICU 78 through the vectors of
// tools/cldr_vectors.py — the cases where ICU (CLDR 48) and the library
// (CLDR 46) agree; the rest differ by the data, and the library's own data is
// held here by hand, case by case, from the XML of CLDR 46.
#include "tests/types.h"
#include "tests/txt/cldr_answer.h"
#include "tests/txt/cldr_plural_samples.h"
#include "tests/txt/cldr_vectors.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>

namespace {
    std::string text(const char8_t* s) {
        return std::string(reinterpret_cast<const char*>(s));
    }

    // every vector of a table, the failures named
    template<size_t N>
    void check_vectors(const CldrVector (&table)[N]) {
        size_t bad = 0;
        for (const auto& v : table) {
            std::string q = text(v.question);
            std::string got = cldr_answer::text_answer(cldr_answer::fields(q));
            if (got != text(v.answer)) {
                if (++bad <= 20) {
                    ADD_FAILURE() << q << "\n  got " << got << "\n  want " << text(v.answer);
                }
            }
        }
        EXPECT_EQ(bad, 0u) << "of " << N;
    }

    txt::locale loc(const char* tag) {
        return txt::locale(string(tag));
    }
}

//------------------------------------------------------------------------------
// locale
//------------------------------------------------------------------------------
TEST(Cldr_Locale_Tests, Parts) {
    auto l = loc("sr-latn-rs");
    EXPECT_EQ(l.language(), string("sr"));
    EXPECT_EQ(l.script(), string("Latn"));
    EXPECT_EQ(l.region(), string("RS"));
    EXPECT_EQ(l.to_string(), string("sr-Latn-RS"));
    EXPECT_EQ(loc("es_419").to_string(), string("es-419"));
    EXPECT_EQ(loc("es_419").region(), string("419"));
    EXPECT_EQ(loc("pl_PL.UTF-8").to_string(), string("pl-PL"));
    EXPECT_EQ(loc("sr@latin").to_string(), string("sr-Latn"));
    EXPECT_EQ(loc("sr_RS@cyrillic").to_string(), string("sr-Cyrl-RS"));
    EXPECT_EQ(loc("en-US-u-nu-latn").to_string(), string("en-US-u-nu-latn"));
    EXPECT_TRUE(loc("ar-EG-u-ca-gregory-nu-latn").latin_digits());
    EXPECT_FALSE(loc("ar-EG-u-nu-arab").latin_digits());
    EXPECT_EQ(loc("zh-yue-HK").to_string(), string("zh-HK"));            // an extended language is read past
    EXPECT_EQ(loc("de-CH-1996").to_string(), string("de-CH"));           // a variant too
    EXPECT_EQ(loc("de-x-private-CH").to_string(), string("de"));         // private use ends the tag
    EXPECT_EQ(loc("en-a-bbb-US").to_string(), string("en"));             // an extension's subtags are its own
    EXPECT_EQ(loc("und-PL").to_string(), string("und-PL"));
    // the root locale: no tag, an unreadable one
    for (const char* tag : {"", "C", "POSIX", "x", "-tr", "123", "root", "\xC5\x82t"}) {
        EXPECT_EQ(loc(tag), txt::locale::root()) << tag;
        EXPECT_EQ(loc(tag).to_string(), string("und")) << tag;
    }
    EXPECT_EQ(txt::locale().language(), string());
    EXPECT_EQ(txt::locale().script(), string());
    EXPECT_EQ(txt::locale().region(), string());
    // the deprecated codes CLDR replaces
    EXPECT_EQ(loc("iw").to_string(), string("he"));
    EXPECT_EQ(loc("in-ID").to_string(), string("id-ID"));
    EXPECT_EQ(loc("ji").to_string(), string("yi"));
    EXPECT_EQ(loc("jw").to_string(), string("jv"));
    EXPECT_EQ(loc("mo").to_string(), string("ro"));
    EXPECT_EQ(loc("tl").to_string(), string("fil"));
    EXPECT_EQ(loc("sh").to_string(), string("sr-Latn"));
    EXPECT_EQ(loc("sh-Cyrl").to_string(), string("sr-Cyrl"));
    static_assert(sizeof(txt::locale) == 8 && std::is_trivially_copyable_v<txt::locale>);
}

// A behaviour change of an existing API (DESIGN 495): a locale is its
// language, script and region, so two locales of one language in two
// countries are two locales; the case mappings still ask by language
TEST(Cldr_Locale_Tests, Equality) {
    EXPECT_NE(loc("tr-TR"), txt::locale::turkish());
    EXPECT_NE(loc("tr-TR"), loc("tr-CY"));
    EXPECT_EQ(loc("TR-tr"), loc("tr_TR.UTF-8"));
    EXPECT_NE(loc("sr-Latn"), loc("sr-Cyrl"));
    EXPECT_NE(loc("en-u-nu-latn"), loc("en"));
    EXPECT_TRUE(loc("tr-TR").dotted_i());
    EXPECT_TRUE(loc("az-Latn-AZ").dotted_i());
    EXPECT_TRUE(loc("lt-LT").keeps_dot());
    EXPECT_EQ(loc("tr-TR").subtag(), txt::locale::turkish().subtag());
    EXPECT_EQ(txt::to_lower_full(string("I"), loc("tr-TR")), string("\xC4\xB1"));
}

TEST(Cldr_Locale_Tests, Likely) {
    EXPECT_EQ(loc("pl").maximize().to_string(), string("pl-Latn-PL"));
    EXPECT_EQ(loc("sr").maximize().to_string(), string("sr-Cyrl-RS"));
    EXPECT_EQ(loc("sr-ME").maximize().to_string(), string("sr-Latn-ME"));
    EXPECT_EQ(loc("zh-TW").maximize().to_string(), string("zh-Hant-TW"));
    EXPECT_EQ(loc("und-PL").maximize().to_string(), string("pl-Latn-PL"));
    EXPECT_EQ(loc("und").maximize().to_string(), string("en-Latn-US"));
    EXPECT_EQ(loc("und-Zzzz-ZZ").maximize().to_string(), string("en-Latn-US"));
    EXPECT_EQ(loc("qaa").maximize().to_string(), string("qaa"));            // a language CLDR does not know
    EXPECT_EQ(loc("en-u-nu-latn").maximize().to_string(), string("en-Latn-US-u-nu-latn"));
    EXPECT_EQ(loc("zh-Hant-TW").minimize().to_string(), string("zh-TW"));
    EXPECT_EQ(loc("sr-Cyrl-RS").minimize().to_string(), string("sr"));
    EXPECT_EQ(loc("pa-Arab-PK").minimize().to_string(), string("pa-PK"));
    EXPECT_EQ(loc("qaa-Latn").minimize().to_string(), string("qaa-Latn"));
    // the parents of CLDR: parentLocales, truncation, a script not the
    // language's own has root as its parent
    EXPECT_EQ(loc("es-MX").parent().to_string(), string("es-419"));
    EXPECT_EQ(loc("es-419").parent().to_string(), string("es"));
    EXPECT_EQ(loc("es").parent(), txt::locale::root());
    EXPECT_EQ(loc("en-GB").parent().to_string(), string("en-001"));
    EXPECT_EQ(loc("en-AT").parent().to_string(), string("en-150"));
    EXPECT_EQ(loc("pt-AO").parent().to_string(), string("pt-PT"));
    EXPECT_EQ(loc("nb").parent().to_string(), string("no"));
    EXPECT_EQ(loc("sr-Latn").parent(), txt::locale::root());
    EXPECT_EQ(loc("sr-Latn-BA").parent().to_string(), string("sr-Latn"));
    EXPECT_EQ(loc("zh-Hant-MO").parent().to_string(), string("zh-Hant-HK"));
    EXPECT_EQ(txt::locale::root().parent(), txt::locale::root());
    // the key of the digits rides along, but not onto root
    EXPECT_EQ(loc("sr-Latn-RS-u-nu-latn").parent().to_string(), string("sr-Latn-u-nu-latn"));
    EXPECT_EQ(loc("sr-Latn-u-nu-latn").parent(), txt::locale::root());
}

TEST(Cldr_Locale_Tests, Autonyms) {
    EXPECT_EQ(loc("pl").autonym(), string("polski"));
    EXPECT_EQ(loc("pl-PL").autonym(), string("polski"));
    EXPECT_EQ(loc("de").autonym(), string("Deutsch"));
    EXPECT_EQ(loc("de-CH").autonym(), string("Schweizer Hochdeutsch"));
    EXPECT_EQ(loc("en-GB").autonym(), string("British English"));
    EXPECT_EQ(loc("es-419").autonym(), string("espa\xC3\xB1ol latinoamericano"));
    EXPECT_EQ(loc("ja").autonym(), string("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"));
    EXPECT_EQ(loc("sr").autonym(), string("\xD1\x81\xD1\x80\xD0\xBF\xD1\x81\xD0\xBA\xD0\xB8"));
    EXPECT_EQ(loc("sr-Latn").autonym(), string("srpski"));
    EXPECT_EQ(loc("qaa").autonym(), string("qaa"));    // no data: the code
    EXPECT_EQ(txt::locale().autonym(), string());
}

TEST(Cldr_Locale_Tests, Matching) {
    txt::locale supported[] = {loc("en"), loc("de"), loc("en-GB"), loc("pt-BR"), loc("pt-PT"), loc("sr-Latn"),
                               loc("zh-TW"), loc("es"), loc("es-419")};
    auto best = [&](const char* header) {
        return txt::best_match(string(header), supported).to_string();
    };
    EXPECT_EQ(best("de-CH"), string("de"));
    EXPECT_EQ(best("en-AU"), string("en-GB"));
    EXPECT_EQ(best("en-US"), string("en"));
    EXPECT_EQ(best("pt-AO"), string("pt-PT"));
    EXPECT_EQ(best("sr"), string("sr-Latn"));
    EXPECT_EQ(best("zh-HK"), string("zh-TW"));
    EXPECT_EQ(best("es-AR"), string("es-419"));
    EXPECT_EQ(best("fr"), string("en"));                         // nothing close: the first, the default
    EXPECT_EQ(best("pl, de;q=0.5"), string("de"));
    EXPECT_EQ(best("fr;q=0.9, de;q=0.8"), string("de"));
    EXPECT_EQ(best("de;q=0.1, en-GB;q=0.9"), string("en-GB"));    // by weight, not by order
    EXPECT_EQ(best("de;q=0, en-GB"), string("en-GB"));            // weight 0: not wanted
    EXPECT_EQ(best("*"), string("en"));
    EXPECT_EQ(best(""), string("en"));
    EXPECT_EQ(best(" , ;q=1, de "), string("de"));
    EXPECT_EQ(txt::best_match(string("de"), slice<const txt::locale>()), txt::locale());
    txt::locale wanted[] = {loc("xx"), loc("de-AT")};
    EXPECT_EQ(txt::best_match(wanted, supported).to_string(), string("de"));
    EXPECT_EQ(txt::best_match(slice<const txt::locale>(), supported).to_string(), string("en"));
    // an exact supported locale wins over one that maximizes the same
    txt::locale en_us[] = {loc("en"), loc("en-US")};
    EXPECT_EQ(txt::best_match(string("en-US"), en_us).to_string(), string("en-US"));
    // more than 32 languages in a header: the first 32 read
    std::string many;
    for (int i = 0; i < 40; ++i) {
        many += "xx,";
    }
    many += "de";
    EXPECT_EQ(txt::best_match(string(many.c_str()), supported).to_string(), string("en"));
}

TEST(Cldr_Locale_Tests, System) {
    const char* saved[3] = {std::getenv("LC_ALL"), std::getenv("LC_MESSAGES"), std::getenv("LANG")};
    std::string keep[3];
    for (int i = 0; i < 3; ++i) {
        keep[i] = saved[i] ? saved[i] : "";
    }
    ::unsetenv("LC_ALL");
    ::unsetenv("LC_MESSAGES");
    ::setenv("LANG", "pl_PL.UTF-8", 1);
    EXPECT_EQ(txt::locale::system().to_string(), string("pl-PL"));
    ::setenv("LC_MESSAGES", "de_CH.UTF-8", 1);
    EXPECT_EQ(txt::locale::system().to_string(), string("de-CH"));
    ::setenv("LC_ALL", "C", 1);
    EXPECT_EQ(txt::locale::system(), txt::locale::root());
    ::unsetenv("LC_ALL");
    ::unsetenv("LC_MESSAGES");
    ::unsetenv("LANG");
    EXPECT_EQ(txt::locale::system(), txt::locale::root());
    const char* names[3] = {"LC_ALL", "LC_MESSAGES", "LANG"};
    for (int i = 0; i < 3; ++i) {
        if (saved[i]) {
            ::setenv(names[i], keep[i].c_str(), 1);
        }
    }
}

TEST(Cldr_Locale_Tests, Vectors) {
    check_vectors(LocaleVectors);
}

//------------------------------------------------------------------------------
// plural rules
//------------------------------------------------------------------------------
TEST(Cldr_Plural_Tests, Samples) {
    size_t n = 0, bad = 0;
    for (const auto& s : PluralSamples) {
        std::istringstream locales(PluralSampleSets[s.set].locales);
        std::string tag;
        while (locales >> tag) {
            txt::locale l = loc(tag == "root" ? "" : tag.c_str());
            txt::plural got;
            if (PluralSampleSets[s.set].ordinal) {
                got = txt::ordinal_of(std::stoll(s.number), l);
            } else {
                got = txt::plural_of(string(s.number), l);
            }
            ++n;
            if (int(got) != s.category && ++bad <= 20) {
                ADD_FAILURE() << tag << " " << s.number << " got " << int(got) << " want " << s.category;
            }
        }
    }
    EXPECT_EQ(bad, 0u) << "of " << n;
    EXPECT_GT(n, 14000u);
}

TEST(Cldr_Plural_Tests, Overloads) {
    auto pl = loc("pl");
    EXPECT_EQ(txt::plural_of(1, pl), txt::plural::one);
    EXPECT_EQ(txt::plural_of(2, pl), txt::plural::few);
    EXPECT_EQ(txt::plural_of(5, pl), txt::plural::many);
    EXPECT_EQ(txt::plural_of(22, pl), txt::plural::few);
    EXPECT_EQ(txt::plural_of(12, pl), txt::plural::many);
    EXPECT_EQ(txt::plural_of(1.5, pl), txt::plural::other);
    EXPECT_EQ(txt::plural_of(1.0, pl), txt::plural::one);        // a double: its shortest digits, 1
    EXPECT_EQ(txt::plural_of(string("1.0"), pl), txt::plural::other);   // text: as written
    EXPECT_EQ(txt::plural_of(-5, pl), txt::plural::many);        // the absolute value
    EXPECT_EQ(txt::plural_of(uint8_t(3), pl), txt::plural::few);
    EXPECT_EQ(txt::plural_of(std::numeric_limits<long long>::min(), pl), txt::plural::many);
    EXPECT_EQ(txt::plural_of(std::numeric_limits<unsigned long long>::max(), pl), txt::plural::many);
    auto en = loc("en");
    EXPECT_EQ(txt::plural_of(1, en), txt::plural::one);
    EXPECT_EQ(txt::plural_of(string("1.0"), en), txt::plural::other);
    EXPECT_EQ(txt::ordinal_of(1, en), txt::plural::one);
    EXPECT_EQ(txt::ordinal_of(2, en), txt::plural::two);
    EXPECT_EQ(txt::ordinal_of(3, en), txt::plural::few);
    EXPECT_EQ(txt::ordinal_of(11, en), txt::plural::other);
    EXPECT_EQ(txt::ordinal_of(-21, en), txt::plural::one);
    auto fr = loc("fr");
    EXPECT_EQ(txt::plural_of(1e6, fr), txt::plural::many);
    EXPECT_EQ(txt::plural_of(1e20, fr), txt::plural::many);
    EXPECT_EQ(txt::plural_of(string("1.2c6"), fr), txt::plural::many);
    EXPECT_EQ(txt::plural_of(string("1c3"), fr), txt::plural::other);
    // what is not a number reads as far as it is one
    EXPECT_EQ(txt::plural_of(string(""), pl), txt::plural::many);        // 0
    EXPECT_EQ(txt::plural_of(string("abc"), pl), txt::plural::many);
    EXPECT_EQ(txt::plural_of(string("2xyz"), pl), txt::plural::few);
    EXPECT_EQ(txt::plural_of(string(std::string(500, '9').c_str()), pl), txt::plural::many);
    EXPECT_EQ(txt::plural_of(string(("0." + std::string(300, '0') + "1").c_str()), en), txt::plural::other);
    EXPECT_EQ(txt::plural_of(std::numeric_limits<double>::infinity(), en), txt::plural::other);
    EXPECT_EQ(txt::plural_of(std::nan(""), en), txt::plural::other);
    EXPECT_EQ(txt::plural_of(1e-300, en), txt::plural::other);
    EXPECT_EQ(txt::plural_of(5, txt::locale()), txt::plural::other);   // root
    EXPECT_EQ(txt::plural_of(1, loc("qaa")), txt::plural::other);      // no rules: root's
    // a region of its own (pt-PT) and a language without one
    EXPECT_EQ(txt::plural_of(0, loc("pt")), txt::plural::one);
    EXPECT_EQ(txt::plural_of(0, loc("pt-PT")), txt::plural::other);
    EXPECT_EQ(txt::plural_of(0, loc("pt-BR")), txt::plural::one);
}

TEST(Cldr_Plural_Tests, Vectors) {
    check_vectors(PluralVectors);
}

//------------------------------------------------------------------------------
// numbers
//------------------------------------------------------------------------------
TEST(Cldr_Number_Tests, Vectors) {
    check_vectors(NumberVectors);
}

TEST(Cldr_Number_Tests, Defaults) {
    EXPECT_EQ(txt::format_number(1234567.891, loc("pl")), string("1\xC2\xA0" "234\xC2\xA0" "567,891"));
    EXPECT_EQ(txt::format_number(1234, loc("pl")), string("1234"));           // two digits before a group
    EXPECT_EQ(txt::format_number(12345, loc("pl")), string("12\xC2\xA0" "345"));
    EXPECT_EQ(txt::format_number(123456789.5, loc("hi")), string("12,34,56,789.5"));
    EXPECT_EQ(txt::format_number(1234.5), string("1,234.5"));                 // root
    EXPECT_EQ(txt::format_number(1.23456), string("1.235"));                  // three fraction digits
    EXPECT_EQ(txt::format_number(2.675, loc("en"), {.max_fraction = 2}), string("2.68"));   // the shortest digits
    EXPECT_EQ(txt::format_number(1234, loc("ar-EG")), string("\xD9\xA1\xD9\xAC\xD9\xA2\xD9\xA3\xD9\xA4"));
    EXPECT_EQ(txt::format_number(1234, loc("ar-EG-u-nu-latn")), string("1,234"));
    EXPECT_EQ(txt::format_number(1234567, loc("de-CH")), string("1\xE2\x80\x99" "234\xE2\x80\x99" "567"));
    EXPECT_EQ(txt::format_number(0.256, loc("en"), {.style = txt::number_style::percent}), string("26%"));
    EXPECT_EQ(txt::format_number(0.256, loc("de"), {.style = txt::number_style::percent}), string("26\xC2\xA0%"));
    EXPECT_EQ(txt::format_number(0.0123, loc("en"), {.style = txt::number_style::permille}), string("12\xE2\x80\xB0"));
    EXPECT_EQ(txt::format_number(1234.5678, loc("en"), {.style = txt::number_style::scientific}), string("1.235E3"));
    EXPECT_EQ(txt::format_number(1234567, loc("pl"), {.style = txt::number_style::compact}), string("1,2\xC2\xA0mln"));
    EXPECT_EQ(txt::format_number(999999, loc("en"), {.style = txt::number_style::compact}), string("1M"));
    EXPECT_EQ(txt::format_number(1000, loc("it"), {.style = txt::number_style::compact_long}), string("mille"));
    EXPECT_EQ(txt::format_number(5000000, loc("pl"), {.style = txt::number_style::compact_long}),
              string("5 milion\xC3\xB3w"));
    EXPECT_EQ(txt::format_number(1e21, loc("en"), {.style = txt::number_style::compact}), string("1,000,000,000T"));
    string huge = txt::format_number(1e300, loc("pl"), {.style = txt::number_style::compact_long});
    EXPECT_TRUE(huge.view().ends_with(" bilion\xC3\xB3w"));    // the plural of a huge one, from its last digits
    EXPECT_EQ(txt::format_number(-1e300, loc("fr"), {.style = txt::number_style::currency_compact,
                                                   .currency = txt::currency(string("EUR"))}).empty(), false);
    EXPECT_EQ(txt::format_number(-1234, loc("sw"), {.style = txt::number_style::compact}),
              string("elfu\xC2\xA0-1.2"));                                          // a negative form of its own
}

TEST(Cldr_Number_Tests, Boundaries) {
    auto en = loc("en");
    EXPECT_EQ(txt::format_number(0, en), string("0"));
    EXPECT_EQ(txt::format_number(-0.0, en), string("-0"));                       // as ICU
    EXPECT_EQ(txt::format_number(-0.0, en, {.sign = txt::sign_display::negative}), string("0"));
    EXPECT_EQ(txt::format_number(-0.0001, en), string("-0"));
    EXPECT_EQ(txt::format_number(std::numeric_limits<double>::quiet_NaN(), en), string("NaN"));
    EXPECT_EQ(txt::format_number(std::numeric_limits<double>::infinity(), en), string("\xE2\x88\x9E"));
    EXPECT_EQ(txt::format_number(-std::numeric_limits<double>::infinity(), en), string("-\xE2\x88\x9E"));
    EXPECT_EQ(txt::format_number(std::numeric_limits<long long>::min(), en), string("-9,223,372,036,854,775,808"));
    EXPECT_EQ(txt::format_number(std::numeric_limits<unsigned long long>::max(), en),
              string("18,446,744,073,709,551,615"));
    EXPECT_EQ(txt::format_number(char(65), en), string("65"));
    EXPECT_EQ(txt::format_number(1.5f, en), string("1.5"));
    EXPECT_EQ(txt::format_number(1e300, en).size(), 301u + 100u);                 // every group written
    EXPECT_EQ(txt::format_number(5e-324, en, {.max_fraction = 400}).size(), 2u + 324u);
    EXPECT_EQ(txt::format_number(1.0, en, {.min_fraction = 5000}).size(), 2u + 1000u);   // at most 1000
    EXPECT_EQ(txt::format_number(1.0, en, {.min_integer = 4, .min_fraction = 2}), string("0,001.00"));
    EXPECT_EQ(txt::format_number(1234567, en, {.grouping = false}), string("1234567"));
    // fraction and significant digits, the defaults given up to them
    EXPECT_EQ(txt::format_number(1.5, en, {.min_fraction = 5}), string("1.50000"));
    EXPECT_EQ(txt::format_number(1.5, en, {.max_fraction = 0}), string("2"));
    EXPECT_EQ(txt::format_number(2.5, en, {.max_fraction = 0}), string("2"));
    EXPECT_EQ(txt::format_number(1.0, en, {.min_significant = 3, .max_significant = 3}), string("1.00"));
    EXPECT_EQ(txt::format_number(0.0, en, {.min_significant = 3, .max_significant = 3}), string("0.00"));
    EXPECT_EQ(txt::format_number(123456, en, {.max_significant = 2}), string("120,000"));
    EXPECT_EQ(txt::format_number(0.000123456, en, {.max_significant = 2}), string("0.00012"));
    // every rounding of sgcl::rounding; unnecessary drops nothing
    struct Case {
        sgcl::rounding mode;
        const char* plus;
        const char* minus;
    } cases[] = {
        {sgcl::rounding::half_even, "2", "-2"}, {sgcl::rounding::half_up, "3", "-3"},
        {sgcl::rounding::half_down, "2", "-2"}, {sgcl::rounding::up, "3", "-3"}, {sgcl::rounding::down, "2", "-2"},
        {sgcl::rounding::ceiling, "3", "-2"}, {sgcl::rounding::floor, "2", "-3"},
        {sgcl::rounding::unnecessary, "2.5", "-2.5"},
    };
    for (auto c : cases) {
        EXPECT_EQ(txt::format_number(2.5, en, {.max_fraction = 0, .mode = c.mode}), string(c.plus)) << int(c.mode);
        EXPECT_EQ(txt::format_number(-2.5, en, {.max_fraction = 0, .mode = c.mode}), string(c.minus)) << int(c.mode);
    }
    EXPECT_EQ(txt::format_number(2.51, en, {.max_fraction = 0, .mode = sgcl::rounding::half_down}), string("3"));
    // signs
    EXPECT_EQ(txt::format_number(5, en, {.sign = txt::sign_display::always}), string("+5"));
    EXPECT_EQ(txt::format_number(0, en, {.sign = txt::sign_display::always}), string("+0"));
    EXPECT_EQ(txt::format_number(0, en, {.sign = txt::sign_display::except_zero}), string("0"));
    EXPECT_EQ(txt::format_number(-5, en, {.sign = txt::sign_display::never}), string("5"));
}

TEST(Cldr_Number_Tests, DecimalText) {
    txt::number_format nf(loc("en"));
    auto big = nf.format(string("123456789012345678901234567890.123"));
    ASSERT_TRUE(big);
    EXPECT_EQ(*big, string("123,456,789,012,345,678,901,234,567,890.123"));
    EXPECT_EQ(*nf.format(string("-0.0005")), string("-0"));          // a tie, to the even 0
    EXPECT_EQ(*nf.format(string("-0.0015")), string("-0.002"));
    EXPECT_EQ(*nf.format(string("+1.5e3")), string("1,500"));
    EXPECT_EQ(*nf.format(string("1E-2")), string("0.01"));
    EXPECT_EQ(*nf.format(string(".5")), string("0.5"));
    EXPECT_EQ(*nf.format(string("5.")), string("5"));
    EXPECT_EQ(*nf.format(string("000")), string("0"));
    for (const char* bad : {"", "-", ".", "e5", "1e", "1.2.3", "12a", " 1", "1 ", "--1", "0x10", "1e+", "inf"}) {
        EXPECT_FALSE(nf.format(string(bad))) << bad;
    }
    // past 200 significant digits: rounded there, half to even by what follows
    std::string digits = "1" + std::string(199, '0') + "5" + std::string(50, '0') + "1";
    auto r = txt::number_format(loc("en"), {.grouping = false}).format(string(digits.c_str()));
    ASSERT_TRUE(r);
    EXPECT_EQ(std::string(r->c_str()), "1" + std::string(198, '0') + "1" + std::string(52, '0'));
}

TEST(Cldr_Number_Tests, FormatTo) {
    txt::number_format nf(loc("pl"));
    char room[64];
    size_t n = nf.format_to(slice<char>(room, sizeof room), 1234567.5);
    EXPECT_EQ(std::string(room, n), "1\xC2\xA0" "234\xC2\xA0" "567,5");
    char small[4];
    size_t whole = nf.format_to(slice<char>(small, sizeof small), 1234567.5);
    EXPECT_EQ(whole, n);                                          // what the whole takes
    EXPECT_EQ(std::string(small, 4), std::string(room, 4));      // what fitted
    EXPECT_EQ(nf.format_to(slice<char>(), 7), 1u);
    EXPECT_EQ(nf.where(), loc("pl"));
    EXPECT_EQ(txt::number_format().format(1234.5), string("1,234.5"));
    // a long text past the room on the stack is written in a second pass
    txt::number_format wide(loc("en"), {.min_integer = 150});
    EXPECT_EQ(wide.format(1).size(), 150u + 49u);
}

TEST(Cldr_Number_Tests, Currencies) {
    auto pln = txt::currency(string("PLN"));
    EXPECT_EQ(txt::format_currency(12.5, pln, loc("pl")), string("12,50\xC2\xA0z\xC5\x82"));
    EXPECT_EQ(txt::format_currency(12.5, pln, loc("en")), string("PLN\xC2\xA0" "12.50"));
    EXPECT_EQ(txt::format_currency(12.5, pln, loc("en"), txt::currency_display::code), string("PLN\xC2\xA0" "12.50"));
    EXPECT_EQ(txt::format_currency(5, txt::currency(string("USD")), loc("en")), string("$5.00"));
    EXPECT_EQ(txt::format_currency(5, txt::currency(string("USD")), loc("pl")), string("5,00\xC2\xA0USD"));
    EXPECT_EQ(txt::format_currency(5, txt::currency(string("USD")), loc("pl"), txt::currency_display::narrow_symbol),
              string("5,00\xC2\xA0$"));
    EXPECT_EQ(txt::format_currency(1234.5, txt::currency(string("JPY")), loc("ja")), string("\xEF\xBF\xA5" "1,234"));   // half to even
    EXPECT_EQ(txt::format_currency(1.2345, txt::currency(string("KWD")), loc("en")), string("KWD\xC2\xA0" "1.234"));
    EXPECT_EQ(txt::format_currency(-3.456, txt::currency(string("EUR")), loc("en"),
                                   txt::currency_display::symbol), string("-\xE2\x82\xAC" "3.46"));
    EXPECT_EQ(txt::format_number(-3.456, loc("en"), {.style = txt::number_style::accounting,
                                                     .currency = txt::currency(string("EUR"))}),
              string("(\xE2\x82\xAC" "3.46)"));
    EXPECT_EQ(txt::format_number(1234567, loc("en"), {.style = txt::number_style::currency_compact,
                                                      .currency = txt::currency(string("USD"))}),
              string("$1.2M"));
    // no currency given: the locale's region's
    EXPECT_EQ(txt::format_number(5, loc("pl"), {.style = txt::number_style::currency}), string("5,00\xC2\xA0z\xC5\x82"));
    EXPECT_EQ(txt::format_number(5, loc("de-CH"), {.style = txt::number_style::currency}), string("CHF\xC2\xA0" "5.00"));
    // cash: the Swiss franc in steps of 0.05
    txt::number_options cash{.style = txt::number_style::currency, .currency = txt::currency(string("CHF")), .cash = true};
    EXPECT_EQ(txt::format_number(1.23, loc("de-CH"), cash), string("CHF\xC2\xA0" "1.25"));
    EXPECT_EQ(txt::format_number(1.22, loc("de-CH"), cash), string("CHF\xC2\xA0" "1.20"));
    EXPECT_EQ(txt::format_number(1.225, loc("de-CH"), cash), string("CHF\xC2\xA0" "1.20"));   // a tie to even steps
    EXPECT_EQ(txt::format_number(1.975, loc("de-CH"), cash), string("CHF\xC2\xA0" "2.00"));
    EXPECT_EQ(txt::format_number(-0.024, loc("de-CH"), cash), string("CHF-0.00"));
    // a currency ISO does not list: two digits, its code
    EXPECT_EQ(txt::format_currency(5, txt::currency(string("XYZ")), loc("en")), string("XYZ\xC2\xA0" "5.00"));
}

TEST(Cldr_Currency_Tests, Value) {
    auto c = txt::currency::parse(string("pln"));
    ASSERT_TRUE(c);
    EXPECT_EQ(c->code(), string("PLN"));
    EXPECT_EQ(*c, txt::currency(string("PLN")));
    EXPECT_EQ(c->digits(), 2);
    EXPECT_EQ(txt::currency(string("JPY")).digits(), 0);
    EXPECT_EQ(txt::currency(string("KWD")).digits(), 3);
    EXPECT_EQ(txt::currency(string("HUF")).cash_digits(), 0);
    EXPECT_EQ(txt::currency(string("CHF")).cash_increment(), 5);
    EXPECT_EQ(txt::currency(string("EUR")).cash_increment(), 0);
    EXPECT_EQ(txt::currency(string("PLN")).symbol(loc("pl")), string("z\xC5\x82"));
    EXPECT_EQ(txt::currency(string("PLN")).symbol(loc("en")), string("PLN"));
    EXPECT_EQ(txt::currency(string("USD")).symbol(loc("en")), string("$"));
    EXPECT_EQ(txt::currency(string("USD")).symbol(loc("en-CA")), string("US$"));
    EXPECT_EQ(txt::currency(string("CAD")).symbol(loc("en-CA")), string("$"));
    EXPECT_EQ(txt::currency(string("USD")).narrow_symbol(loc("pl")), string("$"));
    EXPECT_EQ(txt::currency(string("CHF")).narrow_symbol(loc("en")), string("CHF"));
    EXPECT_EQ(txt::currency::of(loc("pl")), txt::currency(string("PLN")));
    EXPECT_EQ(txt::currency::of(loc("de-CH")), txt::currency(string("CHF")));
    EXPECT_EQ(txt::currency::of(loc("en")), txt::currency(string("USD")));
    EXPECT_EQ(txt::currency::of(loc("pt")), txt::currency(string("BRL")));
    EXPECT_FALSE(txt::currency::of(loc("und-AQ")));
    EXPECT_FALSE(txt::currency());
    EXPECT_EQ(txt::currency().code(), string());
    EXPECT_EQ(txt::currency().digits(), 2);
    static_assert(std::is_trivially_copyable_v<txt::currency>);
    // what is not a code: the offset of the byte that is not a letter, or
    // of where the third letter should have been
    struct Bad {
        const char* text;
        size_t offset;
    } bad[] = {{"", 0}, {"PL", 2}, {"PLNX", 3}, {"P1N", 1}, {"P N", 1}, {"\xC5\x82", 0}, {"12", 0}};
    for (auto b : bad) {
        auto r = txt::currency::parse(string(b.text));
        ASSERT_FALSE(r) << b.text;
        EXPECT_EQ(r.error().offset(), b.offset) << b.text;
        EXPECT_EQ(r.error().what(), txt::code_error::kind::currency);
        EXPECT_FALSE(r.error().message().empty());
    }
    EXPECT_THROW(txt::currency(string("PL")), bad_expected_access<txt::code_error>);
}

//------------------------------------------------------------------------------
// lists and relative time
//------------------------------------------------------------------------------
TEST(Cldr_List_Tests, Vectors) {
    check_vectors(ListVectors);
}

TEST(Cldr_List_Tests, Forms) {
    auto pl = loc("pl");
    EXPECT_EQ(txt::format_list(slice<const string>(), pl), string());
    EXPECT_EQ(txt::format_list({"Ala"}, pl), string("Ala"));
    EXPECT_EQ(txt::format_list({"Ala", "Ola"}, pl), string("Ala i Ola"));
    EXPECT_EQ(txt::format_list({"Ala", "Ola", "Ela"}, pl), string("Ala, Ola i Ela"));
    EXPECT_EQ(txt::format_list({"a", "b", "c", "d"}, loc("en"), txt::list_type::disjunction), string("a, b, c, or d"));
    EXPECT_EQ(txt::format_list({"3 h", "5 min"}, loc("en"), txt::list_type::unit, txt::width::narrow),
              string("3 h 5 min"));
    EXPECT_EQ(txt::format_list({"a", "b", "c"}, loc("en"), txt::list_type::conjunction, txt::width::abbreviated),
              string("a, b, & c"));
    // Spanish: y before i, o before o; Hebrew: a hyphen before Latin
    EXPECT_EQ(txt::format_list({"Juan", "Irene"}, loc("es")), string("Juan e Irene"));
    EXPECT_EQ(txt::format_list({"Juan", "Hilda"}, loc("es")), string("Juan e Hilda"));
    EXPECT_EQ(txt::format_list({"agua", "hielo"}, loc("es")), string("agua y hielo"));
    EXPECT_EQ(txt::format_list({"uno", "otro"}, loc("es"), txt::list_type::disjunction), string("uno u otro"));
    EXPECT_EQ(txt::format_list({"7", "8"}, loc("es-MX"), txt::list_type::disjunction), string("7 u 8"));
    EXPECT_EQ(txt::format_list({"10", "11"}, loc("es"), txt::list_type::disjunction), string("10 u 11"));
    EXPECT_EQ(txt::format_list({"10", "110"}, loc("es"), txt::list_type::disjunction), string("10 o 110"));
    EXPECT_EQ(txt::format_list({"\xD7\x99\xD7\x95\xD7\xA1\xD7\x99", "Bob"}, loc("he")),
              string("\xD7\x99\xD7\x95\xD7\xA1\xD7\x99 \xD7\x95-Bob"));
    // a long list past the room on the stack
    vector<string> many;
    for (int i : range(100)) {
        many.push_back(string(std::to_string(i).c_str()));
    }
    string all = txt::format_list(many, loc("en"));
    EXPECT_EQ(all.view().substr(0, 9), std::string_view("0, 1, 2, "));
    EXPECT_EQ(all.view().substr(all.size() - 10), std::string_view("98, and 99"));
}

TEST(Cldr_Relative_Tests, Vectors) {
    check_vectors(RelativeVectors);
}

TEST(Cldr_Relative_Tests, Forms) {
    auto pl = loc("pl");
    EXPECT_EQ(txt::format_relative(-1, txt::time_unit::day, pl), string("wczoraj"));
    EXPECT_EQ(txt::format_relative(-2, txt::time_unit::day, pl), string("przedwczoraj"));
    EXPECT_EQ(txt::format_relative(-1, txt::time_unit::day, pl, {.numeric = true}), string("1 dzie\xC5\x84 temu"));
    EXPECT_EQ(txt::format_relative(3, txt::time_unit::day, pl), string("za 3 dni"));
    EXPECT_EQ(txt::format_relative(5, txt::time_unit::day, pl), string("za 5 dni"));
    EXPECT_EQ(txt::format_relative(1.5, txt::time_unit::day, pl), string("za 1,5 dnia"));
    EXPECT_EQ(txt::format_relative(-0.995, txt::time_unit::day, loc("en")), string("yesterday"));   // ICU's 1%
    EXPECT_EQ(txt::format_relative(0, txt::time_unit::second, loc("en")), string("now"));
    EXPECT_EQ(txt::format_relative(-0.0, txt::time_unit::day, loc("en"), {.numeric = true}), string("0 days ago"));
    EXPECT_EQ(txt::format_relative(2, txt::time_unit::hour, loc("en"), {.width = txt::width::narrow}),
              string("in 2h"));
    EXPECT_EQ(txt::format_relative(1234.5, txt::time_unit::year, loc("de")), string("in 1.234,5 Jahren"));
    EXPECT_EQ(txt::format_relative(std::nan(""), txt::time_unit::day, loc("en")), string("in NaN days"));
    using namespace std::chrono_literals;
    EXPECT_EQ(txt::format_relative(duration(-90min), loc("de")), string("vor 2 Stunden"));
    EXPECT_EQ(txt::format_relative(duration(30s), loc("en")), string("in 30 seconds"));
    EXPECT_EQ(txt::format_relative(duration(0s), loc("en")), string("now"));
    EXPECT_EQ(txt::format_relative(duration(-24h), loc("en")), string("yesterday"));
    EXPECT_EQ(txt::format_relative(duration(24h * 10), loc("en")), string("next week"));
    EXPECT_EQ(txt::format_relative(duration(24h * 20), loc("en")), string("in 3 weeks"));
    EXPECT_EQ(txt::format_relative(duration(24h * 100), loc("en")), string("in 3 months"));
    EXPECT_EQ(txt::format_relative(duration(24h * 800), loc("en")), string("in 2 years"));
    EXPECT_EQ(txt::format_relative(duration::max(), loc("en")), string("in 292 years"));
}
