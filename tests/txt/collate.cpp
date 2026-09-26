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
#include "tests/txt/collate_option_tests.h"

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

    int by_bytes(const vector<byte>& a, const vector<byte>& b) {
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
    vector<byte> previous_key;
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
    auto less = [](const vector<byte>& a, const vector<byte>& b) {
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
        byte buffer[256];
        size_t n = c.key_to(buffer, word);
        ASSERT_EQ(n, want.size());
        for (size_t i = 0; i < n; ++i) {
            ASSERT_EQ(buffer[i], want[i]) << i;
        }
    }

    // A buffer too small is told how much it needs and is not written
    auto want = polish.key(word);
    ASSERT_GT(want.size(), 4u);
    byte small[4];
    std::memset(small, 0xCD, sizeof small);
    EXPECT_EQ(polish.key_to(small, word), want.size());
    for (auto b : small) {
        EXPECT_EQ(b, byte(0xCD));          // nothing written: half a key is not a key
    }

    // and an empty buffer is how a caller asks what it needs
    EXPECT_EQ(polish.key_to(slice<byte>(), word), want.size());
    EXPECT_EQ(polish.key_to(slice<byte>(), string()), 4u);   // the two level boundaries

    // The keys still order the words the way the collator does
    byte a[128], b[128];
    size_t na = polish.key_to(a, string("łoś")), nb = polish.key_to(b, string("zamek"));
    int by_bytes = std::memcmp(a, b, na < nb ? na : nb);
    EXPECT_LT(by_bytes, 0);
    EXPECT_LT(polish.compare(string("łoś"), string("zamek")), 0);
}

//------------------------------------------------------------------------------
// the settings: what a caller asks for beside the order of the letters
//------------------------------------------------------------------------------
namespace {
    // A collator built the way a row of the oracle was built. A setting
    // the row does not give is the language's own, which is what the
    // four languages that carry one are there to check.
    txt::collator collator_of(const ucd::OptionOrder& row) {
        txt::options how;
        how.strength = txt::strength(row.strength);
        if (row.punctuation >= 0) {
            how.punctuation = txt::punctuation(row.punctuation);
        }
        if (row.case_order >= 0) {
            how.case_order = txt::case_order(row.case_order);
        }
        if (row.case_level >= 0) {
            how.case_level = row.case_level != 0;
        }
        if (row.backwards >= 0) {
            how.backwards = row.backwards != 0;
        }
        how.numeric = row.numeric;
        return row.language[0] ? txt::collator(txt::locale(string(row.language)), how)
                               : txt::collator(how);
    }

    int sign(int d) {
        return d < 0 ? -1 : (d > 0 ? 1 : 0);
    }

    // What a key says about two texts, which must be what the collator
    // says about them
    int by_key(const txt::collator& c, const string& a, const string& b) {
        return sign(by_bytes(c.key(a), c.key(b)));
    }
}

TEST(Collate_Tests, EverySettingAgainstICU) {
    size_t settings = 0, pairs = 0;
    for (const auto& row : ucd::OptionOrders) {
        auto collator = collator_of(row);
        for (size_t i = 1; i < row.size; ++i) {
            string a(row.texts[i - 1]), b(row.texts[i]);
            ASSERT_EQ(sign(collator.compare(a, b)), row.signs[i - 1])
                << row.language << " #" << settings << ": " << a << " against " << b;
            ASSERT_EQ(by_key(collator, a, b), row.signs[i - 1])
                << row.language << " #" << settings << ": the key of " << a << " against " << b;
            ++pairs;
        }
        ++settings;
    }
    EXPECT_EQ(settings, std::size(ucd::OptionOrders));
    EXPECT_GT(pairs, 1000u);
}

TEST(Collate_Tests, AKeyIsTheComparisonWhateverTheSettings) {
    // The fault a setting invites is a key that does not say what the
    // comparison says — a level written that is not compared, or
    // compared and not written, or written the wrong way round. So
    // every setting is asked about every pair of a list of texts made
    // for the purpose, and the key must give the same sign as the
    // comparison, both for the key that allocates and for the one
    // written into a buffer of the caller's.
    const char* texts[] = {
        "resume", "résumé", "RESUME", "Resume", "resumé", "RÉSUMÉ",
        "cote", "côte", "coté", "côté", "COTÉ",
        "de luxe", "de-luxe", "deluxe", "De-Luxe", "de.luxe", "-deluxe", "deluxe-",
        "plik2", "plik9", "plik10", "plik09", "plik0009", "plik100",
        "9", "10", "007", "7", "0", "00", "x2y", "x10y", "١٢", "12",
        "99999999999999999999999", "99999999999999999999998", "999999999999999999999990",
        "", "a", "A", "ala", "Ala", "ALA", "ała", "łoś", "zebra", "Ćma", "ćma",
        "ch", "ca", "cz", "ff", "æther", "aether", "Æther",
    };
    txt::locale languages[] = {txt::locale(), txt::locale(string("pl")), txt::locale(string("da")),
                               txt::locale(string("cs")), txt::locale(string("cu"))};
    size_t checked = 0;
    for (auto where : languages) {
        for (int bits = 0; bits < 64; ++bits) {
            txt::options how;
            how.strength = txt::strength(bits & 3);
            how.punctuation = (bits & 4) ? txt::punctuation::shifted : txt::punctuation::counted;
            how.case_order = (bits & 8) ? txt::case_order::upper_first : txt::case_order::lower_first;
            how.case_level = (bits & 16) != 0;
            how.backwards = (bits & 32) != 0;
            how.numeric = (bits & 4) == 0 && (bits & 8) != 0;   // half the settings ask for it
            txt::collator collator{where, how};
            byte buffer[512];
            for (size_t i = 0; i < std::size(texts); ++i) {
                for (size_t j = i + 1; j < std::size(texts); ++j) {
                    string a(texts[i]), b(texts[j]);
                    int wanted = sign(collator.compare(a, b));
                    ASSERT_EQ(by_key(collator, a, b), wanted)
                        << "bits " << bits << ": " << a << " against " << b;
                    // and the key into a buffer is the same key
                    auto want = collator.key(a);
                    size_t n = collator.key_to(buffer, a);
                    ASSERT_EQ(n, want.size()) << "bits " << bits << ": " << a;
                    for (size_t k = 0; k < n; ++k) {
                        ASSERT_EQ(buffer[k], want[k]) << "bits " << bits << ": " << a << " at " << k;
                    }
                    ++checked;
                }
            }
        }
    }
    EXPECT_GT(checked, 400000u);
}

