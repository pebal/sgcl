//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
        for (auto it = txt::word_breaks(s).begin(); it != txt::word_breaks(s).end(); ++it) {
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
    for (auto w : txt::word_breaks(s)) {
        segments.emplace_back(w.data(), w.size());
    }
    EXPECT_EQ(segments, (std::vector<std::string>{"Ala", " ", "ma", " ", "kota", "."}));

    // The words themselves: the segments with something alphanumeric in them
    auto is_word = [](slice<const char> w) { return w.runes().exists(txt::is_alnum); };
    EXPECT_EQ(txt::word_breaks(s).count_of(is_word), 3u);

    // What the rules keep inside a word
    EXPECT_EQ(txt::word_breaks(string("don't")).count_of(is_word), 1u);        // the apostrophe
    EXPECT_EQ(txt::word_breaks(string("3.14")).count_of(is_word), 1u);         // the decimal point
    EXPECT_EQ(txt::word_breaks(string("192.168.0.1")).count_of(is_word), 1u);
    EXPECT_EQ(txt::word_breaks(string("e-mail")).count_of(is_word), 2u);       // and what it does not
    EXPECT_EQ(txt::word_breaks(string("\u0141\u00F3d\u017A nad Wis\u0142\u0105")).count_of(is_word), 3u);
    EXPECT_EQ(txt::word_breaks(string("can't stop, won't stop")).count_of(is_word), 4u);

    // A text with no letters at all, and no text
    EXPECT_EQ(txt::word_breaks(string("... ")).count_of(is_word), 0u);
    EXPECT_EQ(txt::word_breaks(string("... ")).count(), 4u);                 // each stop its own, the spaces one (WB3d)
    EXPECT_TRUE(txt::word_breaks(string()).empty());

    // An accent does not cut a word in two (rule WB4), and a flag is one
    EXPECT_EQ(txt::word_breaks(string("e\u0301gal")).count(), 1u);
    EXPECT_EQ(txt::word_breaks(string("\U0001F1F5\U0001F1F1")).count(), 1u);

    // The same range machinery as the graphemes
    static_assert(req::enumerable<txt::word_breaks>);
    EXPECT_EQ((*txt::word_breaks(s).begin()).data(), s.data());

    // words: the words alone, the same segments without the runs between
    std::vector<std::string> only;
    for (auto w : txt::words(s)) {
        only.emplace_back(w.data(), w.size());
    }
    EXPECT_EQ(only, (std::vector<std::string>{"Ala", "ma", "kota"}));
    EXPECT_EQ(txt::words(string("can't stop, won't stop")).count(), 4u);
    EXPECT_EQ(txt::words(string("... ")).count(), 0u);
    EXPECT_TRUE(txt::words(string("... ")).begin() == txt::words(string("... ")).end());
    EXPECT_TRUE(txt::words(string(" . ")).empty());          // segments, none of them a word
    EXPECT_FALSE(txt::word_breaks(string(" . ")).empty());
    EXPECT_EQ(txt::words(string(" 3.14 ")).count(), 1u);
    EXPECT_EQ((*txt::words(string("  x")).begin()).size(), 1u);   // the first word, past the spaces before it
    static_assert(req::enumerable<txt::words>);
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

namespace {
    // The pieces of a range tile the text: each one non-empty, each
    // starting where the one before it ended, the last ending at the end
    template<class Range>
    void expect_tiles(const Range& range, const string& text, const char* what) {
        size_t at = 0;
        size_t n = 0;
        for (auto it = range.begin(); it != range.end(); ++it) {
            ASSERT_EQ(it.pos(), at) << what;
            ASSERT_GT(it.size(), 0u) << what;
            ASSERT_EQ((*it).data(), text.data() + at) << what;
            at += it.size();
            ++n;
        }
        EXPECT_EQ(at, text.size()) << what;
        EXPECT_EQ(range.count(), n) << what;
        EXPECT_EQ(range.empty(), n == 0) << what;
    }
}

// DESIGN 408: a range default-constructed, copied and moved from; a text of
// one byte, every byte; broken UTF-8 anywhere; the cursor at and past the
// ends and inside a code point; the widths at their limits
TEST(Segment_Tests, TheRangesAtTheirEdges) {
    // Default-constructed: an empty text, no piece, begin is end
    EXPECT_TRUE(txt::graphemes().empty());
    EXPECT_EQ(txt::words().count(), 0u);
    EXPECT_TRUE(txt::word_breaks().begin() == txt::word_breaks().end());
    EXPECT_TRUE(txt::sentences().empty());
    EXPECT_EQ(txt::line_breaks().count(), 0u);
    EXPECT_TRUE(txt::line_breaks().begin() == txt::line_breaks().end());
    EXPECT_TRUE(txt::graphemes().text().empty());
    EXPECT_TRUE(txt::graphemes::iterator() == txt::graphemes::iterator());
    EXPECT_EQ((*txt::graphemes::iterator()).size(), 0u);
    EXPECT_EQ((*txt::line_breaks::iterator()).size(), 0u);

    // Copied and moved: the copy answers as the original, the moved-to too
    string s("ab cd. Ef!");
    txt::words w(s);
    auto copy = w;
    EXPECT_EQ(copy.count(), 3u);
    auto moved = std::move(copy);
    EXPECT_EQ(moved.count(), 3u);
    (void)copy.count();                                      // usable, whatever it holds
    txt::line_breaks lines(s);
    auto lines_moved = std::move(lines);
    EXPECT_EQ(lines_moved.count(), 3u);
    (void)lines.count();

    // Every byte alone is one grapheme, one word piece, one sentence and one line piece
    for (int b = 0; b < 256; ++b) {
        string one(1, char(b));
        ASSERT_EQ(txt::graphemes(one).count(), 1u) << b;
        ASSERT_EQ(txt::word_breaks(one).count(), 1u) << b;
        ASSERT_EQ(txt::sentences(one).count(), 1u) << b;
        ASSERT_EQ(txt::line_breaks(one).count(), 1u) << b;
        ASSERT_LE(txt::words(one).count(), 1u) << b;
        ASSERT_EQ(txt::words(one).count(), b < 0x80 && std::isalnum(b) ? 1u : 0u) << b;
    }

    // Random bytes: the pieces of every range tile the text, whatever it is
    std::mt19937 rng(20261003);
    std::uniform_int_distribution<int> byte(0, 255);
    std::uniform_int_distribution<int> length(0, 24);
    const char* alphabet[] = {"a", " ", ".", "\r", "\n", "\xCC\x81", "\xE2\x80\x8D", "\xF0\x9F\x87\xB5", "1", "'",
                              "\xE6\xBC\xA2", "\xFF", "\xC3", "\xE2\x82", "(", "\"", "\xD7\x90", "\xE0\xA4\x95"};
    for (int round = 0; round < 3000; ++round) {
        std::string raw;
        int n = length(rng);
        for (int i = 0; i < n; ++i) {
            if (round % 2) {
                raw.push_back(char(byte(rng)));
            } else {
                raw += alphabet[byte(rng) % std::size(alphabet)];
            }
        }
        string text(raw.data(), raw.size());
        expect_tiles(txt::graphemes(text), text, "graphemes");
        expect_tiles(txt::word_breaks(text), text, "word_breaks");
        expect_tiles(txt::sentences(text), text, "sentences");
        expect_tiles(txt::line_breaks(text), text, "line_breaks");
        if (::testing::Test::HasFatalFailure()) {
            FAIL() << "round " << round;
        }
        EXPECT_EQ(txt::grapheme_count(text), txt::graphemes(text).count());
        // The cursor: from every byte position, next and prev land on boundaries around it
        for (size_t pos = 0; pos <= text.size(); ++pos) {
            size_t start = txt::grapheme_start(text, pos);
            size_t next = txt::grapheme_next(text, pos);
            size_t prev = txt::grapheme_prev(text, pos);
            ASSERT_LE(start, pos);
            ASSERT_TRUE(next > pos || next == text.size());
            ASSERT_TRUE(prev < pos || pos == 0);
            ASSERT_EQ(txt::grapheme_start(text, start), start);
            ASSERT_EQ(txt::grapheme_start(text, next), next);
            ASSERT_EQ(txt::grapheme_start(text, prev), prev);
        }
    }
}

TEST(Segment_Tests, TheCursorAtItsEdges) {
    // Empty text: every position is 0
    string empty;
    EXPECT_EQ(txt::grapheme_start(empty, 0), 0u);
    EXPECT_EQ(txt::grapheme_next(empty, 0), 0u);
    EXPECT_EQ(txt::grapheme_prev(empty, 0), 0u);
    EXPECT_EQ(txt::grapheme_next(empty, 5), 0u);
    EXPECT_EQ(txt::grapheme_prev(empty, SIZE_MAX), 0u);

    // Past the end, far past it
    string s("a\U0001F600b");                                // 1 + 4 + 1 bytes
    EXPECT_EQ(txt::grapheme_start(s, SIZE_MAX), 6u);
    EXPECT_EQ(txt::grapheme_next(s, SIZE_MAX), 6u);
    EXPECT_EQ(txt::grapheme_prev(s, SIZE_MAX), 5u);
    // Inside the four bytes of one code point
    for (size_t pos = 2; pos < 5; ++pos) {
        EXPECT_EQ(txt::grapheme_start(s, pos), 1u) << pos;
        EXPECT_EQ(txt::grapheme_next(s, pos), 5u) << pos;
        EXPECT_EQ(txt::grapheme_prev(s, pos), 1u) << pos;
    }
    // Between the CR and the LF of one break
    string crlf("\r\n");
    EXPECT_EQ(txt::grapheme_start(crlf, 1), 0u);
    EXPECT_EQ(txt::grapheme_next(crlf, 1), 2u);
    EXPECT_EQ(txt::grapheme_prev(crlf, 1), 0u);

    // A C text: a null pointer is empty, an array is read to its first NUL
    const char* null = nullptr;
    EXPECT_EQ(txt::grapheme_count(null), 0u);
    EXPECT_EQ(txt::grapheme_next(null, 3), 0u);
    EXPECT_EQ(txt::grapheme_prev(null, 3), 0u);
    EXPECT_EQ(txt::grapheme_start(null, 3), 0u);
    const char stopped[] = {'a', 'b', '\0', 'c'};
    EXPECT_EQ(txt::grapheme_count(stopped), 2u);
    EXPECT_EQ(txt::grapheme_next(stopped, 2), 2u);
    const char full[] = {'a', 'b'};                           // no NUL at all
    EXPECT_EQ(txt::grapheme_count(full), 2u);
    EXPECT_EQ(txt::grapheme_prev(full, 9), 1u);
    EXPECT_TRUE(txt::graphemes(null).empty());
    EXPECT_TRUE(txt::line_breaks(null).empty());
    EXPECT_TRUE(txt::wrap(null, 10).empty());

    // A very long cluster: one letter and ten thousand marks is one grapheme
    std::string marks = "a";
    for (int i = 0; i < 10000; ++i) {
        marks += "\xCC\x81";
    }
    string cluster(marks.data(), marks.size());
    EXPECT_EQ(txt::grapheme_count(cluster), 1u);
    EXPECT_EQ(txt::grapheme_next(cluster, 0), cluster.size());
    EXPECT_EQ(txt::grapheme_prev(cluster, cluster.size()), 0u);
    EXPECT_EQ(txt::columns(cluster), 1u);
}

TEST(Segment_Tests, WrapAndTruncateAtTheirLimits) {
    auto texts = [](const vector<slice<const char>>& lines) {
        std::vector<std::string> out;
        for (auto& line : lines) {
            out.emplace_back(line.data(), line.size());
        }
        return out;
    };
    using lines_t = std::vector<std::string>;
    string s("ab cd ef");
    // Width 0: every piece a line of its own; the largest width: one line
    EXPECT_EQ(texts(txt::wrap(s, 0)), (lines_t{"ab", "cd", "ef"}));
    EXPECT_EQ(texts(txt::wrap(s, 1)), (lines_t{"ab", "cd", "ef"}));
    EXPECT_EQ(texts(txt::wrap(s, SIZE_MAX)), (lines_t{"ab cd ef"}));
    EXPECT_EQ(texts(txt::wrap(s, 5)), (lines_t{"ab cd", "ef"}));       // exactly the width
    // Hard breaks at the ends and in a row, and spaces alone
    EXPECT_EQ(texts(txt::wrap(string("\n"), 10)), (lines_t{""}));
    EXPECT_EQ(texts(txt::wrap(string("a\n"), 10)), (lines_t{"a"}));
    EXPECT_EQ(texts(txt::wrap(string("a\n\nb"), 10)), (lines_t{"a", "", "b"}));
    EXPECT_EQ(texts(txt::wrap(string("a\r\nb"), 10)), (lines_t{"a", "b"}));
    EXPECT_EQ(texts(txt::wrap(string("a   "), 10)), (lines_t{"a"}));
    EXPECT_EQ(texts(txt::wrap(string(" a"), 10)), (lines_t{" a"}));     // leading spaces stay
    EXPECT_EQ(texts(txt::wrap(string("   "), 10)), (lines_t{""}));
    // Broken UTF-8: a replacement a byte, one column each
    EXPECT_EQ(texts(txt::wrap(string("\xFF\xFF \xFF"), 2)), (lines_t{"\xFF\xFF", "\xFF"}));
    // The lines hold the text, and a line is a piece of it
    string held("x y");
    auto kept = txt::wrap(held, 1);
    EXPECT_EQ(kept[0].data(), held.data());

    // truncate: exactly the width is the same object, one column less is cut
    string fits("abcde");
    EXPECT_EQ(txt::truncate(fits, 5).data(), fits.data());
    EXPECT_EQ(txt::truncate(fits, 4), string("abc…"));
    EXPECT_EQ(txt::truncate(fits, 1), string("…"));
    EXPECT_EQ(txt::truncate(string(), 0), string());
    EXPECT_EQ(txt::truncate(fits, 3, ""), string("abc"));                // no ellipsis
    EXPECT_EQ(txt::truncate(fits, 0, ""), string());
    // An ellipsis wider than the limit: the widest prefix of it that fits
    EXPECT_EQ(txt::truncate(fits, 0), string());
    EXPECT_EQ(txt::truncate(fits, 2, "..."), string(".."));
    EXPECT_EQ(txt::truncate(fits, 3, "..."), string("..."));
    EXPECT_EQ(txt::truncate(fits, 1, "漢"), string());                   // a wide character does not fit half
    EXPECT_EQ(txt::truncate(fits, 3, "éééé"), string("ééé"));
    // A zero-width text past the limit only by its marks never is: columns decide
    EXPECT_EQ(txt::truncate(string("́́"), 0).size(), 4u);
    // Broken bytes are kept as they are, one column each
    EXPECT_EQ(txt::truncate(string("\xFF\xFF\xFF"), 2), string("\xFF…"));
    // Into itself
    string self("abcdef");
    self = txt::truncate(self, 3);
    EXPECT_EQ(self, string("ab…"));
}

namespace {
    // a = std::move(a) without the compiler's warning about it
    template<class T>
    void move_into_itself(T& a) {
        T& same = a;
        a = std::move(same);
    }
}

namespace {
    template<class Range>
    void expect_moved_from_is_empty(const char* what) {
        string text("Ala ma kota. Kot ma Ale.");
        Range range(text);
        size_t n = range.count();
        ASSERT_GT(n, 0u) << what;
        auto moved = std::move(range);
        EXPECT_EQ(moved.count(), n) << what;
        EXPECT_TRUE(range.empty()) << what;                    // NOLINT(bugprone-use-after-move)
        EXPECT_EQ(range.count(), 0u) << what;
        EXPECT_TRUE(range.text().empty()) << what;
        EXPECT_TRUE(range.begin() == range.end()) << what;
        range = Range(string("a b"));
        EXPECT_GT(range.count(), 0u) << what;
        size_t again = range.count();
        move_into_itself(range);
        EXPECT_EQ(range.count(), again) << what;
        Range assigned;
        assigned = std::move(moved);
        EXPECT_EQ(assigned.count(), n) << what;
        EXPECT_TRUE(moved.empty()) << what;                    // NOLINT(bugprone-use-after-move)
        auto copy = assigned;
        EXPECT_EQ(copy.count(), n) << what;
        EXPECT_EQ(assigned.count(), n) << what;
    }
}

// A range moved from is the empty one, as the one made with nothing
// (after DESIGN 429)
TEST(Segment_Tests, AMovedFromRangeIsTheEmptyOne) {
    expect_moved_from_is_empty<txt::graphemes>("graphemes");
    expect_moved_from_is_empty<txt::words>("words");
    expect_moved_from_is_empty<txt::word_breaks>("word_breaks");
    expect_moved_from_is_empty<txt::sentences>("sentences");
    expect_moved_from_is_empty<txt::line_breaks>("line_breaks");
}
