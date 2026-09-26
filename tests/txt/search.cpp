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

    // The position of an occurrence, npos for none: what the searches
    // that give a position are compared with
    size_t pos_of(const sgcl::optional<txt::occurrence>& o) {
        return o ? o->pos : npos;
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
    EXPECT_EQ(txt::find_fold(text, string("straße"))->pos, 4u);
    EXPECT_EQ(txt::find_fold(text, string("STRASSE"))->pos, 4u);
    EXPECT_EQ(txt::find_fold(text, string("straße"))->size, 7u);          // the bytes of the text, not of the pattern
    EXPECT_EQ(txt::find_fold(string("straße"), string("STRASSE"))->size, 7u);   // "STRASSE" covers the seven bytes of "straße"
    EXPECT_EQ(txt::find_fold(text, string("ist"))->pos, 12u);
    EXPECT_EQ(txt::find_fold(text, string("IST"))->pos, 12u);
    EXPECT_FALSE(txt::find_fold(text, string("nope")));
    EXPECT_TRUE(txt::contains_fold(text, string("Straße")));

    // A letter that folds to two, and one that folds across alphabets
    EXPECT_EQ(txt::find_fold(string("straße"), string("STRASSE"))->pos, 0u);
    EXPECT_EQ(txt::find_fold(string("ΣΟΦΟΣ"), string("σοφος"))->pos, 0u);
    EXPECT_FALSE(txt::find_fold(string("ΣΟΦΟΣ"), string("σοφoς")));   // a Latin o is not a Greek one
    EXPECT_EQ(txt::find_fold(string("ŁÓDŹ"), string("łódź"))->pos, 0u);
    EXPECT_EQ(txt::find_fold(string("aßb"), string("SS"))->pos, 1u);

    // From a position, and the edges
    EXPECT_EQ(txt::find_fold(string("abcABC"), string("abc"), 1)->pos, 3u);
    EXPECT_EQ(txt::find_fold(string("abc"), string())->pos, 0u);
    EXPECT_FALSE(txt::find_fold(string(), string("a")));
}

