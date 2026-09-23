//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::segment: the boundaries of UAX #29. The oracle is the UCD's own
// test file, every case of it (break_tests.h, generated with the tables);
// the rest checks the interface over the algorithm — the range, the cursor
// and the text held by a slice.
#include "tests/types.h"
#include "tests/txt/break_tests.h"

#include <string>

namespace {
    // A case of the UCD file as UTF-8, and the byte position of every
    // boundary it says there is
    std::pair<std::string, std::vector<size_t>> encoded(const ucd::BreakCase& c) {
        std::string text;
        std::vector<size_t> boundaries;
        for (size_t i = 0; c.breaks[i]; ++i) {
            if (c.breaks[i] == '1') {
                boundaries.push_back(text.size());
            }
            if (c.text[i]) {
                char buf[utf8::max_width];
                text.append(buf, utf8::encode(c.text[i], buf));
            }
        }
        return {text, boundaries};
    }

    std::string describe(const ucd::BreakCase& c) {
        std::string out;
        char buf[16];
        for (size_t i = 0; c.text[i]; ++i) {
            snprintf(buf, sizeof(buf), "%04X ", uint32_t(c.text[i]));
            out += buf;
        }
        return out + "(" + c.breaks + ")";
    }
}

TEST(Segment_Tests, EveryCaseOfTheUcdGraphemeTest) {
    size_t cases = 0;
    for (const auto& c : ucd::GraphemeBreakTests) {
        auto [text, boundaries] = encoded(c);
        string s(text.data(), text.size());

        // The boundaries the range walks are the ones the file names; the
        // file's last boundary is the end of the text, which a range of
        // clusters does not hand out as a cluster of its own
        std::vector<size_t> found;
        for (auto it = txt::graphemes(s).begin(); it != txt::graphemes(s).end(); ++it) {
            found.push_back(it.pos());
        }
        boundaries.pop_back();
        ASSERT_EQ(found, boundaries) << describe(c);
        ASSERT_EQ(txt::grapheme_count(s), boundaries.size()) << describe(c);

        // The clusters put back together are the text
        std::string joined;
        for (auto g : txt::graphemes(s)) {
            joined.append(g.data(), g.size());
        }
        ASSERT_EQ(joined, text) << describe(c);
        ++cases;
    }
    EXPECT_EQ(cases, std::size(ucd::GraphemeBreakTests));
    EXPECT_GT(cases, 1000u);
}

TEST(Segment_Tests, EveryCaseOfTheUcdWordTest) {
    for (const auto& c : ucd::WordBreakTests) {
        auto [text, boundaries] = encoded(c);
        string s(text.data(), text.size());
        std::vector<size_t> found;
        for (auto it = txt::words(s).begin(); it != txt::words(s).end(); ++it) {
            found.push_back(it.pos());
        }
        boundaries.pop_back();
        ASSERT_EQ(found, boundaries) << describe(c);
    }
    EXPECT_EQ(std::size(ucd::WordBreakTests), 1826u);
}

TEST(Segment_Tests, EveryCaseOfTheUcdSentenceTest) {
    for (const auto& c : ucd::SentenceBreakTests) {
        auto [text, boundaries] = encoded(c);
        string s(text.data(), text.size());
        std::vector<size_t> found;
        for (auto it = txt::sentences(s).begin(); it != txt::sentences(s).end(); ++it) {
            found.push_back(it.pos());
        }
        boundaries.pop_back();
        ASSERT_EQ(found, boundaries) << describe(c);
    }
    EXPECT_EQ(std::size(ucd::SentenceBreakTests), 512u);
}

TEST(Segment_Tests, EveryCaseOfTheUcdLineTest) {
    size_t failed = 0;
    for (const auto& c : ucd::LineBreakTests) {
        auto [text, boundaries] = encoded(c);
        string s(text.data(), text.size());
        std::vector<size_t> found;
        for (auto it = txt::line_breaks(s).begin(); it != txt::line_breaks(s).end(); ++it) {
            found.push_back(it.pos());
        }
        // LineBreakTest.txt opens with a prohibited break (LB2: never at
        // the start of the text), where the other files open with one, so
        // the range's first segment is not among the file's boundaries
        boundaries.pop_back();
        found.erase(found.begin());
        if (found != boundaries) {
            if (++failed <= 10) {
                ADD_FAILURE() << describe(c);
            }
        }
    }
    EXPECT_EQ(failed, 0u) << failed << " of " << std::size(ucd::LineBreakTests) << " cases";
}