TEST(Collate_Tests, ARunOfDigitsIsANumber) {
    txt::collator plain;
    txt::collator numeric{txt::options{.numeric = true}};

    // what the option is for
    EXPECT_LT(numeric.compare(string("plik9"), string("plik10")), 0);
    EXPECT_GT(plain.compare(string("plik9"), string("plik10")), 0);    // 9 after 1, letter by letter
    EXPECT_LT(numeric.compare(string("2"), string("10")), 0);
    EXPECT_LT(numeric.compare(string("x2y"), string("x10y")), 0);

    // the leading zeros are not part of the number
    EXPECT_EQ(numeric.compare(string("007"), string("7")), 0);
    EXPECT_EQ(numeric.compare(string("0"), string("00000")), 0);
    EXPECT_LT(numeric.compare(string("0"), string("1")), 0);
    EXPECT_LT(numeric.compare(string("plik09"), string("plik10")), 0);

    // a number longer than any integer holds, which is the reason the
    // digits are counted rather than added up
    string long_a, long_b;
    {
        std::string a(400, '9'), b(400, '9');
        b[399] = '8';
        long_a = string(a.data(), a.size());
        long_b = string(b.data(), b.size());
    }
    EXPECT_GT(numeric.compare(long_a, long_b), 0);
    EXPECT_EQ(numeric.compare(long_a, long_a), 0);
    EXPECT_LT(numeric.compare(long_b, string(("1" + std::string(400, '0')).data(), 401)), 0);

    // the digits of another script are digits too
    EXPECT_LT(numeric.compare(string("٩"), string("١٠")), 0);          // Arabic-Indic 9 and 10

    // and a key says what the comparison says
    auto less = [&numeric](const char* a, const char* b) {
        return by_bytes(numeric.key(string(a)), numeric.key(string(b))) < 0;
    };
    EXPECT_TRUE(less("plik9", "plik10"));
    EXPECT_TRUE(less("plik10", "plik100"));
    EXPECT_FALSE(less("plik100", "plik10"));
    EXPECT_EQ(numeric.key(string("007")), numeric.key(string("7")));

    // a number sorts where the digits it is written with sort: before
    // the letters and after the punctuation
    EXPECT_LT(numeric.compare(string("12"), string("ab")), 0);
    EXPECT_GT(numeric.compare(string("12"), string("-")), 0);
}

TEST(Collate_Tests, PunctuationShiftedAside) {
    txt::collator counted;
    txt::collator shifted{txt::options{.punctuation = txt::punctuation::shifted}};
    txt::collator fourth{txt::options{.strength = txt::strength::quaternary,
                                      .punctuation = txt::punctuation::shifted}};

    // look as though the hyphen were not there
    EXPECT_TRUE(shifted.equal(string("re-sume"), string("resume")));
    EXPECT_TRUE(shifted.equal(string("de luxe"), string("deluxe")));
    EXPECT_FALSE(counted.equal(string("re-sume"), string("resume")));

    // a letter is still a letter, and the accents and the case are
    // still what they were
    EXPECT_FALSE(shifted.equal(string("re-sume"), string("résumé")));
    EXPECT_FALSE(shifted.equal(string("re-sume"), string("RESUME")));
    EXPECT_LT(shifted.compare(string("de-luxe"), string("demain")), 0);

    // at the fourth level the punctuation is a difference again, and a
    // smaller one than any letter
    EXPECT_FALSE(fourth.equal(string("re-sume"), string("resume")));
    EXPECT_LT(fourth.compare(string("re-sume"), string("resume")), 0);
    EXPECT_LT(fourth.compare(string("resume"), string("rezume")), 0);

    // and the keys agree with all of it
    EXPECT_EQ(shifted.key(string("re-sume")), shifted.key(string("resume")));
    EXPECT_NE(fourth.key(string("re-sume")), fourth.key(string("resume")));
    EXPECT_LT(by_bytes(fourth.key(string("re-sume")), fourth.key(string("resume"))), 0);
}

TEST(Collate_Tests, TheCaseOnALevelOfItsOwn) {
    txt::collator plain;
    txt::collator cased{txt::options{.strength = txt::strength::primary, .case_level = true}};
    txt::collator upper{txt::options{.case_order = txt::case_order::upper_first}};

    // the letters and the case, and nothing else: what a search over
    // names wants when it must still tell IBM from ibm
    EXPECT_TRUE(cased.equal(string("resume"), string("résumé")));      // the accent is nothing
    EXPECT_FALSE(cased.equal(string("resume"), string("RESUME")));     // the case is not
    EXPECT_LT(cased.compare(string("resume"), string("RESUME")), 0);
    EXPECT_EQ(cased.key(string("resume")), cased.key(string("résumé")));
    EXPECT_NE(cased.key(string("resume")), cased.key(string("RESUME")));

    // the capitals first, which is what Danish asks for
    EXPECT_GT(plain.compare(string("Ala"), string("ala")), 0);
    EXPECT_LT(upper.compare(string("Ala"), string("ala")), 0);
    EXPECT_LT(by_bytes(upper.key(string("Ala")), upper.key(string("ala"))), 0);
    // and it is a difference of case, not of letter: the letters still
    // settle it first
    EXPECT_LT(upper.compare(string("ala"), string("Ola")), 0);

    // a language that asks for it, and a caller who asks otherwise
    txt::collator danish{txt::locale(string("da"))};
    EXPECT_TRUE(danish.capitals_first());
    EXPECT_LT(danish.compare(string("Ala"), string("ala")), 0);
    txt::collator danish_lower{txt::locale(string("da")),
                               txt::options{.case_order = txt::case_order::lower_first}};
    EXPECT_GT(danish_lower.compare(string("Ala"), string("ala")), 0);
    EXPECT_EQ(danish_lower.compare(string("æble"), string("øl")),
              danish.compare(string("æble"), string("øl")));      // the letters are still Danish
}

