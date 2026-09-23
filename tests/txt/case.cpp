//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::case: the full case mappings. Two oracles: what Python makes of
// every code point that has a case and of the strings the conditions of
// SpecialCasing.txt are about (the rule of the final sigma among them),
// and the conditional mappings of the three languages, which Python does
// not do and which are written out here from the file.
#include "tests/types.h"
#include "tests/txt/case_tests.h"

#include <string>

namespace {
    string utf8_of(const char32_t* points) {
        std::string out;
        char buf[utf8::max_width];
        for (size_t i = 0; points[i]; ++i) {
            out.append(buf, utf8::encode(points[i], buf));
        }
        return string(out.data(), out.size());
    }

    std::string describe(const char32_t* points) {
        std::string out;
        char buf[16];
        for (size_t i = 0; points[i]; ++i) {
            snprintf(buf, sizeof(buf), "%04X ", uint32_t(points[i]));
            out += buf;
        }
        return out;
    }
}

TEST(Case_Tests, EveryCodePointThatHasACase) {
    size_t n = 0;
    for (const auto& c : ucd::CasePoints) {
        string source = utf8_of(c.source);
        ASSERT_EQ(txt::to_lower_full(source), utf8_of(c.lower)) << describe(c.source);
        ASSERT_EQ(txt::to_upper_full(source), utf8_of(c.upper)) << describe(c.source);
        ASSERT_EQ(txt::fold_case(source), utf8_of(c.fold)) << describe(c.source);
        ++n;
    }
    EXPECT_EQ(n, std::size(ucd::CasePoints));
    EXPECT_GT(n, 2900u);
}

TEST(Case_Tests, TheStringsTheConditionsAreAbout) {
    for (const auto& c : ucd::CaseStrings) {
        string source = utf8_of(c.source);
        ASSERT_EQ(txt::to_lower_full(source), utf8_of(c.lower)) << describe(c.source);
        ASSERT_EQ(txt::to_upper_full(source), utf8_of(c.upper)) << describe(c.source);
        ASSERT_EQ(txt::fold_case(source), utf8_of(c.fold)) << describe(c.source);
    }
    EXPECT_GT(std::size(ucd::CaseStrings), 200u);
}

TEST(Case_Tests, ALetterThatBecomesTwo) {
    // What one code point at a time cannot do, which is why core's
    // simple mapping is not enough
    EXPECT_EQ(txt::to_upper_full(string("straße")), string("STRASSE"));
    EXPECT_EQ(txt::to_upper_full(string("\uFB01n")), string("FIN"));            // the fi ligature
    EXPECT_EQ(txt::to_upper_full(string("\u0390")), string("\u0399\u0308\u0301"));   // three code points
    EXPECT_EQ(txt::to_lower_full(string("\u0130")), string("i\u0307"));         // the Turkish I outside Turkish
    EXPECT_EQ(string("straße").to_upper(), string("STRAßE"));         // core's, one to one

    // The text comes back unchanged where no letter changes
    string plain = "already lower 123";
    EXPECT_EQ(txt::to_lower_full(plain), plain);
    EXPECT_EQ(txt::to_lower_full(string()), string());
    EXPECT_EQ(txt::to_upper_full(string("中文")), string("中文"));
}

TEST(Case_Tests, TheSigmaAtTheEndOfAWord) {
    // The one rule that looks around the letter: a Greek final sigma is
    // written differently from one inside a word
    EXPECT_EQ(txt::to_lower_full(string("\u03A3")), string("\u03C3"));          // alone: not final
    EXPECT_EQ(txt::to_lower_full(string("\u039F\u03A3")), string("\u03BF\u03C2"));   // after a letter: final
    EXPECT_EQ(txt::to_lower_full(string("\u039F\u03A3\u039F")), string("\u03BF\u03C3\u03BF"));
    EXPECT_EQ(txt::to_lower_full(string("\u039F\u03A3'")), string("\u03BF\u03C2'"));   // an apostrophe is ignored
    EXPECT_EQ(txt::to_lower_full(string("\u039F\u03A3 \u039F")), string("\u03BF\u03C2 \u03BF"));
    EXPECT_EQ(txt::to_upper_full(string("\u03C2")), string("\u03A3"));          // and back to one sigma
    EXPECT_TRUE(txt::equal_fold_full(string("\u03C2"), string("\u03C3")));
}

