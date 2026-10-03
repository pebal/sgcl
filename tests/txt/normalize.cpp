//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
    template<class T> concept Classes = requires(T c) { txt::combining_class_of(c); };
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
    static_assert(txt::combining_class_of(U'a') == 0);
    static_assert(txt::combining_class_of(U'\u0301') == 230);      // above
    static_assert(txt::combining_class_of(U'\u0323') == 220);      // below
    static_assert(txt::combining_class_of(U'\u0328') == 202);
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
    EXPECT_EQ(string("e\u0301\u0323").runes().count_of([](char32_t c) { return txt::combining_class_of(c) != 0; }), 2u);
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

// An invalid byte of UTF-8 is U+FFFD and is in no form: normalize puts
// U+FFFD in its place and is_normalized says no, whatever the rest of the
// text is. The answer used to depend on the rest: beside text the quick
// check passed, the byte was kept and the text called normalized; beside
// a mark that made the check say "maybe" or "no", the byte was replaced
// and the text called not normalized.
TEST(Normalize_Tests, AnInvalidByteIsOneRuleWhateverStandsBesideIt) {
    string replacement("\ufffd");
    for (const char* rest : {"abc", "\u00e9", "e\u0301", "\u1e0b\u0323"}) {
        for (const char* bad : {"\xff", "\xc3", "\xe2\x82", "\xed\xa0\x80", "\xc0\xaf"}) {
            std::string raw = std::string(rest) + bad + "x";
            string text(raw.data(), raw.size());
            for (int form = 0; form < 4; ++form) {
                auto norm = form == 0 ? txt::normalize(text, txt::nfc)
                          : form == 1 ? txt::normalize(text, txt::nfd)
                          : form == 2 ? txt::normalize(text, txt::nfkc)
                          : txt::normalize(text, txt::nfkd);
                bool normal = form == 0 ? txt::is_normalized(text, txt::nfc)
                            : form == 1 ? txt::is_normalized(text, txt::nfd)
                            : form == 2 ? txt::is_normalized(text, txt::nfkc)
                            : txt::is_normalized(text, txt::nfkd);
                EXPECT_FALSE(normal) << raw << " form " << form;
                EXPECT_TRUE(utf8::valid(norm.view())) << raw << " form " << form;
                EXPECT_TRUE(norm.contains(replacement)) << raw << " form " << form;
                EXPECT_FALSE(norm == text) << raw << " form " << form;
            }
        }
    }
    // U+FFFD written as itself is a valid code point and in every form
    string written("abc\ufffd");
    EXPECT_TRUE(txt::is_normalized(written, txt::nfc));
    EXPECT_EQ(txt::normalize(written, txt::nfc).data(), written.data());
}

// without_marks: NFD, the nonspacing marks dropped, NFC. What it does
// not do is the half worth testing \u2014 a letter whose stroke is part of
// the letter keeps it, the case is untouched, and a spacing mark stays
// because it spells a vowel and is not an accent.
TEST(Normalize_Tests, TheMarksTakenOff) {
    EXPECT_EQ(txt::without_marks(string("caf\u00e9")), string("cafe"));
    EXPECT_EQ(txt::without_marks(string("Gr\u00fc\u00dfe")), string("Gru\u00dfe"));
    // the \u017c, the \u00f3 and the \u0107 lose their marks; the \u0142 has none to lose,
    // its stroke being part of the letter
    EXPECT_EQ(txt::without_marks(string("\u017c\u00f3\u0142\u0107")), string("zo\u0142c"));
    EXPECT_EQ(txt::without_marks(string("\u1f04\u03bd\u03b8\u03c1\u03c9\u03c0\u03bf\u03c2")), string("\u03b1\u03bd\u03b8\u03c1\u03c9\u03c0\u03bf\u03c2"));
    EXPECT_EQ(txt::without_marks(string("\u00c5ngstr\u00f6m")), string("Angstrom"));

    // the same whether the text arrived composed or decomposed
    EXPECT_EQ(txt::without_marks(txt::normalize(string("caf\u00e9"), txt::nfd)), string("cafe"));
    EXPECT_EQ(txt::without_marks(txt::normalize(string("caf\u00e9"), txt::nfc)), string("cafe"));

    // and what it is not: not a transliteration, so the letters whose
    // mark is part of the letter come through as they are
    EXPECT_EQ(txt::without_marks(string("\u0141\u00f3d\u017a")), string("\u0141odz"));
    EXPECT_EQ(txt::without_marks(string("\u00d8")), string("\u00d8"));
    EXPECT_EQ(txt::without_marks(string("\u0111")), string("\u0111"));
    EXPECT_EQ(txt::without_marks(string("\u00df")), string("\u00df"));
    EXPECT_EQ(txt::without_marks(string("\u0131")), string("\u0131"));
    // not a slug, so the case, the spaces and the punctuation stay
    EXPECT_EQ(txt::without_marks(string("Caf\u00e9 au Lait!")), string("Cafe au Lait!"));
    // and a spacing mark is spelling, not an accent: the Devanagari
    // vowel sign stays where a mark above would be taken off
    string ka_with_aa("\u0915\u093e");
    EXPECT_EQ(txt::without_marks(ka_with_aa), ka_with_aa);

    // a text with nothing to take off comes back as the object it was
    string plain("abc def");
    EXPECT_EQ(txt::without_marks(plain).data(), plain.data());
    string cyrillic("\u043f\u0435\u0440\u0435\u043c\u0435\u043d\u043d\u0430\u044f");
    EXPECT_EQ(txt::without_marks(cyrillic).data(), cyrillic.data());
    EXPECT_EQ(txt::without_marks(string("")), string(""));
}