TEST(Collate_Tests, AccentsFromTheEndOfTheWord) {
    // The rule of Canadian French, and the four words every account of
    // it uses: read from the front, an accent early in the word settles
    // it; read from the end, a late one does.
    txt::collator forwards;
    txt::collator backwards{txt::options{.backwards = true}};
    const char* words[] = {"cote", "coté", "côte", "côté"};

    EXPECT_LT(forwards.compare(string("cote"), string("coté")), 0);
    EXPECT_LT(forwards.compare(string("coté"), string("côte")), 0);
    EXPECT_LT(backwards.compare(string("cote"), string("côte")), 0);
    EXPECT_LT(backwards.compare(string("côte"), string("coté")), 0);
    EXPECT_LT(backwards.compare(string("coté"), string("côté")), 0);

    for (size_t i = 1; i < std::size(words); ++i) {
        string a(words[i - 1]), b(words[i]);
        EXPECT_EQ(sign(backwards.compare(a, b)), sign(by_bytes(backwards.key(a), backwards.key(b))))
            << a << " against " << b;
    }

    // Church Slavonic asks for it in its own rules, and for the case on
    // a level of its own and its capitals first
    txt::collator slavonic{txt::locale(string("cu"))};
    EXPECT_TRUE(slavonic.backwards());
    EXPECT_TRUE(slavonic.case_level());
    EXPECT_TRUE(slavonic.capitals_first());
    // and Thai asks for its punctuation to be shifted aside
    txt::collator thai{txt::locale(string("th"))};
    EXPECT_TRUE(thai.shifts_punctuation());
    EXPECT_TRUE(thai.equal(string("re-sume"), string("resume")));
    // and what a collator settled on is asked of it as a question,
    // never by unwrapping the optional it was given
    txt::collator asked{txt::locale(string("da")),
                        txt::options{.case_order = txt::case_order::lower_first,
                                     .numeric = true}};
    EXPECT_FALSE(asked.capitals_first());       // the caller overruled the language
    EXPECT_TRUE(asked.numeric());
    EXPECT_FALSE(asked.backwards());
    EXPECT_FALSE(asked.shifts_punctuation());
    EXPECT_FALSE(asked.case_level());
    EXPECT_EQ(asked.level(), txt::strength::tertiary);

    // a language that asks for nothing gets nothing
    txt::collator polish{txt::locale(string("pl"))};
    EXPECT_FALSE(polish.backwards());
    EXPECT_FALSE(polish.case_level());
    EXPECT_FALSE(polish.shifts_punctuation());
    EXPECT_FALSE(polish.capitals_first());
}

TEST(Collate_Tests, FindingOneTextInsideAnother) {
    string text = "Le résumé du candidat";
    txt::collator search{txt::options{.strength = txt::strength::primary}};

    // the point of a search by collation: the accent is not a
    // difference, and the answer is in the bytes of the text as it was
    // given rather than of any form of its own
    auto hit = search.find(text, string("resume"));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->at, 3u);
    EXPECT_EQ(hit->size, 8u);                       // "résumé" is eight bytes
    EXPECT_EQ(std::string(text.view().substr(hit->at, hit->size)), "résumé");
    EXPECT_TRUE(search.contains(text, string("RESUME")));
    EXPECT_TRUE(search.contains(text, string("candidat")));
    EXPECT_FALSE(search.contains(text, string("resumes")));
    EXPECT_FALSE(search.find(text, string("xyz")).has_value());

    // both ends of the text
    EXPECT_TRUE(search.starts_with(text, string("le")));
    EXPECT_TRUE(search.starts_with(text, string("LE RESUME")));
    EXPECT_FALSE(search.starts_with(text, string("resume")));
    EXPECT_TRUE(search.ends_with(text, string("CANDIDAT")));
    EXPECT_FALSE(search.ends_with(text, string("candida")));

    // where to look from, and the empty pattern
    EXPECT_EQ(search.find(text, string("u"))->at, 7u);       // the u of "résumé"
    EXPECT_EQ(search.find(text, string("u"), 8)->at, 13u);    // and the u of "du"

    ASSERT_TRUE(search.find(text, string()).has_value());
    EXPECT_EQ(search.find(text, string())->size, 0u);
    EXPECT_TRUE(search.starts_with(text, string()));
    EXPECT_TRUE(search.ends_with(text, string()));

    // a match begins and ends on a letter, never inside one: the accent
    // of a "résumé" belongs to the letter it sits on and so to the
    // match, and the acute alone is not a thing to be found
    EXPECT_FALSE(search.find(text, string("́")).has_value());
    EXPECT_EQ(search.find(string("café"), string("cafe"))->size, 6u);
    EXPECT_EQ(search.find(string("café"), string("cafe"))->size, 5u);

    // nor may it cut a contraction: to a Czech "ch" is one letter, and
    // a "c" is not the first half of it
    txt::collator czech{txt::locale(string("cs")), txt::options{.strength = txt::strength::primary}};
    EXPECT_TRUE(czech.contains(string("chyba"), string("ch")));
    EXPECT_FALSE(czech.contains(string("chyba"), string("c")));
    EXPECT_TRUE(search.contains(string("chyba"), string("c")));       // the root has no such letter

    // what the settings do to a search: with the punctuation shifted a
    // hyphen is not there to be found around
    txt::collator shifted{txt::options{.strength = txt::strength::primary,
                                       .punctuation = txt::punctuation::shifted}};
    ASSERT_TRUE(shifted.find(string("un re-sume ici"), string("resume")).has_value());
    EXPECT_EQ(shifted.find(string("un re-sume ici"), string("resume"))->size, 7u);
    EXPECT_FALSE(search.contains(string("un re-sume ici"), string("resume")));
    // and with the numeric order a number is found as a number
    txt::collator numeric{txt::options{.strength = txt::strength::primary, .numeric = true}};
    EXPECT_TRUE(numeric.contains(string("plik0042.txt"), string("42")));
    EXPECT_FALSE(numeric.contains(string("plik0042.txt"), string("4")));

    // a search at full strength is the same search with less counted as
    // equal
    txt::collator exact;
    EXPECT_FALSE(exact.contains(text, string("RESUME")));
    EXPECT_TRUE(exact.contains(text, string("résumé")));
    EXPECT_TRUE(exact.contains(text, string("résumé")));   // however it was written
}

