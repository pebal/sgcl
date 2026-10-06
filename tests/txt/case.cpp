//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
    // and of the three capitals that carry their accent precomposed,
    // whatever follows them (found by the fuzzer against ICU,
    // tests/txt/fuzz/txt_icu_fuzz.cpp: they were lowered as the root lowers)
    EXPECT_EQ(txt::to_lower_full(string("\u00cc\u00cd\u0128"), lt), string("i\u0307\u0300i\u0307\u0301i\u0307\u0303"));
    EXPECT_EQ(txt::to_lower_full(string("R\u00cdGA"), lt), string("ri\u0307\u0301ga"));
    EXPECT_EQ(txt::to_lower_full(string("\u00cd"), root), string("\u00ed"));

    // A tag is read as BCP-47 and an unknown one is the root locale
    EXPECT_EQ(txt::locale(string("TR")), txt::locale::turkish());
    // a locale is its language, script and region (DESIGN 495): Turkish in
    // Turkey is not Turkish alone, but its language is
    EXPECT_NE(txt::locale(string("tr-TR")), txt::locale::turkish());
    EXPECT_EQ(txt::locale(string("tr-TR")).subtag(), txt::locale::turkish().subtag());
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

// Found by the fuzzer against ICU (tests/txt/fuzz/txt_icu_fuzz.cpp): a
// letter whose title case is its full upper case of two or three code
// points (the title table holds only where the two differ) kept itself,
// the simple upper mapping, which it has none of, taken instead
TEST(Case_Tests, TitleCaseThatIsTheFullUpperCase) {
    EXPECT_EQ(txt::to_title(string("ΰab ΰ")), string("Ϋ́ab Ϋ́"));
    EXPECT_EQ(txt::to_title(string("ǰob")), string("J̌ob"));
    EXPECT_EQ(txt::to_title(string("ẖ")), string("H̱"));
    EXPECT_EQ(txt::to_title(string("ŉ")), string("ʼN"));
}

// DESIGN 408: the empty text, one code point, broken UTF-8, the
// conditions of SpecialCasing at the very ends of a text, growth, a text
// mapped into itself, and the tags a locale is read from
TEST(Case_Tests, TheEdges) {
    auto tr = txt::locale::turkish();
    auto lt = txt::locale::lithuanian();
    for (auto where : {txt::locale(), tr, lt}) {
        EXPECT_EQ(txt::to_lower_full(string(), where), string());
        EXPECT_EQ(txt::to_upper_full(string(), where), string());
        EXPECT_EQ(txt::to_title(string(), where), string());
    }
    EXPECT_EQ(txt::fold_case(string()), string());

    // Unchanged: an ASCII text with the root locale is the same object,
    // any other text an equal one
    string plain("abc 123");
    EXPECT_EQ(txt::to_lower_full(plain).data(), plain.data());
    EXPECT_EQ(txt::fold_case(plain).data(), plain.data());
    string upper("ABC 123");
    EXPECT_EQ(txt::to_upper_full(upper).data(), upper.data());
    EXPECT_EQ(txt::to_lower_full(string("żółw")), string("żółw"));
    EXPECT_EQ(txt::to_lower_full(plain, tr), plain);

    // Broken UTF-8: a replacement a byte, the letters around it mapped
    EXPECT_EQ(txt::to_upper_full(string("a\xFF" "b")), string("A�B"));
    EXPECT_EQ(txt::to_lower_full(string("\xE2\x82" "A")), string("��a"));
    EXPECT_EQ(txt::fold_case(string("\xC3")), string("�"));
    EXPECT_EQ(txt::to_title(string("\xFF" "ab")), string("�Ab"));

    // The final sigma: alone, last, first, before an invalid byte
    EXPECT_EQ(txt::to_lower_full(string("Σ")), string("σ"));                  // no letter before it
    EXPECT_EQ(txt::to_lower_full(string("ΑΣ")), string("ας"));                // the last code point of the text
    EXPECT_EQ(txt::to_lower_full(string("ΣΑ")), string("σα"));
    EXPECT_EQ(txt::to_lower_full(string("ΑΣ\xFF")), string("ας�"));     // a replacement is not cased
    EXPECT_EQ(txt::to_lower_full(string("\xFFΣ")), string("�σ"));

    // The conditions of Turkish and Lithuanian at the ends of the text
    EXPECT_EQ(txt::to_lower_full(string("̇"), tr), string("̇"));   // a dot with no I before it
    EXPECT_EQ(txt::to_lower_full(string("I"), lt), string("i"));             // no accent after it
    EXPECT_EQ(txt::to_upper_full(string("̇"), lt), string("̇"));   // a dot with no i before it
    EXPECT_EQ(txt::to_lower_full(string("Iİ"), tr), string("ıi"));

    // Growth: each ß three times over, and a ligature into three letters
    std::string many;
    for (int i = 0; i < 1000; ++i) {
        many += "ß";
    }
    auto grown = txt::to_upper_full(string(many.data(), many.size()));
    EXPECT_EQ(grown.size(), 2000u);
    EXPECT_EQ(txt::to_upper_full(string("ﬃ")), string("FFI"));
    EXPECT_EQ(txt::fold_case(string("ﬃ")), string("ffi"));
    EXPECT_TRUE(txt::equal_fold_full(string("ﬃ"), string("FFI")));
    EXPECT_TRUE(txt::equal_fold_full(string("ß"), string("ss")));
    EXPECT_FALSE(txt::equal_fold_full(string("ß"), string("s")));
    EXPECT_FALSE(txt::equal_fold_full(string(), string("a")));

    // Into itself
    string self("Straße");
    self = txt::to_upper_full(self);
    EXPECT_EQ(self, string("STRASSE"));
    self = txt::to_title(self);
    EXPECT_EQ(self, string("Strasse"));
    EXPECT_TRUE(txt::equal_fold_full(self, self));

    // Tags: separators at the edges, lengths at the limits, other characters
    EXPECT_EQ(txt::locale(string("tr-")), tr);
    EXPECT_EQ(txt::locale(string("tr_TR")).subtag(), tr.subtag());
    EXPECT_TRUE(txt::locale(string("tr_TR")).dotted_i());
    EXPECT_EQ(txt::locale(string("-tr")), txt::locale::root());
    EXPECT_EQ(txt::locale(string("t")), txt::locale::root());
    EXPECT_NE(txt::locale(string("tur")), txt::locale::root());              // three letters are a subtag
    EXPECT_NE(txt::locale(string("tur")), tr);
    EXPECT_EQ(txt::locale(string("turk")), txt::locale::root());
    EXPECT_EQ(txt::locale(string("t1")), txt::locale::root());
    EXPECT_EQ(txt::locale(string("tr\0", 3)), txt::locale::root());
    EXPECT_EQ(txt::locale(string("\xC5\x82t")), txt::locale::root());
    EXPECT_EQ(txt::locale::turkish().subtag(), (uint32_t('t') << 8) | 'r');
    EXPECT_EQ(txt::locale::root().subtag(), 0u);
    EXPECT_FALSE(txt::locale::lithuanian().dotted_i());
    EXPECT_FALSE(txt::locale::turkish().keeps_dot());
    EXPECT_TRUE(txt::locale::azerbaijani().dotted_i());
}

