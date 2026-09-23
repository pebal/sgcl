//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::collate: the order a reader expects. The oracle is CollationTest of
// the UCA — a list of texts in the order the root collation puts them, so
// that every line must compare at or before the line after it, and the
// sort key of every line must compare the same way byte by byte.
#include "tests/types.h"
#include "tests/txt/collate_tests.h"
#include "tests/txt/collate_locale_tests.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace {
    // A line of the conformance file: the code points in hexadecimal
    string text_of(const char* line) {
        std::string out;
        char buf[utf8::max_width];
        uint32_t c = 0;
        bool any = false;
        auto flush = [&] {
            if (any) {
                out.append(buf, utf8::encode(char32_t(c), buf));
            }
            c = 0;
            any = false;
        };
        for (const char* p = line; *p; ++p) {
            if (*p == ' ') {
                flush();
            } else {
                char d = *p;
                c = c * 16 + uint32_t(d <= '9' ? d - '0' : (d | 32) - 'a' + 10);
                any = true;
            }
        }
        flush();
        return string(out.data(), out.size());
    }

    int by_bytes(const vector<std::byte>& a, const vector<std::byte>& b) {
        size_t n = a.size() < b.size() ? a.size() : b.size();
        for (size_t i = 0; i < n; ++i) {
            if (a[i] != b[i]) {
                return a[i] < b[i] ? -1 : 1;
            }
        }
        return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
    }
}

TEST(Collate_Tests, TheConformanceFileOfTheUCA) {
    txt::collator root;
    size_t cases = 0, stumbles = 0;
    string previous;
    vector<std::byte> previous_key;
    for (auto* line : ucd::CollationCases) {
        string text = text_of(line);
        auto key = root.key(text);
        if (cases) {
            if (root.compare(previous, text) > 0 || by_bytes(previous_key, key) > 0) {
                if (++stumbles <= 8) {
                    ADD_FAILURE() << line << " sorts before the line above it";
                }
            }
        }
        previous = text;
        previous_key = std::move(key);
        ++cases;
    }
    EXPECT_EQ(stumbles, 0u);
    EXPECT_EQ(cases, std::size(ucd::CollationCases));
    EXPECT_GT(cases, 60000u);
}

TEST(Collate_Tests, EveryLanguageAgainstICU) {
    // The order ICU puts a language's own letters in. ICU is the same
    // standard from other hands and other data, so where it and this
    // module agree on every one of these, the rules were read the same
    // way and the weights they ask for came out the same.
    size_t languages = 0, texts = 0;
    for (const auto& order : ucd::LocaleOrders) {
        txt::collator collator{txt::locale(string(order.language))};
        ASSERT_TRUE(collator.tailored()) << order.language;
        for (size_t i = 1; i < order.size; ++i) {
            string a(order.texts[i - 1]), b(order.texts[i]);
            ASSERT_LE(collator.compare(a, b), 0)
                << order.language << ": " << a << " sorts after " << b;
        }
        texts += order.size;
        ++languages;
    }
    EXPECT_EQ(languages, std::size(ucd::LocaleOrders));
    EXPECT_GT(texts, 4000u);
}

TEST(Collate_Tests, WhatALanguageExpects) {
    auto sorted_by = [](txt::collator collator, std::initializer_list<const char*> words) {
        vector<string> out;
        for (auto w : words) {
            out.push_back(string(w));
        }
        std::sort(out.begin(), out.end(), collator);
        std::string line;
        for (auto& w : out) {
            line += std::string(w.view()) + " ";
        }
        return line;
    };

    // The root order already knows that a capital is the same letter and
    // that an accent is a smaller difference than a letter
    txt::collator root;
    EXPECT_EQ(sorted_by(root, {"zebra", "Ala", "ósmy", "ada", "łoś", "résumé", "resume"}),
              "ada Ala łoś ósmy resume résumé zebra ");

    // Polish puts its own letters right after the ones they are made from
    txt::collator polish{txt::locale(string("pl"))};
    EXPECT_EQ(sorted_by(polish, {"zamek", "źrebak", "żaba", "ćma", "cebula", "łoś", "lis"}),
              "cebula ćma lis łoś zamek źrebak żaba ");

    // Danish and Swedish disagree about where ä and ö belong, which is
    // the plainest reason a collator takes a language at all
    txt::collator swedish{txt::locale(string("sv"))};
    EXPECT_EQ(sorted_by(swedish, {"öl", "äpple", "ål", "zebra"}), "zebra ål äpple öl ");
    txt::collator german{txt::locale(string("de"))};
    EXPECT_EQ(sorted_by(german, {"öl", "äpple", "zebra"}), "äpple öl zebra ");

    // Hungarian reads cs and zs as one letter each
    txt::collator hungarian{txt::locale(string("hu"))};
    EXPECT_EQ(sorted_by(hungarian, {"csak", "cukor", "zsák", "zebra"}), "cukor csak zebra zsák ");

    // Czech puts ch between h and i, where a Czech dictionary has it
    txt::collator czech{txt::locale(string("cs"))};
    EXPECT_EQ(sorted_by(czech, {"chyba", "irský", "hora", "cena"}), "cena hora chyba irský ");

    // and a language the tables know nothing about is the root order
    txt::collator klingon{txt::locale(string("tlh"))};
    EXPECT_FALSE(klingon.tailored());
    EXPECT_EQ(klingon.compare(string("ada"), string("Ala")), root.compare(string("ada"), string("Ala")));
}