TEST(Search_Tests, SearchingWithoutRegardToHowItIsWritten) {
    // The same word written two ways, found either way round
    string composed = "café rue";                 // é in one code point
    string decomposed = "cafe\u0301 rue";         // and in two
    EXPECT_NE(composed, decomposed);
    EXPECT_EQ(txt::find_normalized(composed, string("cafe\u0301"))->pos, 0u);
    EXPECT_EQ(txt::find_normalized(decomposed, string("café"))->pos, 0u);
    EXPECT_EQ(txt::find_normalized(composed, string("café"))->pos, 0u);
    EXPECT_EQ(txt::find_normalized(decomposed, string("cafe\u0301"))->pos, 0u);
    EXPECT_TRUE(txt::contains_normalized(composed, string("cafe\u0301")));

    // The position is in the original text, wherever the match falls
    EXPECT_EQ(txt::find_normalized(string("rue café"), string("café"))->pos, 4u);
    EXPECT_EQ(txt::find_normalized(string("rue cafe\u0301"), string("café"))->pos, 4u);

    // The marks are put in order first, so their order does not matter
    EXPECT_EQ(txt::find_normalized(string("q\u0307\u0323x"), string("q\u0323\u0307"))->pos, 0u);

    // A Korean syllable against its jamo
    EXPECT_EQ(txt::find_normalized(string("\uAC01\uC790"), string("\u1100\u1161\u11A8"))->pos, 0u);

    // And what is not there is not found
    EXPECT_FALSE(txt::find_normalized(composed, string("thé")));
    EXPECT_FALSE(txt::find_normalized(string(), string("a")));
    EXPECT_EQ(txt::find_normalized(string("abc"), string())->pos, 0u);
    EXPECT_EQ(txt::find_normalized(string("abcabc"), string("abc"), 1)->pos, 3u);
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

// find_fold and find_normalized build the whole text afresh on every
// call, so a loop over the occurrences is quadratic. The prepared forms
// build it once — a pattern (fold_searcher), a text asked many times
// (folded_text) and the range of every occurrence (fold_matches) — and
// they must answer exactly what the one-shot functions answer.
TEST(Search_Tests, ThePreparedTextAnswersWhatTheOneShotFunctionsDo) {
    std::mt19937 rng(7);
    const char* pieces[] = {
        "Ala", "MA", "kota", "straße", "STRASSE", "café", "café",
        "ΣΟΦΟΣ", "ŁÓDŹ", "łódź", " ", "q̣̇",
    };
    size_t cases = 0;
    for (int round = 0; round < 2000; ++round) {
        std::string raw;
        for (size_t i = 0, n = rng() % 8; i < n; ++i) {
            raw += pieces[rng() % 12];
        }
        std::string pat = pieces[rng() % 12];
        string text = of(raw), pattern = of(pat);

        txt::folded_text ft(text);
        txt::fold_searcher fs(pattern);
        txt::normalized_text nt(text);
        txt::normalized_searcher ns(pattern);
        for (size_t from : {size_t(0), raw.size() / 2, raw.size()}) {
            ASSERT_EQ(ft.find(fs, from), pos_of(txt::find_fold(text, pattern, from))) << raw << " / " << pat;
            ASSERT_EQ(nt.find(ns, from), pos_of(txt::find_normalized(text, pattern, from))) << raw << " / " << pat;
            ASSERT_EQ(fs.find(text, from), txt::find_fold(text, pattern, from)) << raw << " / " << pat;   // the searcher's own find
            ASSERT_EQ(ns.find(text, from), txt::find_normalized(text, pattern, from)) << raw << " / " << pat;
        }
        ASSERT_EQ(fs.count(text), ft.count(fs)) << raw << " / " << pat;
        ASSERT_EQ(ns.contains(text), nt.contains(ns)) << raw << " / " << pat;
        // and the form that takes the pattern as it stands
        ASSERT_EQ(ft.find(pattern), pos_of(txt::find_fold(text, pattern))) << raw << " / " << pat;
        ASSERT_EQ(nt.find(pattern), pos_of(txt::find_normalized(text, pattern))) << raw << " / " << pat;
        ASSERT_EQ(ft.contains(fs), txt::contains_fold(text, pattern)) << raw << " / " << pat;
        ASSERT_EQ(nt.contains(ns), txt::contains_normalized(text, pattern)) << raw << " / " << pat;

        // The range finds the first occurrence where find_fold finds it,
        // and counts what the prepared text counts
        auto matches = txt::fold_matches(text, pattern);
        ASSERT_EQ(matches.empty(), !txt::find_fold(text, pattern) || pat.empty())
            << raw << " / " << pat;
        if (!matches.empty()) {
            auto first = txt::find_fold(text, pattern);
            ASSERT_TRUE(first) << raw << " / " << pat;
            ASSERT_EQ(matches.begin().pos(), first->pos) << raw << " / " << pat;
            ASSERT_EQ(matches.begin().size(), first->size) << raw << " / " << pat;   // the bytes it covers, as the range has them
        }
        ASSERT_EQ(ft.count(fs), matches.count()) << raw << " / " << pat;
        ASSERT_EQ(nt.count(ns), txt::normalized_matches(text, pattern).count()) << raw << " / " << pat;
        ++cases;
    }
    EXPECT_EQ(cases, 2000u);
}

TEST(Search_Tests, EveryOccurrenceWithTheTextBuiltOnce) {
    string text = "Kot, KOT, kot i Kotek";
    std::vector<std::string> found;
    std::vector<size_t> at;
    for (auto it = txt::fold_matches(text, string("kot")); auto m : it) {
        found.emplace_back(m.data(), m.size());
    }
    for (auto it = txt::fold_matches(text, string("kot")).begin(),
              stop = txt::fold_matches(text, string("kot")).end(); it != stop; ++it) {
        at.push_back(it.pos());
    }
    ASSERT_EQ(found.size(), 4u);
    EXPECT_EQ(found[0], "Kot");
    EXPECT_EQ(found[1], "KOT");
    EXPECT_EQ(found[2], "kot");
    EXPECT_EQ(found[3], "Kot");
    EXPECT_EQ(at, (std::vector<size_t>{0, 5, 10, 16}));
    EXPECT_EQ(txt::fold_matches(text, string("kot")).count(), 4u);
    EXPECT_FALSE(txt::fold_matches(text, string("kot")).empty());
    static_assert(req::enumerable<txt::fold_matches>);
    static_assert(req::enumerable<txt::normalized_matches>);

    // The iterator says where the match is and how long it is, and that
    // is its length in the text and not in the folded copy: the six
    // bytes of "straße" answer a pattern of seven folded points
    string german = "Die STRASSE und die straße";
    auto folds = txt::fold_matches(german, string("Straße"));
    std::vector<std::pair<size_t, size_t>> spans;
    for (auto it = folds.begin(); it != folds.end(); ++it) {
        spans.emplace_back(it.pos(), it.size());
    }
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0], std::make_pair(size_t(4), size_t(7)));     // STRASSE, seven bytes
    EXPECT_EQ(spans[1], std::make_pair(size_t(20), size_t(7)));    // straße, ß being two of them

    // The occurrences do not overlap, as searcher::count counts them
    EXPECT_EQ(txt::fold_matches(string("aAaA"), string("aa")).count(), 2u);

    // An empty pattern matches nowhere: a range of every position is not
    // what anyone asking this question wants, and it would not end
    EXPECT_TRUE(txt::fold_matches(text, string()).empty());
    EXPECT_EQ(txt::fold_matches(text, string()).count(), 0u);
    EXPECT_TRUE(txt::normalized_matches(text, string()).empty());
    EXPECT_TRUE(txt::fold_matches(string(), string("a")).empty());

    // And the same range without regard to the way it was written
    string cafes = "café café cafe";
    std::vector<size_t> norm;
    auto normalized = txt::normalized_matches(cafes, string("café"));
    for (auto it = normalized.begin(); it != normalized.end(); ++it) {
        norm.push_back(it.pos());
    }
    EXPECT_EQ(norm, (std::vector<size_t>{0, 6}));       // both ways of writing it
    EXPECT_EQ(txt::normalized_matches(cafes, string("caf")).count(), 3u);
}