TEST(Collate_Tests, ASearchPreparedOnceAndAskedMany) {
    // The text weighed once and asked many times, and every occurrence
    // as a range — the shapes search.h has for its two searches, here
    // over the collator's idea of equal
    txt::collator search{txt::options{.strength = txt::strength::primary}};
    string text = "Résumé, resume, RESUME: le résumé du candidat.";
    txt::collated_text weighed{search, text};

    EXPECT_EQ(weighed.text().size(), text.size());
    EXPECT_GT(weighed.size(), 0u);                      // elements, not bytes
    EXPECT_FALSE(weighed.empty());

    auto pattern = weighed.searcher(string("resume"));
    EXPECT_EQ(pattern.pattern(), string("resume"));
    EXPECT_EQ(pattern.size(), 6u);
    EXPECT_FALSE(pattern.empty());

    ASSERT_TRUE(weighed.find(pattern).has_value());
    EXPECT_EQ(weighed.find(pattern)->at, 0u);
    EXPECT_EQ(weighed.count(pattern), 4u);
    EXPECT_TRUE(weighed.contains(pattern));
    EXPECT_TRUE(weighed.starts_with(string("RÉSUMÉ")));
    EXPECT_TRUE(weighed.ends_with(string("candidat.")));

    // the same answers as the collator that weighs both on every call
    EXPECT_EQ(search.find(text, string("resume"))->at, 0u);
    EXPECT_TRUE(search.contains(text, string("RESUME")));
    EXPECT_TRUE(search.starts_with(text, string("résumé")));
    EXPECT_TRUE(search.ends_with(text, string("CANDIDAT.")));

    // every occurrence, the text weighed a single time for all of them,
    // each element the slice of the original text the match covers
    vector<size_t> at, size;
    for (auto m : txt::collated_matches(search, text, string("resume"))) {
        at.push_back(size_t(m.begin() - text.view().data()));
        size.push_back(m.size());
    }
    ASSERT_EQ(at.size(), 4u);
    EXPECT_EQ(at[0], 0u);
    EXPECT_EQ(size[0], 8u);                             // "Résumé" is eight bytes
    EXPECT_EQ(size[1], 6u);                             // "resume" is six
    EXPECT_EQ(txt::collated_matches(search, text, string("resume")).count(), 4u);
    EXPECT_FALSE(txt::collated_matches(search, text, string("resume")).empty());
    EXPECT_TRUE(txt::collated_matches(search, text, string("zebra")).empty());

    // the iterator answers where it is as well as what it found, and
    // outlives the range it came from, the weighed text being held by
    // the one tracked object both point at
    auto it = txt::collated_matches(search, text, string("resume")).begin();
    EXPECT_EQ(it.pos(), 0u);
    EXPECT_EQ(it.size(), 8u);
    ++it;
    EXPECT_EQ(it.pos(), 10u);                           // past "Résumé, "
    EXPECT_EQ(it.size(), 6u);

    // an empty pattern matches nowhere in a range, as it counts nowhere
    EXPECT_TRUE(txt::collated_matches(search, text, string()).empty());
    EXPECT_EQ(weighed.count(string()), 0u);
    EXPECT_TRUE(weighed.find(string()).has_value());    // but is found where it is looked for
    EXPECT_TRUE(weighed.starts_with(string()));
}