TEST(Segment_Tests, TheSentencesOfAText) {
    string s = "Ala ma kota. Kot ma Al\u0119! Czy na pewno? Tak.";
    std::vector<std::string> out;
    for (auto x : txt::sentences(s)) {
        out.emplace_back(x.data(), x.size());
    }
    EXPECT_EQ(out.size(), 4u);
    EXPECT_EQ(out[0], "Ala ma kota. ");                  // the space belongs to the sentence it closes
    EXPECT_EQ(out[3], "Tak.");

    // A stop is not enough: an abbreviation and a decimal stay inside
    EXPECT_EQ(txt::sentences(string("U.S.A. to kraj.")).count(), 1u);     // SB7: a stop between two capitals
    EXPECT_EQ(txt::sentences(string("Pan J. Kowalski przyszed\u0142.")).count(), 2u);   // and where the default gives up
    EXPECT_EQ(txt::sentences(string("It costs 3.14 euro. Really.")).count(), 2u);
    EXPECT_EQ(txt::sentences(string("Zobacz np. tutaj.")).count(), 1u);   // a stop and a lower case letter

    // And a stop is not always needed: a paragraph ends a sentence
    EXPECT_EQ(txt::sentences(string("bez kropki\nnast\u0119pna")).count(), 2u);
    EXPECT_EQ(txt::sentences(string("a\u2029b")).count(), 2u);           // a paragraph separator

    // Every byte belongs to exactly one sentence
    std::string joined;
    for (auto x : txt::sentences(s)) {
        joined.append(x.data(), x.size());
    }
    EXPECT_EQ(joined, std::string(s.data(), s.size()));
    EXPECT_TRUE(txt::sentences(string()).empty());
    static_assert(req::enumerable<txt::sentences>);
}

TEST(Segment_Tests, TheWordsOfATextAndWhatIsBetweenThem) {
    // Every segment, the runs between the words included: that is what
    // UAX #29 defines, and what a double click selects
    string s = "Ala ma kota.";
    std::vector<std::string> segments;
    for (auto w : txt::words(s)) {
        segments.emplace_back(w.data(), w.size());
    }
    EXPECT_EQ(segments, (std::vector<std::string>{"Ala", " ", "ma", " ", "kota", "."}));

    // The words themselves: the segments with something alphanumeric in them
    auto is_word = [](slice<const char> w) { return w.runes().exists(txt::is_alnum); };
    EXPECT_EQ(txt::words(s).count_of(is_word), 3u);

    // What the rules keep inside a word
    EXPECT_EQ(txt::words(string("don't")).count_of(is_word), 1u);        // the apostrophe
    EXPECT_EQ(txt::words(string("3.14")).count_of(is_word), 1u);         // the decimal point
    EXPECT_EQ(txt::words(string("192.168.0.1")).count_of(is_word), 1u);
    EXPECT_EQ(txt::words(string("e-mail")).count_of(is_word), 2u);       // and what it does not
    EXPECT_EQ(txt::words(string("\u0141\u00F3d\u017A nad Wis\u0142\u0105")).count_of(is_word), 3u);
    EXPECT_EQ(txt::words(string("can't stop, won't stop")).count_of(is_word), 4u);

    // A text with no letters at all, and no text
    EXPECT_EQ(txt::words(string("... ")).count_of(is_word), 0u);
    EXPECT_EQ(txt::words(string("... ")).count(), 4u);                 // each stop its own, the spaces one (WB3d)
    EXPECT_TRUE(txt::words(string()).empty());

    // An accent does not cut a word in two (rule WB4), and a flag is one
    EXPECT_EQ(txt::words(string("e\u0301gal")).count(), 1u);
    EXPECT_EQ(txt::words(string("\U0001F1F5\U0001F1F1")).count(), 1u);

    // The same range machinery as the graphemes
    static_assert(req::enumerable<txt::words>);
    EXPECT_EQ((*txt::words(s).begin()).data(), s.data());
}