// The mapped text is too large to copy into an iterator, so the range
// holds it in a tracked object and the iterator points at that and not
// at the range: an iterator taken from a temporary must go on working
// after the temporary is gone. Pointing at the range read freed stack.
TEST(Search_Tests, AnIteratorOutlivesTheRangeItCameFrom) {
    string text = "Kot, KOT, kot i Kotek";
    auto it = txt::fold_matches(text, string("kot")).begin();
    auto stop = txt::fold_matches(text, string("kot")).end();
    collector::force_collect();
    std::vector<size_t> at;
    for (; it != stop; ++it) {
        at.push_back(it.pos());
        collector::force_collect();
    }
    EXPECT_EQ(at, (std::vector<size_t>{0, 5, 10, 16}));

    // and so must the slice it gives out, which owns the text
    auto first = *txt::fold_matches(text, string("kotek")).begin();
    collector::force_collect();
    EXPECT_EQ(std::string(first.data(), first.size()), "Kotek");
}

TEST(Search_Tests, WhatThePreparedPatternAndTextAnswerAboutThemselves) {
    txt::fold_searcher f(string("Straße"));
    EXPECT_EQ(f.pattern(), string("Straße"));
    EXPECT_EQ(f.size(), 7u);                 // ß folds to two
    EXPECT_FALSE(f.empty());
    EXPECT_TRUE(txt::fold_searcher(string()).empty());

    txt::normalized_searcher n(string("café"));
    EXPECT_EQ(n.pattern(), string("café"));
    EXPECT_EQ(n.size(), 5u);                 // é comes apart into two

    string text = "Ala ma kota";
    txt::folded_text ft(text);
    EXPECT_EQ(ft.size(), 11u);
    EXPECT_FALSE(ft.empty());
    EXPECT_EQ(ft.text().size(), text.size());
    EXPECT_TRUE(txt::folded_text(string()).empty());
    EXPECT_EQ(txt::folded_text(string()).find(txt::fold_searcher(string("a"))), npos);

    // One text, many questions
    EXPECT_EQ(ft.find(string("ALA")), 0u);
    EXPECT_EQ(ft.find(string("KOTA")), 7u);
    EXPECT_EQ(ft.find(string("MA")), 4u);
    EXPECT_EQ(ft.count(string("a")), 4u);
    EXPECT_FALSE(ft.contains(string("pies")));
}

// A match takes whole characters: it may not cut an expansion or a
// decomposition in two. Before this rule "s" found half of a "ß" and
// reported a position with no text at it — the match began and ended
// inside one character, so its slice was empty and there was nothing to
// cut out or to draw a box round.
TEST(Search_Tests, AMatchMayNotCutACharacterInTwo) {
    // The whole of what a ß folds to is found; half of it is not
    string sharp = "straße";
    EXPECT_EQ(txt::find_fold(sharp, string("ss"))->pos, 4u);
    EXPECT_EQ(txt::find_fold(sharp, string("SS"))->pos, 4u);
    EXPECT_EQ(txt::find_fold(sharp, string("ß"))->pos, 4u);
    EXPECT_EQ(txt::find_fold(sharp, string("aße"))->pos, 3u);
    EXPECT_FALSE(txt::find_fold(string("aßb"), string("s")));
    EXPECT_FALSE(txt::find_fold(string("aßb"), string("as")));
    EXPECT_FALSE(txt::find_fold(string("aßb"), string("sb")));
    EXPECT_FALSE(txt::contains_fold(string("aßb"), string("s")));

    // And the same where the text is decomposed rather than folded: an
    // "é" of one code point is not found by the "e" it comes apart into,
    // nor by the accent alone
    string composed = "café";
    EXPECT_FALSE(txt::find_normalized(composed, string("e")));
    EXPECT_FALSE(txt::find_normalized(composed, string("́")));
    EXPECT_FALSE(txt::find_normalized(composed, string("fe")));
    EXPECT_EQ(txt::find_normalized(composed, string("é"))->pos, 3u);
    EXPECT_EQ(txt::find_normalized(composed, string("é"))->pos, 3u);
    EXPECT_EQ(txt::find_normalized(composed, string("caf"))->pos, 0u);

    // And the same answers where the text is written out in full,
    // which is the point of the rule rather than a second case of it.
    // There the "e" is a character of its own, so the rule above has
    // nothing to say and it is the combining sequence that refuses the
    // match: a search blind to the way a text is written may not answer
    // two ways about two spellings of one text, and it once did.
    string apart = "café";
    ASSERT_NE(composed, apart);                  // two spellings, one text
    EXPECT_FALSE(txt::find_normalized(apart, string("e")));
    EXPECT_FALSE(txt::find_normalized(apart, string("fe")));
    EXPECT_EQ(txt::find_normalized(apart, string("caf"))->pos, 0u);
    // the whole letter, however either side spells it
    EXPECT_EQ(txt::find_normalized(apart, composed.substr(3))->pos, 3u);
    EXPECT_EQ(txt::find_normalized(composed, apart.substr(3))->pos, 3u);
    // and folding answers the same, a mark being no more a character
    // of its own there than it is here
    EXPECT_FALSE(txt::find_fold(apart, string("e")));
    EXPECT_FALSE(txt::find_fold(composed, string("e")));

    // A Korean syllable is three jamo and is taken whole or not at all
    EXPECT_EQ(txt::find_normalized(string("각"), string("각"))->pos, 0u);
    EXPECT_FALSE(txt::find_normalized(string("각"), string("ᄀ")));
    EXPECT_FALSE(txt::find_normalized(string("각"), string("가")));

    // The rule holds down every road, not only through the one-shot
    // functions: the prepared text, the prepared pattern and the range
    string half = "aßb";
    txt::folded_text ft(half);
    EXPECT_EQ(ft.find(txt::fold_searcher(string("s"))), npos);
    EXPECT_EQ(ft.count(txt::fold_searcher(string("s"))), 0u);
    EXPECT_FALSE(ft.contains(string("s")));
    EXPECT_TRUE(txt::fold_matches(half, string("s")).empty());
    EXPECT_EQ(txt::fold_matches(half, string("s")).count(), 0u);
    EXPECT_EQ(txt::normalized_matches(composed, string("e")).count(), 0u);

    // Every match the range hands out is a slice with something in it,
    // which is what the rule is for
    string many = "straße, STRASSE, Straße";
    size_t n = 0;
    for (auto m : txt::fold_matches(many, string("strasse"))) {
        EXPECT_NE(m.size(), 0u);
        ++n;
    }
    EXPECT_EQ(n, 3u);
}