TEST(Collate_Tests, AMatchTakesWholeLetters) {
    // The fault the folded search had: a pattern may not match half of
    // what one letter weighs. A "ß" weighs as two s at primary
    // strength, which is the whole of what it is; "ss" finds it and "s"
    // does not find half of one.
    txt::collator search{txt::options{.strength = txt::strength::primary}};
    string strasse = "straße";
    EXPECT_TRUE(search.contains(strasse, string("ss")));
    EXPECT_EQ(search.find(strasse, string("ss"))->size, 2u);     // the two bytes of ß
    EXPECT_TRUE(search.contains(strasse, string("s")));          // the s of "stra"
    EXPECT_FALSE(search.contains(string("aß"), string("s")));    // but never half of a ß
    EXPECT_TRUE(search.contains(string("aß"), string("ss")));
    EXPECT_EQ(search.find(string("aß"), string("ss"))->at, 1u);
    EXPECT_TRUE(search.contains(string("sok"), string("s")));    // an s that is an s
    EXPECT_TRUE(search.contains(strasse, string("straße")));
    EXPECT_TRUE(search.contains(strasse, string("STRASSE")));
    EXPECT_TRUE(search.starts_with(strasse, string("stra")));
    EXPECT_FALSE(search.ends_with(strasse, string("s")));

    // and the same of a letter a language weighs as two: Danish reads
    // an "aa" as an "å", so an "a" is not half of one
    txt::collator danish{txt::locale(string("da")),
                         txt::options{.strength = txt::strength::primary}};
    EXPECT_TRUE(danish.contains(string("Aarhus"), string("aa")));
    EXPECT_TRUE(danish.contains(string("Aarhus"), string("å")));
    EXPECT_FALSE(danish.contains(string("Aarhus"), string("a")));
    EXPECT_TRUE(search.contains(string("Aarhus"), string("a")));  // the root has no such letter
}
// The same rule from the other side. collate.h reads it off the elements
// it has already gathered rather than off the code points, and the two
// answers must be one answer: "cafe" is not inside a "café" however that
// text is spelt, at any strength that looks at the accent at all.
TEST(Collate_Tests, TwoSpellingsOfOneTextAnswerTheSame) {
    struct { const char* composed; const char* apart; } letters[] = {
        {"café",       "café"},
        {"côte",       "côte"},
        {"ạb",         "ạb"},
        {"Łódź", "Łódź"},
    };
    const char* patterns[] = {"cafe", "cote", "a", "caf", "e", "o", "Łod"};
    txt::strength levels[] = {txt::strength::primary, txt::strength::secondary,
                              txt::strength::tertiary, txt::strength::quaternary};
    size_t checked = 0;
    for (auto level : levels) {
        txt::collator by{level};
        for (auto& l : letters) {
            string composed(l.composed), apart(l.apart);
            ASSERT_NE(composed, apart) << l.composed;
            for (auto* p : patterns) {
                string pattern(p);
                auto a = by.find(composed, pattern);
                auto b = by.find(apart, pattern);
                ASSERT_EQ(a.has_value(), b.has_value())
                    << l.composed << " against " << l.apart << ", pattern " << p
                    << " at strength " << int(level);
                EXPECT_EQ(by.contains(composed, pattern), by.contains(apart, pattern));
                EXPECT_EQ(by.starts_with(composed, pattern), by.starts_with(apart, pattern));
                EXPECT_EQ(by.ends_with(composed, pattern), by.ends_with(apart, pattern));
                // and what was matched is what this collator calls equal
                // to the pattern, which is what a search answers with
                if (a) {
                    string piece(composed.view().data() + a->at, a->size);
                    EXPECT_EQ(by.compare(piece, pattern), 0)
                        << l.composed << ", pattern " << p << " at strength " << int(level);
                }
                ++checked;
            }
        }
    }
    EXPECT_GT(checked, 100u);

    // The accent belongs to the match at the strength that does not look
    // at it and refuses the match at every strength that does — which is
    // the whole of the rule in four lines
    txt::collator primary{txt::strength::primary};
    txt::collator exact;
    for (auto* t : {"café", "café"}) {
        string text(t);
        ASSERT_TRUE(primary.find(text, string("cafe")).has_value()) << t;
        EXPECT_EQ(primary.find(text, string("cafe"))->size, text.size()) << t;
        EXPECT_FALSE(exact.find(text, string("cafe")).has_value()) << t;
    }

    // A refused match moves the scan on and does not end it. The first
    // "cafe" of each text is refused for the accent behind it and the
    // second is found; three refusals in a row is the same question asked
    // of a scan that has to survive more than one.
    for (auto* t : {"café cafe", "café cafe"}) {
        string text(t);
        auto hit = exact.find(text, string("cafe"));
        ASSERT_TRUE(hit.has_value()) << t;
        EXPECT_EQ(hit->at, text.size() - 4) << t;
        EXPECT_EQ(txt::collated_text(exact, text).count(txt::collated_searcher(exact, string("cafe"))), 1u) << t;
        EXPECT_TRUE(exact.ends_with(text, string("cafe"))) << t;
    }
    string many("cafécafécafécafe");
    auto hit = exact.find(many, string("cafe"));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->at, many.size() - 4);
    EXPECT_EQ(txt::collated_text(exact, many).count(txt::collated_searcher(exact, string("cafe"))), 1u);
}
// starts_with and ends_with answer by the rule find answers by, which is
// the one the header promises them. What stands at the end of a text is a
// question about the bytes; what a search by collation is asked is whether
// anything the collator looks at stands there, and with the punctuation
// shifted a hyphen does not. "-resume" contained "resume" and did not
// start with it.
TEST(Collate_Tests, TheEndsOfATextAreWhatTheCollatorLooksAt) {
    txt::collator shifted{txt::options{.strength = txt::strength::primary,
                                       .punctuation = txt::punctuation::shifted}};
    txt::collator counted{txt::options{.strength = txt::strength::primary}};
    struct { const char* text; bool starts, ends; } cases[] = {
        {"resume",    true,  true},
        {"-resume",   true,  true},
        {"resume-",   true,  true},
        {"-resume-",  true,  true},
        {"re-sume",   true,  true},
        {"--resume ", true,  true},
        {"xresume",   false, true},
        {"resumex",   true,  false},
        {"x resume",  false, true},
    };
    string pattern("resume");
    for (auto& c : cases) {
        string text(c.text);
        EXPECT_TRUE(shifted.contains(text, pattern)) << c.text;
        EXPECT_EQ(shifted.starts_with(text, pattern), c.starts) << c.text;
        EXPECT_EQ(shifted.ends_with(text, pattern), c.ends) << c.text;
    }

    // and where the punctuation is counted a hyphen is a character like
    // any other, so the answers go back to being about the text
    EXPECT_FALSE(counted.starts_with(string("-resume"), pattern));
    EXPECT_FALSE(counted.ends_with(string("resume-"), pattern));
    EXPECT_TRUE(counted.starts_with(string("resume-"), pattern));
    EXPECT_TRUE(counted.ends_with(string("-resume"), pattern));

    // An accent at the first strength is the same kind of thing: it is
    // not looked at, so it does not stand between the pattern and the end
    // of the text — and at a strength that does look at it there is no
    // match at either end to begin with
    txt::collator exact;
    for (auto* t : {"café", "café"}) {
        string text(t);
        EXPECT_TRUE(counted.starts_with(text, string("cafe"))) << t;
        EXPECT_TRUE(counted.ends_with(text, string("cafe"))) << t;
        EXPECT_FALSE(exact.starts_with(text, string("cafe"))) << t;
        EXPECT_FALSE(exact.ends_with(text, string("cafe"))) << t;
    }

    // whatever the road: the prepared text answers what the collator does
    txt::collated_text weighed{shifted, string("-resume-")};
    EXPECT_TRUE(weighed.starts_with(string("resume")));
    EXPECT_TRUE(weighed.ends_with(string("resume")));
    EXPECT_TRUE(weighed.starts_with(weighed.searcher(string("resume"))));
    EXPECT_TRUE(weighed.ends_with(weighed.searcher(string("resume"))));

    // an empty text, and a pattern longer than the text
    EXPECT_FALSE(shifted.starts_with(string(), pattern));
    EXPECT_FALSE(shifted.ends_with(string(), pattern));
    EXPECT_FALSE(shifted.ends_with(string("me"), pattern));
    EXPECT_TRUE(shifted.starts_with(string("resume"), string()));
    EXPECT_TRUE(shifted.ends_with(string("resume"), string()));
    // a text of nothing but what the collator passes over is not a text
    // that starts with the pattern
    EXPECT_FALSE(shifted.starts_with(string("---"), pattern));
    EXPECT_FALSE(shifted.ends_with(string("---"), pattern));
}
// ends_with no longer tries every position of the text; it counts the
// pattern's elements back from the end and tries the one place a match
// that ends at the end can begin. That is a piece of reasoning rather
// than a search, so it is worth a test of its own: the cases where the
// one place is not the obvious one.
TEST(Collate_Tests, EndsWithLooksWhereTheMatchWouldHaveToBegin) {
    txt::collator by;
    txt::collator primary{txt::strength::primary};

    // the pattern stands in the text but not at the end, which is the
    // answer a walk that stopped at the first match would get wrong
    EXPECT_FALSE(by.ends_with(string("kotaX"), string("kota")));
    EXPECT_FALSE(by.ends_with(string("kota kota X"), string("kota")));
    EXPECT_TRUE(by.ends_with(string("kota kota"), string("kota")));
    EXPECT_TRUE(by.ends_with(string("kotakota"), string("kota")));

    // a letter that weighs to more than one element, at the end and just
    // before it: the count back is over elements and not over characters
    EXPECT_TRUE(by.ends_with(string("straße"), string("straße")));
    EXPECT_TRUE(by.ends_with(string("straße"), string("e")));
    EXPECT_FALSE(by.ends_with(string("straße"), string("ß")));
    EXPECT_TRUE(by.ends_with(string("straß"), string("ß")));
    EXPECT_TRUE(primary.ends_with(string("straß"), string("ss")));
    EXPECT_FALSE(primary.ends_with(string("straß"), string("s")));

    // a contraction at the end, which is one letter and not two
    txt::collator czech{txt::locale(string("cs")), txt::options{.strength = txt::strength::primary}};
    EXPECT_TRUE(czech.ends_with(string("chyba ch"), string("ch")));
    EXPECT_FALSE(czech.ends_with(string("chyba ch"), string("h")));
    EXPECT_TRUE(czech.ends_with(string("chyba"), string("yba")));

    // a text with fewer elements than the pattern has
    EXPECT_FALSE(by.ends_with(string("ta"), string("kota")));
    EXPECT_FALSE(by.ends_with(string(), string("kota")));
    EXPECT_FALSE(primary.ends_with(string("---"), string("kota")));

    // and a long text, where the whole point is that the walk is the
    // pattern's length and not the text's: the answers must be the ones
    // a walk over the whole text gave
    string filler(4096, 'a');
    EXPECT_TRUE(by.ends_with(filler + string("zebra"), string("zebra")));
    EXPECT_FALSE(by.ends_with(filler + string("zebra") + filler, string("zebra")));
    EXPECT_TRUE(by.ends_with(filler, string("aaaa")));
    EXPECT_TRUE(by.ends_with(filler, filler));
    EXPECT_FALSE(by.ends_with(filler, filler + string("a")));

    // the prepared text answers the same, and so does the range's text
    txt::collated_text weighed{by, filler + string("zebra")};
    EXPECT_TRUE(weighed.ends_with(string("zebra")));
    EXPECT_TRUE(weighed.ends_with(weighed.searcher(string("zebra"))));
    EXPECT_FALSE(weighed.ends_with(string("zebr")));
}
// A searcher weighed by one collator and asked of a text weighed by
// another answered about neither: its elements had been filtered by what
// the one looks at and were compared by what the other calls equal. The
// text weighs the pattern again with its own, so the answer is the one a
// pattern of the text's own making would have given.
TEST(Collate_Tests, APatternWeighedByAnotherCollatorIsWeighedAgain) {
    txt::collator counted{txt::options{.strength = txt::strength::primary}};
    txt::collator shifted{txt::options{.strength = txt::strength::primary,
                                       .punctuation = txt::punctuation::shifted}};
    txt::collator exact;                                  // tertiary, counted
    ASSERT_FALSE(counted == shifted);
    ASSERT_FALSE(counted == exact);
    ASSERT_TRUE(counted == txt::collator{txt::options{.strength = txt::strength::primary}});

    // the pattern has a hyphen the shifted collator dropped; the text
    // counts hyphens, and once found "re-sume" inside "resume" that way
    string text("resume");
    txt::collated_text plain{counted, text};
    txt::collated_searcher theirs{shifted, string("re-sume")};
    EXPECT_FALSE(plain.find(theirs).has_value());
    EXPECT_FALSE(plain.contains(theirs));
    EXPECT_FALSE(plain.starts_with(theirs));
    EXPECT_FALSE(plain.ends_with(theirs));
    EXPECT_EQ(plain.count(theirs), 0u);
    EXPECT_EQ(txt::collated_matches(counted, text, theirs).count(), 0u);
    // which is what the text's own searcher answers
    EXPECT_FALSE(plain.find(plain.searcher(string("re-sume"))).has_value());
    // and what the shifted collator answers is the other thing
    EXPECT_TRUE(txt::collated_text(shifted, text).find(theirs).has_value());

    // the other way round: a searcher that counts, asked of a text that
    // shifts, must answer what the shifting text answers
    txt::collated_text loose{shifted, string("re-sume")};
    txt::collated_searcher strict{counted, string("resume")};
    EXPECT_TRUE(loose.find(strict).has_value());
    EXPECT_EQ(loose.find(strict)->size, 7u);          // the hyphen is inside the match
    EXPECT_TRUE(loose.starts_with(strict));
    EXPECT_TRUE(loose.ends_with(strict));
    EXPECT_EQ(loose.count(strict), 1u);
    EXPECT_EQ(txt::collated_matches(shifted, string("re-sume"), strict).count(), 1u);

    // a difference of strength is a difference of collator too
    txt::collated_text weak{counted, string("résumé")};
    EXPECT_TRUE(weak.find(txt::collated_searcher{exact, string("resume")}).has_value());
    EXPECT_FALSE(txt::collated_text(exact, string("résumé"))
                     .find(txt::collated_searcher{counted, string("resume")}).has_value());

    // and where the two agree nothing is weighed twice: the same
    // searcher, used against a text made with an equal collator that is
    // not the same object, answers as its own would
    txt::collator twin{txt::options{.strength = txt::strength::primary}};
    txt::collated_text other{twin, text};
    EXPECT_TRUE(other.find(txt::collated_searcher{counted, string("resume")}).has_value());

    // a language is part of it: the root has no Czech "ch"
    txt::collator czech{txt::locale(string("cs")), txt::options{.strength = txt::strength::primary}};
    ASSERT_FALSE(czech == counted);
    txt::collated_text root_text{counted, string("chyba")};
    EXPECT_TRUE(root_text.find(txt::collated_searcher{czech, string("c")}).has_value());
    EXPECT_FALSE(txt::collated_text(czech, string("chyba"))
                     .find(txt::collated_searcher{counted, string("c")}).has_value());
}
// The buffer the elements of a text and of a pattern are held in leaves
// its inline part uninitialised past what has been written, which is what
// makes it cost a stack frame and nothing else. The compiler's own copy
// took all of it, two kilobytes to carry a handful, and a prepared
// searcher is copied into a managed object every time a range is built —
// where the collector reads those bytes as words and a stale address
// among them is a root that keeps something dead alive. The copy now
// takes what has been written, and the place it can go wrong is the
// boundary between the inline part and the one behind it.
TEST(Collate_Tests, APreparedPatternIsCopiedByWhatIsInIt) {
    txt::collator by{txt::strength::primary};

    // a pattern longer than the inline part holds: the copy has to cross
    // the boundary and take both halves
    for (size_t n : {size_t(1), size_t(255), size_t(256), size_t(257), size_t(600)}) {
        std::string s(n, 'a');
        string pattern(s.data(), s.size());
        txt::collated_searcher made{by, pattern};
        ASSERT_EQ(made.size(), n) << n;

        txt::collated_searcher copied = made;                  // the copy
        EXPECT_EQ(copied.size(), n) << n;
        EXPECT_EQ(copied.pattern(), pattern) << n;

        txt::collated_searcher assigned{by, string("x")};
        assigned = made;                                       // and the assignment
        EXPECT_EQ(assigned.size(), n) << n;

        txt::collated_searcher moved = std::move(made);
        EXPECT_EQ(moved.size(), n) << n;

        // and all three answer what the original answered
        string text(s.data(), s.size());
        txt::collated_text weighed{by, text};
        ASSERT_TRUE(weighed.find(copied).has_value()) << n;
        EXPECT_EQ(weighed.find(copied)->at, 0u) << n;
        EXPECT_EQ(weighed.find(copied)->size, n) << n;
        EXPECT_EQ(weighed.find(assigned)->size, n) << n;
        EXPECT_EQ(weighed.find(moved)->size, n) << n;
        EXPECT_EQ(weighed.count(copied), 1u) << n;
        EXPECT_TRUE(weighed.starts_with(copied)) << n;
        EXPECT_TRUE(weighed.ends_with(copied)) << n;
        // a text of two of them holds two of it
        EXPECT_EQ(txt::collated_text(by, text + text).count(copied), 2u) << n;
    }

    // and a range, which is where such a copy lands inside a managed
    // object: what it kept is let go of when it is
    string text("Ala ma kota, kota ma Ala");
    auto before = collector::get_live_object_count();
    off_frame([&] {
        auto pattern = txt::collated_searcher{by, string("ala")};
        size_t n = 0;
        for (auto m : txt::collated_matches(by, text, pattern)) {
            (void)m;
            ++n;
        }
        EXPECT_EQ(n, 2u);
    });
    collector::clear_stack();
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_LE(collector::get_live_object_count(), before);
}
// A range holds its text inside a managed object, and a slice is two raw
// pointers into a managed buffer beside the owner that keeps it alive. A
// raw pointer from one managed object into another is the one thing the
// collector does not read as data, and it said so on stderr in every
// build without NDEBUG. The text is a string now and the slice is made
// where it is asked for, which is what this checks has not cost anything.
TEST(Collate_Tests, ARangeHoldsItsTextAsAString) {
    txt::collator by{txt::strength::primary};
    string text("Résumé, resume, RESUME");

    // the matches are slices of the text and say where they are
    std::vector<std::pair<size_t, size_t>> spans;
    std::vector<std::string> found;
    for (auto it = txt::collated_matches(by, text, string("resume")).begin(),
              e = txt::collated_matches(by, text, string("resume")).end(); it != e; ++it) {
        spans.push_back({it.pos(), it.size()});
        found.push_back(std::string((*it).data(), (*it).size()));
    }
    ASSERT_EQ(spans.size(), 3u);
    EXPECT_EQ(spans[0].first, 0u);
    EXPECT_EQ(found[0], "Résumé");
    EXPECT_EQ(found[1], "resume");
    EXPECT_EQ(found[2], "RESUME");
    EXPECT_EQ(txt::collated_matches(by, text, string("resume")).text().size(), text.size());

    // a range built over a piece of a text keeps that piece, so the
    // positions are the piece's own and the text it came from need not
    // outlive it
    size_t n = 0;
    std::string piece;
    off_frame([&] {
        auto range = txt::collated_matches(by, string("xx resume yy").as_slice(3, 6),
                                           txt::collated_searcher(by, string("resume")));
        for (auto m : range) {
            ++n;
            piece.assign(m.data(), m.size());
        }
        EXPECT_EQ(range.text().size(), 6u);
        EXPECT_EQ(range.begin().pos(), 0u);         // of the piece, not of the text
    });
    EXPECT_EQ(n, 1u);
    EXPECT_EQ(piece, "resume");

    // and an iterator still outlives the range it came from, which is
    // what the tracked state is for
    auto it = txt::collated_matches(by, text, string("resume")).begin();
    collector::force_collect(true);
    EXPECT_EQ(it.pos(), 0u);
    EXPECT_EQ(std::string((*it).data(), (*it).size()), "Résumé");
}
// The same character the page carried as a limit, asked of the collated
// search. Here the answer depends on the strength, and both answers are
// right: at the first strength the marks are not looked at, so the base is
// an occurrence of the character and the match is the whole of it — the
// same reason "resume" finds "résumé" — and at every strength that does
// look at them there is no match at all. What must never happen is a match
// of part of the character, and that is what the size assertions are for.
TEST(Collate_Tests, ADecompositionTheOrderingPullsApart) {
    struct { const char* text; const char* base; const char* whole; } cases[] = {
        {"ḍ̇", "d", "ḍ̇"},
        {"ḍ̇", "d", "ḍ̇"},
        {"ḍ̇", "d", "ḍ̇"},
        {"ḍ̇", "d", "ḍ̇"},
        {"ḉ", "c", "ḉ"},
        {"ḉ", "c", "ḉ"},
        {"ṝ", "r", "ṝ"},
        {"ạ̧́", "a", "ạ̧́"},
    };
    txt::collator primary{txt::strength::primary};
    txt::strength above[] = {txt::strength::secondary, txt::strength::tertiary,
                             txt::strength::quaternary};
    for (auto& c : cases) {
        string text(c.text), base(c.base), whole(c.whole);
        // the first strength: the base is the character, and the match is
        // the whole of it rather than the bytes of the base
        auto hit = primary.find(text, base);
        ASSERT_TRUE(hit.has_value()) << c.text;
        EXPECT_EQ(hit->at, 0u) << c.text;
        EXPECT_EQ(hit->size, text.size()) << c.text;   // whole, never part
        // every strength above it: no match of the base at all
        for (auto level : above) {
            txt::collator by{level};
            EXPECT_FALSE(by.find(text, base).has_value())
                << c.text << " at strength " << int(level);
            EXPECT_FALSE(by.contains(text, base)) << c.text;
            EXPECT_FALSE(by.starts_with(text, base)) << c.text;
            EXPECT_FALSE(by.ends_with(text, base)) << c.text;
            EXPECT_EQ(txt::collated_text(by, text).count(txt::collated_searcher(by, base)), 0u)
                << c.text;
        }
        // and the whole character is found at every strength, however
        // either side spells it
        for (auto level : {txt::strength::primary, txt::strength::secondary,
                           txt::strength::tertiary}) {
            txt::collator by{level};
            for (auto& p : {whole, text}) {
                auto found = by.find(text, p);
                ASSERT_TRUE(found.has_value()) << c.text << " at strength " << int(level);
                EXPECT_EQ(found->at, 0u) << c.text;
                EXPECT_EQ(found->size, text.size()) << c.text;
                // and what came back is equal to what was asked for
                string piece(text.view().data() + found->at, found->size);
                EXPECT_EQ(by.compare(piece, p), 0) << c.text;
            }
        }
    }

    // the base in front of another letter is found where it stands alone,
    // so the rule refuses what it should and nothing else
    txt::collator exact;
    string run("ḍ̇ d");
    EXPECT_TRUE(exact.contains(run, string("d")));
    EXPECT_EQ(exact.find(run, string("d"))->at, 6u);       // the bare one, not the marked one
    EXPECT_EQ(txt::collated_text(exact, run).count(txt::collated_searcher(exact, string("d"))), 1u);
}