// A POSIX locale name, what LANG holds, is read up to the '.' of its
// codeset and the '@' of its modifier too (after DESIGN 429)
TEST(Case_Tests, APosixLocaleName) {
    auto tr = txt::locale::turkish();
    EXPECT_EQ(txt::locale(string("tr.UTF-8")), tr);
    EXPECT_EQ(txt::locale(string("tr_TR.UTF-8")).subtag(), tr.subtag());
    EXPECT_TRUE(txt::locale(string("tr_TR.UTF-8")).dotted_i());
    EXPECT_EQ(txt::locale(string("tr_TR.ISO-8859-9@euro")).subtag(), tr.subtag());
    EXPECT_TRUE(txt::locale(string("tr_TR.ISO-8859-9@euro")).dotted_i());
    EXPECT_EQ(txt::locale(string("tr@euro")), tr);
    EXPECT_EQ(txt::locale(string("TR.utf8")), tr);
    EXPECT_EQ(txt::locale(string("tr.")), tr);
    // the region is kept since DESIGN 495, the language as before
    EXPECT_EQ(txt::locale(string("pl_PL.UTF-8")), txt::locale(string("pl-PL")));
    EXPECT_EQ(txt::locale(string("pl_PL.UTF-8")).subtag(), txt::locale(string("pl")).subtag());
    EXPECT_TRUE(txt::locale(string("lt_LT.UTF-8")).keeps_dot());
    EXPECT_TRUE(txt::locale(string("az_AZ@latin")).dotted_i());
    // the modifiers @latin and @cyrillic are the script (DESIGN 495)
    EXPECT_EQ(txt::locale(string("sr@latin")), txt::locale(string("sr-Latn")));
    EXPECT_EQ(txt::locale(string("sr_RS@latin")), txt::locale(string("sr-Latn-RS")));
    EXPECT_EQ(txt::locale(string("az_AZ@latin")), txt::locale(string("az-Latn-AZ")));
    // the names that are no language
    EXPECT_EQ(txt::locale(string("C.UTF-8")), txt::locale::root());
    EXPECT_EQ(txt::locale(string("C")), txt::locale::root());
    EXPECT_EQ(txt::locale(string("POSIX")), txt::locale::root());
    EXPECT_EQ(txt::locale(string(".UTF-8")), txt::locale::root());
    EXPECT_EQ(txt::locale(string("@euro")), txt::locale::root());
    EXPECT_EQ(txt::locale(string("t.UTF-8")), txt::locale::root());
    EXPECT_EQ(txt::locale(string("turk.UTF-8")), txt::locale::root());
    // and what the language does once read so
    EXPECT_EQ(txt::to_upper_full(string("i"), txt::locale(string("tr_TR.UTF-8"))), string("İ"));
    txt::collator danish(txt::locale(string("da_DK.UTF-8")));
    EXPECT_TRUE(danish.tailored());
    EXPECT_TRUE(danish.capitals_first());
}