// The easiest mistake here: after turning a match down the scan has to
// go on from the next position, not stop. Every one of these has a
// refused match standing in front of a good one.
TEST(Search_Tests, ARefusedMatchDoesNotEndTheSearch) {
    // the "s" inside the ß comes first and is refused; the real one is
    // behind it
    EXPECT_EQ(txt::find_fold(string("ß s"), string("s"))->pos, 3u);
    EXPECT_EQ(txt::find_fold(string("ßßßs"), string("s"))->pos, 6u);
    EXPECT_EQ(txt::find_fold(string("aßsb"), string("s"))->pos, 3u);

    // and two refusals in a row
    EXPECT_EQ(txt::find_fold(string("ßxßx s"), string("s"))->pos, 7u);

    // the same where the accent is refused before a real one
    EXPECT_EQ(txt::find_normalized(string("ée"), string("e"))->pos, 2u);
    EXPECT_EQ(txt::find_normalized(string("éée"), string("e"))->pos, 4u);

    // a refusal in the middle of a run does not stop the count either
    EXPECT_EQ(txt::fold_matches(string("sßsßs"), string("s")).count(), 3u);
    EXPECT_EQ(txt::normalized_matches(string("eée"), string("e")).count(), 2u);

    // and the loop over find, which is what the range is measured
    // against, walks the same ones
    string text = "sßsßs";
    std::vector<size_t> at;
    for (size_t i = pos_of(txt::find_fold(text, string("s"))); i != npos;
         i = pos_of(txt::find_fold(text, string("s"), i + 1))) {
        at.push_back(i);
    }
    EXPECT_EQ(at, (std::vector<size_t>{0, 3, 6}));
}