TEST(Case_Tests, TheThreeLanguagesThatSpellAnIDifferently) {
    auto tr = txt::locale(string("tr"));
    auto az = txt::locale(string("az-Latn-AZ"));
    auto lt = txt::locale(string("lt"));
    auto root = txt::locale();

    // Turkish and Azerbaijani: the dot is part of the letter
    EXPECT_EQ(txt::to_lower_full(string("I"), tr), string("\u0131"));           // dotless
    EXPECT_EQ(txt::to_lower_full(string("\u0130"), tr), string("i"));           // dotted, the dot not spelled out
    EXPECT_EQ(txt::to_upper_full(string("i"), tr), string("\u0130"));
    EXPECT_EQ(txt::to_lower_full(string("I\u0307"), tr), string("i"));          // the dot is absorbed
    EXPECT_EQ(txt::to_lower_full(string("I"), az), string("\u0131"));
    EXPECT_EQ(txt::to_upper_full(string("i"), az), string("\u0130"));

    // and outside them the root rules hold
    EXPECT_EQ(txt::to_lower_full(string("I"), root), string("i"));
    EXPECT_EQ(txt::to_upper_full(string("i"), root), string("I"));
    EXPECT_EQ(txt::to_lower_full(string("I")), string("i"));

    // Lithuanian keeps the dot of an i under an accent
    EXPECT_EQ(txt::to_lower_full(string("I\u0300"), lt), string("i\u0307\u0300"));
    EXPECT_EQ(txt::to_lower_full(string("J\u0300"), lt), string("j\u0307\u0300"));
    EXPECT_EQ(txt::to_lower_full(string("I\u0300"), root), string("i\u0300"));
    EXPECT_EQ(txt::to_upper_full(string("i\u0307"), lt), string("I"));

    // A tag is read as BCP-47 and an unknown one is the root locale
    EXPECT_EQ(txt::locale(string("TR")), txt::locale::turkish());
    EXPECT_EQ(txt::locale(string("tr-TR")), txt::locale::turkish());
    EXPECT_EQ(txt::locale(string("pl")), txt::locale(string("pl")));
    EXPECT_EQ(txt::locale(string("zzz-nonsense")), txt::locale(string("zzz")));
    EXPECT_EQ(txt::locale(string("x")), txt::locale::root());
    EXPECT_EQ(txt::locale(string()), txt::locale::root());
    EXPECT_EQ(txt::to_lower_full(string("I"), txt::locale(string("pl"))), string("i"));
    static_assert(txt::locale() == txt::locale::root());
    static_assert(txt::locale::turkish().dotted_i() && txt::locale::lithuanian().keeps_dot());
    static_assert(!txt::locale::root().dotted_i());
}

TEST(Case_Tests, TitleCaseAndFolding) {
    // The first letter of every word, the words being the ones UAX #29
    // finds, so an apostrophe does not start a new one
    EXPECT_EQ(txt::to_title(string("ala ma kota")), string("Ala Ma Kota"));
    EXPECT_EQ(txt::to_title(string("don't stop")), string("Don't Stop"));
    EXPECT_EQ(txt::to_title(string("\u0142ód\u017A nad wis\u0142\u0105")), string("\u0141ód\u017A Nad Wis\u0142\u0105"));
    EXPECT_EQ(txt::to_title(string("e-mail")), string("E-Mail"));
    EXPECT_EQ(txt::to_title(string("straße")), string("Straße"));           // only the first letter changes
    EXPECT_EQ(txt::to_title(string("\u01C6")), string("\u01C5"));                // the digraph has a title case of its own
    EXPECT_EQ(txt::to_title(string("123 abc")), string("123 Abc"));
    EXPECT_EQ(txt::to_title(string()), string());

    // Folding is for comparing, not for showing
    EXPECT_EQ(txt::fold_case(string("STRASSE")), string("strasse"));
    EXPECT_EQ(txt::fold_case(string("straße")), string("strasse"));
    EXPECT_EQ(txt::fold_case(string("\u03A3")), string("\u03C3"));
    EXPECT_EQ(txt::fold_case(string("\u017F")), string("s"));                    // the long s folds to s
    EXPECT_EQ(txt::fold_case(string("µ")), string("\u03BC"));               // micro to mu

    EXPECT_TRUE(txt::equal_fold_full(string("straße"), string("STRASSE")));
    EXPECT_TRUE(txt::equal_fold_full(string("\u0141ÓD\u0179"), string("\u0142ód\u017A")));
    EXPECT_FALSE(txt::equal_fold_full(string("a"), string("b")));
    EXPECT_TRUE(txt::equal_fold_full(string(), string()));

    // core's equal_fold is one code point to one, and says no here
    EXPECT_FALSE(string("straße").equal_fold("STRASSE"));
}