TEST(Collate_Tests, ThePositionsOfTheElementsAscend) {
    // What the bisection that finds where a search starts rests on. Here
    // an element carries the bytes of the combining sequence it came
    // from, and the ordering moves a mark inside a sequence and never out
    // of one; search.h reaches the same by leaving its positions where
    // they were written and moving only the marks.
    // Asked of texts made to be awkward: marks out of canonical order,
    // a contraction reached across a mark, Hangul, Tibetan.
    const char* texts[] = {
        "á̧b", "á̧b", "q̣̇u", "é́e",
        "ཱིᴖ5ཱྀx", "한국어", "café café", "Ala ma kota",
        "ḍ̇ ḍ̇ zebra", "ch chyba čz",
        "0̉1́ 2", "ré-sùme",
    };
    txt::locale languages[] = {txt::locale(), txt::locale(string("cs")),
                               txt::locale(string("da"))};
    size_t checked = 0;
    for (auto where : languages) {
        for (int bits = 0; bits < 8; ++bits) {
            txt::options how;
            how.strength = txt::strength(bits & 3);
            how.punctuation = (bits & 4) ? txt::punctuation::shifted : txt::punctuation::counted;
            how.numeric = (bits & 4) != 0;
            txt::collator collator{where, how};
            for (auto* t : texts) {
                string text(t);
                txt::collated_text weighed{collator, text};
                size_t last = 0;
                for (size_t i = 0; i < weighed.size(); ++i) {
                    size_t at = weighed.at(i);
                    ASSERT_GE(at, last) << t << " at element " << i;
                    ASSERT_LE(at, text.size());
                    last = at;
                    ++checked;
                }
                // and what the bisection finds is what a walk finds:
                // every position of the text asked of one pattern
                for (size_t from = 0; from <= text.size(); ++from) {
                    auto hit = weighed.find(string("a"), from);
                    ASSERT_TRUE(!hit || hit->at >= from) << t << " from " << from;
                }
            }
        }
    }
    EXPECT_GT(checked, 2000u);
}