// An empty pattern is found where it is looked for, and nowhere past the
// end of the text — which is what std::string::find and the searcher of
// this header answer, and what find_fold answered wrongly by giving back
// a position that was never in the text
TEST(Search_Tests, AnEmptyPatternPastTheEndOfTheText) {
    string text = "abc";
    for (size_t from = 0; from <= 3; ++from) {
        EXPECT_EQ(pos_of(txt::find_fold(text, string(), from)), from) << from;
        EXPECT_EQ(pos_of(txt::find_normalized(text, string(), from)), from) << from;
        EXPECT_EQ(txt::searcher(string()).find(text, from), from) << from;
    }
    for (size_t from : {size_t(4), size_t(100), npos - 1}) {
        EXPECT_FALSE(txt::find_fold(text, string(), from)) << from;
        EXPECT_FALSE(txt::find_normalized(text, string(), from)) << from;
        EXPECT_EQ(txt::searcher(string()).find(text, from), npos) << from;
    }
    // and over an empty text, where only position zero is in it
    EXPECT_EQ(txt::find_fold(string(), string())->pos, 0u);
    EXPECT_FALSE(txt::find_fold(string(), string(), 1));
    EXPECT_FALSE(txt::find_normalized(string(), string(), 1));

    // A folded text is longer or shorter than the text it came from, and
    // the bound is the text's
    string sharp = "ß";                 // two bytes, two folded points
    EXPECT_EQ(txt::find_fold(sharp, string(), 2)->pos, 2u);
    EXPECT_FALSE(txt::find_fold(sharp, string(), 3));
}
// The one rule both searches keep: a match may not end in front of a mark
// that belongs to the letter it ends on. It is normalize.h's
// opens_sequence, asked here of the code points and in collate.h of the
// elements, and what it is worth is that the answer stops depending on how
// the text happens to be written. "cafe" was found inside a "café" spelt
// with a combining acute and not inside one spelt with a single code
// point, which is the same text.
TEST(Search_Tests, TwoSpellingsOfOneTextAnswerTheSame) {
    struct { const char* composed; const char* apart; } letters[] = {
        {"café",       "café"},        // e acute
        {"côte",       "côte"},        // o circumflex
        {"ạ",          "ạ"},           // a with a dot below
        {"Ą",          "Ą"},           // A with an ogonek
    };
    const char* patterns[] = {"cafe", "cote", "a", "A", "caf", "e", "o"};
    for (auto& l : letters) {
        string composed(l.composed), apart(l.apart);
        ASSERT_NE(composed, apart) << l.composed;
        for (auto* p : patterns) {
            string pattern(p);
            EXPECT_EQ(!txt::find_normalized(composed, pattern),
                      !txt::find_normalized(apart, pattern))
                << l.composed << " against " << l.apart << ", pattern " << p;
            EXPECT_EQ(!txt::find_fold(composed, pattern),
                      !txt::find_fold(apart, pattern))
                << l.composed << " against " << l.apart << ", pattern " << p;
        }
    }

    // and the same down the prepared roads and the range, which have
    // their own way to the rule
    string apart("café cafe");
    txt::normalized_searcher pattern(string("cafe"));
    txt::normalized_text weighed(apart);
    // A refused match moves the scan on and does not end it: the first
    // "cafe" is refused for the acute behind it and the second is found.
    // This is the mistake the header names and it is the one worth a test.
    EXPECT_EQ(weighed.find(pattern), 7u);
    EXPECT_EQ(weighed.count(pattern), 1u);
    EXPECT_EQ(txt::find_normalized(apart, string("cafe"))->pos, 7u);
    EXPECT_EQ(txt::normalized_matches(apart, pattern).count(), 1u);
    size_t at = npos;
    for (auto m : txt::normalized_matches(apart, pattern)) {
        at = size_t(m.data() - apart.view().data());
    }
    EXPECT_EQ(at, 7u);

    // three refusals in a row, so that the scan has to survive more than
    // one: only the last of the four is a match
    string many("cafécafécafécafe");
    EXPECT_EQ(txt::find_normalized(many, string("cafe"))->pos, 18u);
    EXPECT_EQ(txt::normalized_text(many).count(pattern), 1u);
}
// Where the canonical ordering moves a mark, the bisection that finds where
// a search starts has been a fault twice. First it was given up altogether:
// the positions moved with the marks and did not ascend, every find began at
// the front of the text, and one mark pair written the wrong way round
// anywhere in a text made a loop over the occurrences quadratic. Then it was
// done over a running maximum of those positions, a lower bound that one
// shape of text could push too high. The positions stay where they are
// written now and the marks move without them, so they ascend in every text
// and the bisection is over them. What this asks is that no answer depends
// on it: from every byte of a text, a bisected find answers what a walk from
// the front answers.
TEST(Search_Tests, TheBisectionOverMarksPutInOrder) {
    // The narrow case that told the earlier bisections apart: a text that
    // begins with marks, which the canonical order then permutes among
    // themselves. The first code point of a text opens a combining
    // sequence whatever it is, so a match may begin there, and after the
    // reordering the point at index 0 is the mark written second, at byte
    // 2. The match is both marks, so it begins at byte 0, and asked for
    // from any byte after that it is behind the caller.
    {
        string text("̖̀ḃ");
        string pattern("̖̀");
        EXPECT_EQ(txt::find_normalized(text, pattern)->pos, 0u);
        EXPECT_FALSE(txt::find_normalized(text, pattern, 1));
        EXPECT_FALSE(txt::find_normalized(text, pattern, 2));
        EXPECT_FALSE(txt::find_normalized(text, pattern, 3));
        txt::normalized_text weighed(text);
        txt::normalized_searcher searcher(pattern);
        EXPECT_EQ(weighed.find(searcher, 0), 0u);
        EXPECT_EQ(weighed.find(searcher, 1), npos);
        EXPECT_EQ(weighed.find(searcher, 2), npos);
        // the whole text as its own pattern, which begins at the same
        // place and runs to the end
        EXPECT_EQ(txt::find_normalized(text, text)->pos, 0u);
        EXPECT_FALSE(txt::find_normalized(text, text, 1));
        // and the letter behind the marks, from every byte up to it
        for (size_t from = 0; from <= 4; ++from) {
            EXPECT_EQ(weighed.find(txt::normalized_searcher(string("ḃ")), from), 4u) << from;
        }
    }

    // a text with marks out of canonical order in several places, a few
    // letters that are found, and the same text with the marks written
    // the other way round
    string out_of_order = "ẋ̣aỵ̇aḍ̇aẓ̇a";
    string in_order     = "ẋ̣aỵ̇aḍ̇aẓ̇a";
    const char* patterns[] = {"a", "ay", "az", "aa", "x", "q"};

    for (auto* p : patterns) {
        for (auto& t : {out_of_order, in_order}) {
            string text(t), pattern(p);
            txt::normalized_searcher searcher(pattern);
            txt::normalized_text weighed(text);

            // what a bisected find answers, from every byte of the text,
            // is what a walk from the front answers
            for (size_t from = 0; from <= text.size() + 1; ++from) {
                size_t want = npos;
                for (size_t at = weighed.find(searcher, 0); at != npos;) {
                    if (at >= from) {
                        want = at;
                        break;
                    }
                    size_t next = weighed.find(searcher, at + 1);
                    if (next == at) {
                        break;
                    }
                    at = next;
                }
                EXPECT_EQ(weighed.find(searcher, from), want)
                    << "pattern " << p << " from " << from;
                EXPECT_EQ(pos_of(txt::find_normalized(text, pattern, from)), want)
                    << "pattern " << p << " from " << from << ", one shot";
            }
        }
    }

    // the two spellings hold the same number of occurrences, which is what
    // says the reordering is not changing the answers, only the positions
    for (auto* p : patterns) {
        string pattern(p);
        txt::normalized_searcher searcher(pattern);
        EXPECT_EQ(txt::normalized_text(out_of_order).count(searcher),
                  txt::normalized_text(in_order).count(searcher)) << p;
        // and a range, which walks by index and asks no bisection at all,
        // counts what the prepared text counts
        EXPECT_EQ(txt::normalized_matches(out_of_order, searcher).count(),
                  txt::normalized_text(out_of_order).count(searcher)) << p;
    }

    // a loop over every occurrence of a text whose marks are out of order
    // terminates and finds each of them once, which is the loop that was
    // quadratic
    string many;
    for (int i = 0; i < 200; ++i) {
        many = many + string("ẋ̣kota ");
    }
    txt::normalized_searcher kota(string("kota"));
    txt::normalized_text weighed(many);
    size_t found = 0;
    for (size_t at = weighed.find(kota, 0), guard = 0; at != npos && guard < 1000; ++guard) {
        ++found;
        at = weighed.find(kota, at + 1);
    }
    EXPECT_EQ(found, 200u);
    EXPECT_EQ(weighed.count(kota), 200u);
}
// The same for the ranges here: the text a range holds is a string and
// not a slice, so no raw pointer from one managed object into another
// lives inside the tracked state.
TEST(Search_Tests, ARangeHoldsItsTextAsAString) {
    string text("Ala ma kota, ALA ma kota");
    std::vector<size_t> at;
    std::vector<std::string> found;
    for (auto it = txt::fold_matches(text, string("ala")).begin(),
              e = txt::fold_matches(text, string("ala")).end(); it != e; ++it) {
        at.push_back(it.pos());
        found.push_back(std::string((*it).data(), (*it).size()));
    }
    EXPECT_EQ(at, (std::vector<size_t>{0, 13}));
    EXPECT_EQ(found[0], "Ala");
    EXPECT_EQ(found[1], "ALA");
    EXPECT_EQ(txt::fold_matches(text, string("ala")).text().size(), text.size());

    // a range over a piece of a text keeps the piece
    size_t n = 0;
    std::string piece;
    off_frame([&] {
        auto range = txt::fold_matches(string("xx ALA yy").as_slice(3, 3),
                                       txt::fold_searcher(string("ala")));
        for (auto m : range) {
            ++n;
            piece.assign(m.data(), m.size());
        }
        EXPECT_EQ(range.text().size(), 3u);
        EXPECT_EQ(range.begin().pos(), 0u);
    });
    EXPECT_EQ(n, 1u);
    EXPECT_EQ(piece, "ALA");

    // and the iterator outlives the range
    auto it = txt::normalized_matches(text, string("kota")).begin();
    collector::force_collect(true);
    EXPECT_EQ(it.pos(), 7u);
    EXPECT_EQ(std::string((*it).data(), (*it).size()), "kota");
}
// The case the module page carried as the one place the rule was looser
// than it reads: a character that decomposes to a base and two marks which
// the canonical ordering then pulls apart. "d with a dot above" and then a
// dot below it maps to d, dot-below, dot-above — the dot-above belongs to
// the first character of the text and no longer stands next to it — so the
// point past a match of the base alone is the first output of its own
// character, and the rule about cutting a character in two has nothing to
// say about it. What refuses the match is the other rule: that point is a
// mark, and a match may not end inside a combining sequence.
//
// The two rules together are a rule about the set the match covers and not
// about its neighbours, which is what this asks: the base alone is refused
// however the text spells it, and the whole character is still found.
TEST(Search_Tests, ADecompositionTheOrderingPullsApart) {
    struct { const char* text; const char* base; const char* whole; } cases[] = {
        // d with a dot above, then a dot below: the page's own example
        {"ḍ̇", "d", "ḍ̇"},
        // the same character spelt three other ways
        {"ḍ̇", "d", "ḍ̇"},
        {"ḍ̇", "d", "ḍ̇"},
        {"ḍ̇", "d", "ḍ̇"},
        // a cedilla under an acute: classes 202 and 230
        {"ḉ", "c", "ḉ"},
        {"ḉ", "c", "ḉ"},
        // a macron over a dot below, which chains through two tables
        {"ṝ", "r", "ṝ"},
        {"ṝ", "r", "ṝ"},
        // three marks of three classes
        {"ạ̧́", "a", "ạ̧́"},
    };
    for (auto& c : cases) {
        string text(c.text), base(c.base), whole(c.whole);
        // the base alone is not an occurrence of the character
        EXPECT_FALSE(txt::find_normalized(text, base)) << c.text;
        EXPECT_FALSE(txt::find_fold(text, base)) << c.text;
        EXPECT_FALSE(txt::contains_normalized(text, base)) << c.text;
        EXPECT_FALSE(txt::contains_fold(text, base)) << c.text;
        // down every road, not only the one-shot one
        EXPECT_EQ(txt::normalized_text(text).find(txt::normalized_searcher(base)), npos) << c.text;
        EXPECT_EQ(txt::normalized_text(text).count(txt::normalized_searcher(base)), 0u) << c.text;
        EXPECT_EQ(txt::folded_text(text).count(txt::fold_searcher(base)), 0u) << c.text;
        EXPECT_EQ(txt::normalized_matches(text, txt::normalized_searcher(base)).count(), 0u) << c.text;
        EXPECT_EQ(txt::fold_matches(text, txt::fold_searcher(base)).count(), 0u) << c.text;
        // and the whole character is found, which is what the rule is for
        EXPECT_EQ(txt::find_normalized(text, whole)->pos, 0u) << c.text;
        EXPECT_EQ(txt::find_normalized(text, text)->pos, 0u) << c.text;
        EXPECT_EQ(txt::normalized_matches(text, txt::normalized_searcher(whole)).count(), 1u) << c.text;
        // the slice a match hands back is the whole character and nothing
        // less: it normalizes to what the pattern normalizes to
        for (auto m : txt::normalized_matches(text, txt::normalized_searcher(whole))) {
            string piece(m.data(), m.size());
            EXPECT_EQ(txt::normalize(piece, txt::nfd), txt::normalize(whole, txt::nfd)) << c.text;
            EXPECT_EQ(m.size(), text.size()) << c.text;
        }
    }

    // a base that keeps its own marks is still found in front of another
    // letter, so the rule refuses what it should and nothing else
    string run("ḍ̇x ḍ̇");
    EXPECT_EQ(txt::find_normalized(run, string("x"))->pos, 5u);
    EXPECT_EQ(txt::normalized_matches(run, txt::normalized_searcher(string("ḍ̇"))).count(), 2u);
    EXPECT_FALSE(txt::find_normalized(run, string("d")));
}

