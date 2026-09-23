//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::search: a pattern prepared once, a search blind to case, and one
// blind to the way a text was written. The oracle for the exact search is
// std::string_view::find over thousands of random patterns and texts; the
// other two are checked against what they are defined to do — the folding
// and the decomposition of the headers beside this one.
#include "tests/types.h"

#include <random>
#include <string>

namespace {
    string of(std::string_view s) {
        return string(s.data(), s.size());
    }
}

TEST(Search_Tests, ThePreparedPatternAgainstStdFind) {
    // Random texts over a small alphabet, where matches and near misses
    // are frequent, and random patterns cut out of them or made up
    std::mt19937 rng(1);
    const char* alphabet = "aabbc";
    size_t cases = 0;
    for (int round = 0; round < 4000; ++round) {
        std::string text;
        for (size_t i = 0, n = rng() % 40; i < n; ++i) {
            text += alphabet[rng() % 5];
        }
        std::string pattern;
        if (!text.empty() && rng() % 2) {
            size_t at = rng() % text.size();
            pattern = text.substr(at, 1 + rng() % 4);
        } else {
            for (size_t i = 0, n = rng() % 4; i < n; ++i) {
                pattern += alphabet[rng() % 5];
            }
        }
        txt::searcher s(of(pattern));
        size_t from = text.empty() ? 0 : rng() % text.size();
        ASSERT_EQ(s.find(of(text)), std::string_view(text).find(pattern)) << text << " / " << pattern;
        ASSERT_EQ(s.find(of(text), from), std::string_view(text).find(pattern, from)) << text << " / " << pattern;
        ++cases;
    }
    EXPECT_EQ(cases, 4000u);
}

TEST(Search_Tests, WhatThePreparedPatternAnswers) {
    string text = "Ala ma kota, a kot ma kota";
    txt::searcher kota(string("kota"));
    EXPECT_EQ(kota.find(text), 7u);
    EXPECT_EQ(kota.find(text, 8), 22u);
    EXPECT_EQ(kota.find(text, 23), npos);
    EXPECT_EQ(kota.count(text), 2u);
    EXPECT_TRUE(kota.contains(text));
    EXPECT_EQ(kota.pattern(), string("kota"));

    // The edges std::string draws the same way
    EXPECT_EQ(txt::searcher(string()).find(text), 0u);
    EXPECT_EQ(txt::searcher(string()).find(text, 5), 5u);
    EXPECT_EQ(txt::searcher(string()).count(text), 0u);
    EXPECT_EQ(txt::searcher(string("nie ma")).find(text), npos);
    EXPECT_EQ(txt::searcher(string("x")).find(string()), npos);
    EXPECT_EQ(txt::searcher(string("dłuższy niż tekst")).find(string("abc")), npos);

    // Overlapping occurrences are counted once, as a loop over find does
    EXPECT_EQ(txt::searcher(string("aa")).count(string("aaaa")), 2u);

    // The bytes are safe to search: a valid UTF-8 pattern cannot match
    // inside a character, because the encoding synchronises itself
    string japanese = "日本語";
    EXPECT_EQ(txt::searcher(string("本")).find(japanese), 3u);
    EXPECT_EQ(txt::searcher(string("¥")).find(japanese), npos);      // its bytes are inside 日, and it is not found
    EXPECT_EQ(txt::searcher(string("ż")).find(string("żółw")), 0u);
    EXPECT_EQ(txt::searcher(string("ó")).find(string("żółw")), 2u);
}