TEST(Collate_Tests, HowMuchOfADifferenceCounts) {
    string resume = "resume", accented = "résumé", shouted = "RESUME";
    txt::collator primary{txt::strength::primary};
    txt::collator secondary{txt::strength::secondary};
    txt::collator tertiary;

    EXPECT_TRUE(primary.equal(resume, accented));       // one word to a search
    EXPECT_TRUE(primary.equal(resume, shouted));
    EXPECT_FALSE(secondary.equal(resume, accented));    // the accent is a difference
    EXPECT_TRUE(secondary.equal(resume, shouted));      // the case is not
    EXPECT_FALSE(tertiary.equal(resume, shouted));
    EXPECT_EQ(tertiary.compare(resume, resume), 0);
    EXPECT_EQ(tertiary.compare(string(), string()), 0);
    EXPECT_LT(tertiary.compare(string(), string("a")), 0);

    // The same text written two ways is one text at every strength
    EXPECT_TRUE(tertiary.equal(string("café"), string("cafe\u0301")));

    // A key compares byte by byte the way the collator compares texts
    auto key = [](const txt::collator& c, const char* s) { return c.key(string(s)); };
    auto less = [](const vector<std::byte>& a, const vector<std::byte>& b) {
        size_t n = a.size() < b.size() ? a.size() : b.size();
        for (size_t i = 0; i < n; ++i) {
            if (a[i] != b[i]) {
                return a[i] < b[i];
            }
        }
        return a.size() < b.size();
    };
    EXPECT_TRUE(less(key(tertiary, "ada"), key(tertiary, "Ala")));
    EXPECT_TRUE(less(key(tertiary, "resume"), key(tertiary, "résumé")));
    EXPECT_EQ(key(primary, "resume"), key(primary, "RESUME"));
    EXPECT_NE(key(tertiary, "resume"), key(tertiary, "RESUME"));
    EXPECT_TRUE(key(tertiary, "").empty() || key(tertiary, "").size() == 4);

    // A language's key is its own
    txt::collator polish{txt::locale(string("pl"))};
    EXPECT_NE(polish.key(string("żaba")), tertiary.key(string("żaba")));
    EXPECT_EQ(polish.where(), txt::locale(string("pl")));
    EXPECT_EQ(polish.level(), txt::strength::tertiary);
}

TEST(Collate_Tests, AKeyIntoTheCallersBuffer) {
    txt::collator root, polish{txt::locale(string("pl"))};
    string word = "żaba";

    // The same bytes as the key that allocates, whatever the strength
    for (auto c : {root, polish, txt::collator(txt::strength::primary),
                   txt::collator(txt::strength::secondary)}) {
        auto want = c.key(word);
        std::byte buffer[256];
        size_t n = c.key(word, buffer);
        ASSERT_EQ(n, want.size());
        for (size_t i = 0; i < n; ++i) {
            ASSERT_EQ(buffer[i], want[i]) << i;
        }
    }

    // A buffer too small is told how much it needs and is not written
    auto want = polish.key(word);
    ASSERT_GT(want.size(), 4u);
    std::byte small[4];
    std::memset(small, 0xCD, sizeof small);
    EXPECT_EQ(polish.key(word, small), want.size());
    for (auto b : small) {
        EXPECT_EQ(b, std::byte(0xCD));          // nothing written: half a key is not a key
    }

    // and an empty buffer is how a caller asks what it needs
    EXPECT_EQ(polish.key(word, slice<std::byte>()), want.size());
    EXPECT_EQ(polish.key(string(), slice<std::byte>()), 4u);   // the two level boundaries

    // The keys still order the words the way the collator does
    std::byte a[128], b[128];
    size_t na = polish.key(string("łoś"), a), nb = polish.key(string("zamek"), b);
    int by_bytes = std::memcmp(a, b, na < nb ? na : nb);
    EXPECT_LT(by_bytes, 0);
    EXPECT_LT(polish.compare(string("łoś"), string("zamek")), 0);
}