TEST(Segment_Tests, WhatAHumanCountsAsACharacter) {
    // A code point is not a character: the same text in two forms, one
    // grapheme either way
    EXPECT_EQ(txt::grapheme_count(string("é")), 1u);                    // U+00E9
    EXPECT_EQ(txt::grapheme_count(string("e\u0301")), 1u);              // e + combining acute
    EXPECT_EQ(string("e\u0301").rune_count(), 2u);
    EXPECT_EQ(txt::grapheme_count(string("\U0001F1F5\U0001F1F1")), 1u); // a flag: two regional indicators
    EXPECT_EQ(txt::grapheme_count(string("\U0001F1F5\U0001F1F1\U0001F1E9\U0001F1EA")), 2u);   // two flags
    EXPECT_EQ(txt::grapheme_count(string("\U0001F44D\U0001F3FD")), 1u); // a thumb with a skin tone
    EXPECT_EQ(txt::grapheme_count(string("\U0001F468\u200D\U0001F469\u200D\U0001F467")), 1u); // a family, joined
    EXPECT_EQ(txt::grapheme_count(string("\u0915\u093F")), 1u);         // \u0915\u093F: a consonant and its vowel sign
    EXPECT_EQ(txt::grapheme_count(string("\u0915\u094D\u0937")), 1u);   // \u0915\u094D\u0937: a conjunct, rule GB9c
    EXPECT_EQ(txt::grapheme_count(string("\r\n")), 1u);                 // one boundary, not two
    EXPECT_EQ(txt::grapheme_count(string("a\r\nb")), 3u);
    EXPECT_EQ(txt::grapheme_count(string("\uAC01")), 1u);               // a precomposed Hangul syllable
    EXPECT_EQ(txt::grapheme_count(string("\u1100\u1161\u11A8")), 1u);   // the same, as jamo
    EXPECT_EQ(txt::grapheme_count(string()), 0u);

    // An invalid byte is one replacement code point and one grapheme
    EXPECT_EQ(txt::grapheme_count(string("a\xFF" "b")), 3u);

    // The three counts of a text, each answering a different question
    string s = "e\u0301\U0001F1F5\U0001F1F1";
    EXPECT_EQ(s.size(), 11u);
    EXPECT_EQ(s.rune_count(), 4u);
    EXPECT_EQ(txt::grapheme_count(s), 2u);
}

TEST(Segment_Tests, TheRangeIsARangeOfTheLibrary) {
    string s = "z\u0301a\u0301b";
    auto g = txt::graphemes(s);
    EXPECT_EQ(g.count(), 3u);
    EXPECT_FALSE(g.empty());
    EXPECT_TRUE(txt::graphemes(string()).empty());
    static_assert(req::enumerable<txt::graphemes>);

    // Every element is a slice of the text, with its position and size
    std::vector<std::pair<size_t, size_t>> at;
    for (auto it = g.begin(); it != g.end(); ++it) {
        at.emplace_back(it.pos(), it.size());
    }
    EXPECT_EQ(at, (std::vector<std::pair<size_t, size_t>>{{0, 3}, {3, 3}, {6, 1}}));

    // The mixin's operations, on slices
    EXPECT_EQ(g.count_of([](slice<const char> c) { return c.size() > 1; }), 2u);
    EXPECT_TRUE(g.exists([](slice<const char> c) { return c == "b"; }));
    EXPECT_EQ(g.find_index([](slice<const char> c) { return c == "b"; }), 2u);

    // A range over a temporary holds the text: the slices stay good
    size_t n = 0;
    for (auto c : txt::graphemes(string("ą\u0301ę"))) {
        n += c.size();
    }
    EXPECT_EQ(n, 6u);

    // The text a slice points into is the string's, not a copy
    EXPECT_EQ((*g.begin()).data(), s.data());
}

TEST(Segment_Tests, MovingACursorByCharacters) {
    // "e" + combining acute, a flag, "b": 3, 8, 1 bytes
    string s = "e\u0301\U0001F1F5\U0001F1F1b";
    EXPECT_EQ(s.size(), 12u);

    EXPECT_EQ(txt::grapheme_next(s, 0), 3u);
    EXPECT_EQ(txt::grapheme_next(s, 3), 11u);
    EXPECT_EQ(txt::grapheme_next(s, 11), 12u);
    EXPECT_EQ(txt::grapheme_next(s, 12), 12u);                   // the end stays the end

    EXPECT_EQ(txt::grapheme_prev(s, 12), 11u);
    EXPECT_EQ(txt::grapheme_prev(s, 11), 3u);
    EXPECT_EQ(txt::grapheme_prev(s, 3), 0u);
    EXPECT_EQ(txt::grapheme_prev(s, 0), 0u);                     // the start stays the start

    // A position inside a grapheme belongs to the grapheme that holds it
    EXPECT_EQ(txt::grapheme_start(s, 1), 0u);                    // inside the combining acute
    EXPECT_EQ(txt::grapheme_start(s, 7), 3u);                    // between the two regional indicators
    EXPECT_EQ(txt::grapheme_start(s, 3), 3u);                    // a boundary is its own start
    EXPECT_EQ(txt::grapheme_next(s, 7), 11u);                    // a caret never lands inside one
    EXPECT_EQ(txt::grapheme_prev(s, 7), 3u);                    // the last boundary before it, which is its own start

    // Backspace over the whole text lands on every boundary and nowhere else
    std::vector<size_t> stops;
    for (size_t pos = s.size(); pos > 0; pos = txt::grapheme_prev(s, pos)) {
        stops.push_back(pos);
    }
    EXPECT_EQ(stops, (std::vector<size_t>{12, 11, 3}));
}