// What the same character asks beyond the base alone. A match of the base
// and one of its two marks leaves the other outside it, wherever the
// ordering put that one, and is no more an occurrence of the character
// than the base is. And a refusal moves the scan on rather than ending it:
// a bare letter after a marked one is found, down every road, which is the
// mistake that is easiest to make and was made once in this module.
TEST(Search_Tests, PartOfADecompositionTheOrderingPullsApart) {
    // d with a dot above and a dot below, written the four ways there are
    const char* spellings[] = {"ḍ̇", "ḍ̇", "ḍ̇", "ḍ̇"};
    // the base with one of the two marks, each written both ways
    const char* parts[] = {"ḍ", "ḍ", "ḋ", "ḋ"};
    string whole("ḍ̇");
    for (auto* s : spellings) {
        string letter(s);
        for (auto* p : parts) {
            string part(p);
            EXPECT_FALSE(txt::find_normalized(letter, part)) << s << " " << p;
            EXPECT_EQ(txt::normalized_text(letter).count(txt::normalized_searcher(part)), 0u) << s << " " << p;
            EXPECT_EQ(txt::normalized_matches(letter, txt::normalized_searcher(part)).count(), 0u) << s << " " << p;
        }

        // inside a word the letters round it change nothing: the base is
        // refused, the whole is found where it stands, and the slice a
        // range hands back runs from the letter before it to the letter
        // after it
        string word = string("xa") + letter + string("b");
        EXPECT_FALSE(txt::find_normalized(word, string("ad"))) << s;
        EXPECT_FALSE(txt::find_normalized(word, string("adb"))) << s;
        EXPECT_EQ(txt::find_normalized(word, string("a") + whole)->pos, 1u) << s;
        EXPECT_EQ(txt::find_normalized(word, whole + string("b"))->pos, 2u) << s;
        size_t seen = 0;
        for (auto m : txt::normalized_matches(word, txt::normalized_searcher(string("a") + whole + string("b")))) {
            EXPECT_EQ(std::string(m.data(), m.size()), std::string(word.view().substr(1))) << s;
            ++seen;
        }
        EXPECT_EQ(seen, 1u) << s;

        // marked, bare, marked, bare: two occurrences of "d", each of them
        // behind a refusal
        string run = letter + string("d") + letter + string("d");
        size_t one = letter.size(), two = 2 * letter.size() + 1;
        txt::normalized_searcher d(string("d"));
        EXPECT_EQ(pos_of(txt::find_normalized(run, string("d"))), one) << s;
        EXPECT_EQ(pos_of(txt::find_normalized(run, string("d"), one + 1)), two) << s;
        EXPECT_EQ(txt::normalized_text(run).find(d), one) << s;
        EXPECT_EQ(txt::normalized_text(run).find(d, one + 1), two) << s;
        EXPECT_EQ(txt::normalized_text(run).count(d), 2u) << s;
        txt::normalized_matches every(run, d);
        std::vector<size_t> at;
        for (auto it = every.begin(); it != every.end(); ++it) {
            at.push_back(it.pos());
            EXPECT_EQ(it.size(), 1u) << s;
        }
        EXPECT_EQ(at, (std::vector<size_t>{one, two})) << s;
        // and blind to case, where nothing is decomposed and the marks are
        // refused as the code points they were written as
        EXPECT_EQ(pos_of(txt::find_fold(run, string("D"))), one) << s;
        EXPECT_EQ(pos_of(txt::find_fold(run, string("D"), one + 1)), two) << s;
        EXPECT_EQ(txt::folded_text(run).count(txt::fold_searcher(string("D"))), 2u) << s;
        EXPECT_EQ(txt::fold_matches(run, string("D")).count(), 2u) << s;
    }
}