// DESIGN 408: the code-point questions past the code space, the empty
// text, a text of marks alone, and a run of marks thousands long
TEST(Normalize_Tests, TheEdges) {
    // A value that is no code point: no class, no composition, and taken
    // apart into the replacement character, as every text of the module
    // writes one ("a code point with no decomposition is itself")
    for (char32_t c : {char32_t(0xD800), char32_t(0xDFFF), char32_t(0x110000), char32_t(0xFFFFFFFF)}) {
        EXPECT_EQ(txt::combining_class_of(c), 0) << std::hex << uint32_t(c);
        EXPECT_EQ(txt::compose(c, U'́'), 0u);
        EXPECT_EQ(txt::compose(U'e', c), 0u);
        EXPECT_EQ(txt::decompose(c), string("�")) << std::hex << uint32_t(c);
    }
    EXPECT_EQ(txt::decompose(U'\0'), string(1, '\0'));
    EXPECT_EQ(txt::decompose(char32_t(0x10FFFF)), string("\U0010FFFF"));
    EXPECT_EQ(txt::decompose(char32_t(0xD7A3)), string("힣"));    // the last syllable
    static_assert(txt::compose(U'ᄒ', U'ᅵ') == U'히');
    static_assert(txt::compose(U'히', U'ᇂ') == U'힣');
    static_assert(txt::compose(U'가', U'ᆧ') == 0);                        // T base is no trailing jamo
    static_assert(txt::compose(U'각', U'ᆨ') == 0);                        // an LVT takes no second one

    // Empty, in every form and every question
    string empty;
    EXPECT_EQ(txt::normalize(empty, txt::nfc), empty);
    EXPECT_EQ(txt::normalize(empty, txt::nfkd), empty);
    EXPECT_TRUE(txt::is_normalized(empty, txt::nfd));
    EXPECT_TRUE(txt::equal_normalized(empty, empty));
    EXPECT_EQ(txt::compare_normalized(empty, empty), 0);
    EXPECT_EQ(txt::compare_normalized(empty, string("a")), -1);
    EXPECT_EQ(txt::compare_normalized(string("a"), empty), 1);
    EXPECT_EQ(txt::hash_normalized(empty), txt::hash_normalized(string()));

    // A text of marks alone, with no starter to sit on, reordered all the same
    string marks("̣́");
    EXPECT_EQ(txt::normalize(marks, txt::nfd), string("̣́"));
    EXPECT_EQ(txt::normalize(marks, txt::nfc), string("̣́"));
    EXPECT_FALSE(txt::is_normalized(marks, txt::nfc));
    EXPECT_TRUE(txt::equal_normalized(marks, string("̣́")));
    EXPECT_EQ(txt::without_marks(marks), string());

    // Thousands of marks after one letter, in the worst order: they come
    // out sorted by class and stable within it
    std::string run = "a";
    for (int i = 0; i < 3000; ++i) {
        run += i % 2 ? "̣" : "́";                 // 230, 220, 230, 220, ...
    }
    string text(run.data(), run.size());
    auto nfd = txt::normalize(text, txt::nfd);
    std::u32string points;
    for (char32_t c : nfd.runes()) {
        points.push_back(c);
    }
    ASSERT_EQ(points.size(), 3001u);
    for (size_t i = 1; i <= 1500; ++i) {
        ASSERT_EQ(points[i], U'̣') << i;
    }
    EXPECT_EQ(points[1501], U'́');
    EXPECT_TRUE(txt::is_normalized(nfd, txt::nfd));
    EXPECT_TRUE(txt::equal_normalized(text, nfd));
    EXPECT_EQ(txt::hash_normalized(text), txt::hash_normalized(nfd));
    EXPECT_EQ(txt::normalize(text, txt::nfc).runes().count(), 3000u);   // a + U+0323 is ạ, the rest stay

    // Into itself
    string self("é");
    self = txt::normalize(self, txt::nfc);
    EXPECT_EQ(self, string("é"));
    EXPECT_TRUE(txt::equal_normalized(self, self));
    EXPECT_EQ(txt::compare_normalized(self, self), 0);
}