TEST(Search_Tests, SearchingWithoutRegardToCase) {
    // The position is in the text as it was given, not in any folded copy
    string text = "Die STRASSE ist lang";
    EXPECT_EQ(txt::find_fold(text, string("straße")), 4u);
    EXPECT_EQ(txt::find_fold(text, string("STRASSE")), 4u);
    EXPECT_EQ(txt::find_fold(text, string("ist")), 12u);
    EXPECT_EQ(txt::find_fold(text, string("IST")), 12u);
    EXPECT_EQ(txt::find_fold(text, string("nope")), npos);
    EXPECT_TRUE(txt::contains_fold(text, string("Straße")));

    // A letter that folds to two, and one that folds across alphabets
    EXPECT_EQ(txt::find_fold(string("straße"), string("STRASSE")), 0u);
    EXPECT_EQ(txt::find_fold(string("ΣΟΦΟΣ"), string("σοφος")), 0u);
    EXPECT_EQ(txt::find_fold(string("ΣΟΦΟΣ"), string("σοφoς")), npos);   // a Latin o is not a Greek one
    EXPECT_EQ(txt::find_fold(string("ŁÓDŹ"), string("łódź")), 0u);
    EXPECT_EQ(txt::find_fold(string("aßb"), string("SS")), 1u);

    // From a position, and the edges
    EXPECT_EQ(txt::find_fold(string("abcABC"), string("abc"), 1), 3u);
    EXPECT_EQ(txt::find_fold(string("abc"), string()), 0u);
    EXPECT_EQ(txt::find_fold(string(), string("a")), npos);
}

TEST(Search_Tests, SearchingWithoutRegardToHowItIsWritten) {
    // The same word written two ways, found either way round
    string composed = "café rue";                 // é in one code point
    string decomposed = "cafe\u0301 rue";         // and in two
    EXPECT_NE(composed, decomposed);
    EXPECT_EQ(txt::find_normalized(composed, string("cafe\u0301")), 0u);
    EXPECT_EQ(txt::find_normalized(decomposed, string("café")), 0u);
    EXPECT_EQ(txt::find_normalized(composed, string("café")), 0u);
    EXPECT_EQ(txt::find_normalized(decomposed, string("cafe\u0301")), 0u);
    EXPECT_TRUE(txt::contains_normalized(composed, string("cafe\u0301")));

    // The position is in the original text, wherever the match falls
    EXPECT_EQ(txt::find_normalized(string("rue café"), string("café")), 4u);
    EXPECT_EQ(txt::find_normalized(string("rue cafe\u0301"), string("café")), 4u);

    // The marks are put in order first, so their order does not matter
    EXPECT_EQ(txt::find_normalized(string("q\u0307\u0323x"), string("q\u0323\u0307")), 0u);

    // A Korean syllable against its jamo
    EXPECT_EQ(txt::find_normalized(string("\uAC01\uC790"), string("\u1100\u1161\u11A8")), 0u);

    // And what is not there is not found
    EXPECT_EQ(txt::find_normalized(composed, string("thé")), npos);
    EXPECT_EQ(txt::find_normalized(string(), string("a")), npos);
    EXPECT_EQ(txt::find_normalized(string("abc"), string()), 0u);
    EXPECT_EQ(txt::find_normalized(string("abcabc"), string("abc"), 1), 3u);
}

// The skip table holds a uint16_t, and a pattern whose length is a
// multiple of 65536 narrowed to a skip of zero: the search then stepped
// nowhere and never returned. A skip is capped now, which only costs
// steps — it cannot pass over a match.
TEST(Search_Tests, APatternLongerThanTheSkipTableCanCount) {
    for (size_t m : {size_t(65535), size_t(65536), size_t(65537), size_t(131072)}) {
        std::string pattern(m, 'a');
        pattern[m / 2] = 'q';
        std::string hay(m * 2 + 100, 'b');
        string p(pattern.data(), pattern.size());
        txt::searcher prepared(p);

        // Nowhere in it
        EXPECT_EQ(prepared.find(string(hay.data(), hay.size())), npos) << m;

        // At the front and at the back
        for (size_t at : {size_t(0), hay.size() - m}) {
            std::string one = hay;
            one.replace(at, m, pattern);
            string text(one.data(), one.size());
            EXPECT_EQ(prepared.find(text), at) << m << " at " << at;
            EXPECT_TRUE(prepared.contains(text)) << m;
            EXPECT_EQ(prepared.count(text), 1u) << m;
        }
    }
}