// Where a match begins and how far it runs is the characters it covers,
// and not the positions of the points at its two ends. The two are the
// same everywhere a match begins on a letter, since nothing the ordering
// moves ever crosses one. They differ at the front of a text that begins
// with marks: the first point of a text opens a sequence whatever it is,
// so a match may begin there, and after the ordering the point at index 0
// is the mark written second. This search used to answer with that
// mark's position, two bytes into a match that covers both marks, and a
// range handed out a slice of one mark, which is half of what matched.
// The collated search answers with the sequence, as it always has, and
// the two may not say different things about one text.
TEST(Search_Tests, AMatchIsTheCharactersItCovers) {
    // a grave (230) written before a grave below (220): the ordering
    // swaps them, so the point at index 0 comes from byte 2
    string text("̖̀ḃ");
    for (auto p : {string("̖̀"), string("̖̀")}) {
        txt::normalized_searcher searcher(p);
        EXPECT_EQ(txt::find_normalized(text, p)->pos, 0u);
        EXPECT_FALSE(txt::find_normalized(text, p, 1));   // it begins at 0, not at 2
        EXPECT_EQ(txt::normalized_text(text).find(searcher), 0u);
        EXPECT_EQ(txt::normalized_text(text).find(searcher, 1), npos);
        txt::normalized_matches every(text, searcher);
        size_t seen = 0;
        for (auto it = every.begin(); it != every.end(); ++it) {
            EXPECT_EQ(it.pos(), 0u);
            EXPECT_EQ(it.size(), 4u);                        // both marks, not the second alone
            EXPECT_EQ(std::string((*it).data(), (*it).size()), std::string(text.view().substr(0, 4)));
            ++seen;
        }
        EXPECT_EQ(seen, 1u);

        // the collated search, asked the same, answers the same
        txt::collator exact;
        auto hit = exact.find(text, p);
        ASSERT_TRUE(hit.has_value());
        EXPECT_EQ(hit->at, 0u);
        EXPECT_EQ(hit->size, 4u);
    }
    // the whole text, from the front and from inside it
    EXPECT_EQ(txt::find_normalized(text, text)->pos, 0u);
    EXPECT_FALSE(txt::find_normalized(text, text, 1));
    for (auto m : txt::normalized_matches(text, txt::normalized_searcher(text))) {
        EXPECT_EQ(m.size(), text.size());
    }
    // and the letter behind the marks is where it always was
    EXPECT_EQ(txt::find_normalized(text, string("ḃ"))->pos, 4u);
    EXPECT_EQ(txt::find_normalized(text, string("ḃ"), 1)->pos, 4u);
}





