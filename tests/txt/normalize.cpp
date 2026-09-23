//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::normalize: the four forms of UAX #15. The oracle is the UCD's
// conformance file, every line of it, with the invariants the standard
// states beside each line; the rest checks the interface — the form as a
// tag, the text that is already in the form coming back as itself, and
// the comparisons that do not care how a text was written.
#include "tests/types.h"
#include "tests/txt/normalization_tests.h"

#include <string>

namespace {
    // A call to a deleted function in a requires-expression outside a
    // template is a hard error, so the refusal is seen through a concept
    template<class T> concept Classes = requires(T c) { txt::combining_class(c); };
    template<class T> concept Composes = requires(T c) { txt::compose(c, c); };

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

TEST(Normalize_Tests, EveryLineOfTheUcdConformanceFile) {
    size_t lines = 0;
    for (const auto& c : ucd::NormalizationTests) {
        string source = utf8_of(c.source);
        string nfc = utf8_of(c.nfc);
        string nfd = utf8_of(c.nfd);
        string nfkc = utf8_of(c.nfkc);
        string nfkd = utf8_of(c.nfkd);
        auto why = [&] { return describe(c.source); };

        // The five columns are one text in five forms, so normalizing any
        // of them gives the column of that form
        for (const auto& x : {source, nfc, nfd}) {
            ASSERT_EQ(txt::normalize(x, txt::nfc), nfc) << why();
            ASSERT_EQ(txt::normalize(x, txt::nfd), nfd) << why();
        }
        ASSERT_EQ(txt::normalize(nfkc, txt::nfc), nfkc) << why();
        ASSERT_EQ(txt::normalize(nfkd, txt::nfd), nfkd) << why();
        for (const auto& x : {source, nfc, nfd, nfkc, nfkd}) {
            ASSERT_EQ(txt::normalize(x, txt::nfkc), nfkc) << why();
            ASSERT_EQ(txt::normalize(x, txt::nfkd), nfkd) << why();
        }

        // A text in a form says so, and one that is not does not
        ASSERT_TRUE(txt::is_normalized(nfc, txt::nfc)) << why();
        ASSERT_TRUE(txt::is_normalized(nfd, txt::nfd)) << why();
        ASSERT_TRUE(txt::is_normalized(nfkc, txt::nfkc)) << why();
        ASSERT_TRUE(txt::is_normalized(nfkd, txt::nfkd)) << why();
        ASSERT_EQ(txt::is_normalized(source, txt::nfc), source == nfc) << why();
        ASSERT_EQ(txt::is_normalized(source, txt::nfd), source == nfd) << why();

        // The five columns are the same text, however they are written
        ASSERT_TRUE(txt::equal_normalized(source, nfc)) << why();
        ASSERT_TRUE(txt::equal_normalized(nfc, nfd)) << why();
        ASSERT_EQ(txt::hash_normalized(nfc), txt::hash_normalized(nfd)) << why();
        ASSERT_EQ(txt::compare_normalized(nfc, nfd), 0) << why();
        ++lines;
    }
    EXPECT_EQ(lines, std::size(ucd::NormalizationTests));
    EXPECT_GT(lines, 19000u);
}

TEST(Normalize_Tests, EveryCodePointOutsidePartOne) {
    // What part one does not name is its own normalization in all four
    // forms — the other half of the conformance test
    std::vector<bool> named(0x110000, false);
    for (const auto& c : ucd::NormalizationTests) {
        if (c.part1) {
            named[c.source[0]] = true;
        }
    }
    size_t checked = 0;
    for (char32_t c = 0; c < 0x110000; ++c) {
        if (named[c] || (c >= 0xD800 && c <= 0xDFFF)) {
            continue;
        }
        string x = utf8_of((char32_t[]){c, 0});
        ASSERT_EQ(txt::normalize(x, txt::nfc), x) << std::hex << uint32_t(c);
        ASSERT_EQ(txt::normalize(x, txt::nfd), x) << std::hex << uint32_t(c);
        ASSERT_EQ(txt::normalize(x, txt::nfkc), x) << std::hex << uint32_t(c);
        ASSERT_EQ(txt::normalize(x, txt::nfkd), x) << std::hex << uint32_t(c);
        ++checked;
    }
    EXPECT_GT(checked, 1000000u);
}

TEST(Normalize_Tests, TheFourFormsOfATextAReaderWrites) {
    // The same letter, written two ways
    string composed = "é";              // é
    string decomposed = "e\u0301";           // e and a combining acute
    EXPECT_NE(composed, decomposed);
    EXPECT_EQ(composed.size(), 2u);
    EXPECT_EQ(decomposed.size(), 3u);
    EXPECT_EQ(txt::normalize(decomposed, txt::nfc), composed);
    EXPECT_EQ(txt::normalize(composed, txt::nfd), decomposed);
    EXPECT_TRUE(txt::equal_normalized(composed, decomposed));
    EXPECT_EQ(txt::hash_normalized(composed), txt::hash_normalized(decomposed));

    // A Korean syllable: one code point or three, and the arithmetic is
    // in the algorithm rather than in a table
    string syllable = "\uAC01";
    string jamo = "\u1100\u1161\u11A8";
    EXPECT_EQ(txt::normalize(syllable, txt::nfd), jamo);
    EXPECT_EQ(txt::normalize(jamo, txt::nfc), syllable);
    EXPECT_TRUE(txt::equal_normalized(syllable, jamo));

    // The marks of one letter are put in the order the standard fixes,
    // whichever order they arrived in
    EXPECT_EQ(txt::normalize(string("q\u0307\u0323"), txt::nfd), string("q\u0323\u0307"));
    EXPECT_TRUE(txt::equal_normalized(string("q\u0307\u0323"), string("q\u0323\u0307")));

    // The compatibility forms change what a text means, not how it looks:
    // a ligature is letters, a circled digit is a digit, a fullwidth
    // letter is a plain one. It cannot be undone
    EXPECT_EQ(txt::normalize(string("\uFB01"), txt::nfkc), string("fi"));
    EXPECT_EQ(txt::normalize(string("\u2460"), txt::nfkc), string("1"));
    EXPECT_EQ(txt::normalize(string("\uFF21"), txt::nfkc), string("A"));
    EXPECT_EQ(txt::normalize(string("½"), txt::nfkc), string("1\u20442"));
    EXPECT_EQ(txt::normalize(string("\uFB01"), txt::nfc), string("\uFB01"));   // nfc leaves it alone

    // A text already in the form is handed back as the same object, not
    // as a copy: the strings of the library are shared, so this is free
    string plain = "Ala ma kota";
    EXPECT_EQ(txt::normalize(plain, txt::nfc).data(), plain.data());
    EXPECT_EQ(txt::normalize(plain, txt::nfkd).data(), plain.data());
    EXPECT_EQ(txt::normalize(composed, txt::nfc).data(), composed.data());
    EXPECT_NE(txt::normalize(composed, txt::nfd).data(), composed.data());

    // An empty text, and one that is only marks
    EXPECT_EQ(txt::normalize(string(), txt::nfc), string());
    EXPECT_TRUE(txt::is_normalized(string(), txt::nfd));
    EXPECT_EQ(txt::normalize(string("\u0301"), txt::nfc), string("\u0301"));
}

TEST(Normalize_Tests, TheQuestionsAboutOneCodePoint) {
    static_assert(txt::combining_class(U'a') == 0);
    static_assert(txt::combining_class(U'\u0301') == 230);      // above
    static_assert(txt::combining_class(U'\u0323') == 220);      // below
    static_assert(txt::combining_class(U'\u0328') == 202);
    static_assert(txt::compose(U'e', U'\u0301') == U'é');
    static_assert(txt::compose(U'a', U'b') == 0);
    static_assert(txt::compose(U'\u1100', U'\u1161') == U'\uAC00');          // Hangul, by arithmetic
    static_assert(txt::compose(U'\uAC00', U'\u11A8') == U'\uAC01');
    static_assert(txt::compose(U'A', U'\u030A') == U'Å');
    static_assert(txt::compose(U'\u2ADC', U'\u0338') == 0);                  // an exclusion: it does not compose back

    EXPECT_EQ(txt::decompose(U'é'), string("e\u0301"));
    EXPECT_EQ(txt::decompose(U'\uAC01'), string("\u1100\u1161\u11A8"));
    EXPECT_EQ(txt::decompose(U'a'), string("a"));
    EXPECT_EQ(txt::decompose(U'\uFB01'), string("\uFB01"));                  // canonical only: a ligature stays

    // A code point and nothing else, as everywhere in this module
    static_assert(Classes<char32_t> && !Classes<char> && !Classes<int>);
    static_assert(Composes<char32_t> && !Composes<char> && !Composes<int>);

    // And the names are predicates like the others
    EXPECT_EQ(string("e\u0301\u0323").runes().count_of([](char32_t c) { return txt::combining_class(c) != 0; }), 2u);
}

TEST(Normalize_Tests, ComparingWithoutCaringHowItIsWritten) {
    string a = "Å";                 // Å
    string b = "A\u030A";                // A and a ring above
    string c = "\u212B";                 // the angstrom sign, which normalizes to Å
    EXPECT_TRUE(txt::equal_normalized(a, b));
    EXPECT_TRUE(txt::equal_normalized(a, c));
    EXPECT_EQ(txt::compare_normalized(a, c), 0);
    EXPECT_EQ(txt::hash_normalized(a), txt::hash_normalized(c));
    EXPECT_FALSE(txt::equal_normalized(a, string("B")));

    // An order that does not depend on the writing
    EXPECT_LT(txt::compare_normalized(string("a"), string("b")), 0);
    EXPECT_GT(txt::compare_normalized(string("b"), string("a")), 0);
    EXPECT_LT(txt::compare_normalized(string("a"), string("a\u0301")), 0);
    EXPECT_EQ(txt::compare_normalized(string(), string()), 0);

    // A map whose keys do not care which form a name arrived in. The
    // map is the library's: a string holds a tracked pointer, which must
    // live on the stack or inside a managed object, and a node of a
    // std::unordered_map is neither — the debug build says so
    map<size_t, string> by_hash;
    by_hash[txt::hash_normalized(a)] = a;
    EXPECT_EQ(by_hash.count(txt::hash_normalized(b)), 1u);
    EXPECT_EQ(by_hash[txt::hash_normalized(c)], a);
}

// normalize() used to answer through is_normalized(), which normalizes a
// "maybe" text to decide and then threw the answer away, so a text that
// did need normalizing was normalized twice. It is done once now, and
// the text that needs nothing must still come back as the object it went
// in as, a string being shared and immutable.
TEST(Normalize_Tests, TheWorkIsDoneOnce) {
    string plain("abc def");
    string composed("\u00e9\u00e9\u00e9");
    string decomposed("e\u0301e\u0301e\u0301");

    // Already in the form: the same object, not a copy of it
    EXPECT_EQ(txt::normalize(plain, txt::nfc).data(), plain.data());
    EXPECT_EQ(txt::normalize(composed, txt::nfc).data(), composed.data());
    EXPECT_EQ(txt::normalize(decomposed, txt::nfd).data(), decomposed.data());

    // And the work itself is unchanged
    EXPECT_EQ(txt::normalize(decomposed, txt::nfc), composed);
    EXPECT_EQ(txt::normalize(composed, txt::nfd), decomposed);
    EXPECT_TRUE(txt::is_normalized(composed, txt::nfc));
    EXPECT_FALSE(txt::is_normalized(decomposed, txt::nfc));

    // A text the quick check calls "maybe" and which turns out to be in
    // the form takes the road that compares, and must come back as itself
    string maybe("a\u0301bc");
    EXPECT_EQ(txt::normalize(maybe, txt::nfd).data(), maybe.data());
}