TEST(Segment_Tests, WhereALineMayBeBroken) {
    // The pieces that must stay together, their trailing spaces included
    string s = "Ala ma kota.";
    std::vector<std::string> pieces;
    for (auto piece : txt::line_breaks(s)) {
        pieces.emplace_back(piece.data(), piece.size());
    }
    EXPECT_EQ(pieces, (std::vector<std::string>{"Ala ", "ma ", "kota."}));

    // What may not be cut: a number and its decimal mark, a bracket and
    // what is in it, a non-breaking space
    EXPECT_EQ(txt::line_breaks(string("3.14 euro")).count(), 2u);
    EXPECT_EQ(txt::line_breaks(string("(a) b")).count(), 2u);
    EXPECT_EQ(txt::line_breaks(string("10 km")).count(), 1u);      // a no-break space
    EXPECT_EQ(txt::line_breaks(string("e-mail me")).count(), 3u);       // after the hyphen a line may break
    EXPECT_EQ(txt::line_breaks(string("漢字")).count(), 2u);    // between two ideographs it may
    EXPECT_TRUE(txt::line_breaks(string()).empty());
    static_assert(req::enumerable<txt::line_breaks>);
}

TEST(Segment_Tests, WrappingToAWidth) {
    string s = "Ala ma kota a kot ma Ale";
    auto lines = txt::wrap(s, 10);
    std::vector<std::string> out;
    for (auto line : lines) {
        out.emplace_back(line.data(), line.size());
    }
    EXPECT_EQ(out, (std::vector<std::string>{"Ala ma", "kota a kot", "ma Ale"}));
    for (auto line : lines) {
        EXPECT_LE(txt::columns(line), 10u);
    }

    // The hard breaks the text already has are kept
    auto kept = txt::wrap(string("a\nbb cc"), 10);
    EXPECT_EQ(kept.size(), 2u);
    EXPECT_EQ(txt::columns(kept[0]), 1u);

    // A piece wider than the limit takes a line of its own and overflows
    auto wide = txt::wrap(string("ab nieprzewidywalnie cd"), 6);
    EXPECT_EQ(wide.size(), 3u);
    EXPECT_EQ(txt::columns(wide[1]), 17u);

    // Two columns per ideograph, counted by the width not the bytes
    auto cjk = txt::wrap(string("漢字 漢字 漢字"), 5);
    EXPECT_EQ(cjk.size(), 3u);
    EXPECT_TRUE(txt::wrap(string(), 10).empty());
}

TEST(Segment_Tests, TruncatingToAWidth) {
    string s = "Ala ma kota";
    EXPECT_EQ(txt::truncate(s, 20), s);                      // it already fits: the same object
    EXPECT_EQ(txt::truncate(s, 20).data(), s.data());
    EXPECT_EQ(txt::truncate(s, 8), "Ala ma …");
    EXPECT_EQ(txt::columns(txt::truncate(s, 8)), 8u);
    EXPECT_EQ(txt::truncate(s, 8, "..."), "Ala m...");
    EXPECT_EQ(txt::columns(txt::truncate(s, 8, "...")), 8u);

    // The cut falls on a grapheme boundary, never inside a character
    string flags = "\U0001F1F5\U0001F1F1\U0001F1E9\U0001F1EA\U0001F1E8\U0001F1FF";
    auto cut = txt::truncate(flags, 5, "!");
    EXPECT_EQ(txt::grapheme_count(cut), 3u);                 // two flags and the mark
    EXPECT_LE(txt::columns(cut), 5u);
    EXPECT_EQ(txt::truncate(string("ééé"), 2, "!"), "é!");

    // Two columns per ideograph
    EXPECT_EQ(txt::truncate(string("漢字漢字"), 5, "!"), "漢字!");
}