// The canonical ordering moves a mark's position along with the mark, so
// the positions of a decomposed text do not ascend, and the bisection
// that finds where a search starts cannot be used there as it stands. It
// was, and a search from inside a reordered sequence then answered npos
// where the match was in front of it.
TEST(Search_Tests, ASearchFromInsideMarksThatWerePutInOrder) {
    // x, then a dot above (ccc 230) and a dot below (ccc 220), which the
    // canonical order swaps: the dot below is written second and comes
    // first in the decomposition.
    string text = "ẋ̣y";
    string letter = text.substr(0, 5);           // the x and its two marks
    string y = text.substr(5);

    // The letter stands twice, so that a search from inside the first has
    // a second to find. A lone mark is no longer a pattern that can be
    // found — it is inside a combining sequence and a match may not begin
    // there — so the question the bisection gets wrong is asked with a
    // pattern that can be, which is a stronger instrument anyway: the
    // answer is a position and not merely npos or not npos.
    string twice = text + letter;                // letter, y, letter
    EXPECT_EQ(txt::find_normalized(twice, letter)->pos, 0u);
    EXPECT_EQ(txt::find_normalized(twice, letter, 1)->pos, 6u);
    EXPECT_EQ(txt::find_normalized(twice, letter, 3)->pos, 6u);
    EXPECT_EQ(txt::find_normalized(twice, letter, 6)->pos, 6u);
    EXPECT_FALSE(txt::find_normalized(twice, letter, 7));
    EXPECT_EQ(txt::find_normalized(twice, y)->pos, 5u);
    EXPECT_EQ(txt::find_normalized(twice, y, 1)->pos, 5u);
    EXPECT_EQ(txt::find_normalized(twice, y, 5)->pos, 5u);
    EXPECT_FALSE(txt::find_normalized(twice, y, 6));

    // the whole sequence, in either order of the two marks, is found at
    // the base it belongs to
    EXPECT_EQ(txt::find_normalized(text, letter)->pos, 0u);

    // and a lone mark is not a thing to be found, here as in the collated
    // search: it stands inside a combining sequence
    EXPECT_FALSE(txt::find_normalized(text, text.substr(1, 2)));
    EXPECT_FALSE(txt::find_normalized(text, text.substr(3, 2)));

    // and a text whose marks are already in order keeps the bisection,
    // which the answers above must not depend on
    string ordered = "ẋ̣y";
    string same = ordered.substr(0, 5);
    EXPECT_EQ(txt::find_normalized(ordered, same)->pos, 0u);
    EXPECT_EQ(txt::find_normalized(ordered + same, same, 1)->pos, 6u);
    EXPECT_EQ(txt::find_normalized(ordered, ordered.substr(5), 1)->pos, 5u);
}
