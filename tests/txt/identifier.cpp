//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::identifier: UAX #31, NFKC_Casefold and UTS #39. Three of the four
// oracles are whole files — XID_Start and XID_Continue over every one of
// the 1114112 code points (from Python's own tables, which the generator
// has already checked against DerivedCoreProperties.txt), the NFKC_CF
// mapping of DerivedNormalizationProps.txt entry by entry, and the
// confusables, Identifier_Status and Identifier_Type of UTS #39 whole.
// The fourth is by hand: the attacks people actually write.
#include "sgcl/txt/identifier.h"
#include "tests/types.h"
#include "tests/txt/identifier_tests.h"

#include <random>
#include <string>

namespace {
    // A call to a deleted function in a requires-expression outside a
    // template is a hard error, so the refusal is seen through a concept
    template<class T> concept Starts = requires(T c) { txt::is_identifier_start(c); };
    template<class T> concept Continues = requires(T c) { txt::is_identifier_continue(c); };
    template<class T> concept Statuses = requires(T c) { txt::identifier_status_of(c); };

    string utf8_of(char32_t c) {
        char buf[utf8::max_width];
        return string(buf, utf8::encode(c, buf));
    }

    string utf8_of(const char32_t* points) {
        std::string out;
        char buf[utf8::max_width];
        for (size_t i = 0; points[i]; ++i) {
            out.append(buf, utf8::encode(points[i], buf));
        }
        return string(out.data(), out.size());
    }

    // The two generated tables are sorted by their first field, so both
    // are searched rather than walked
    bool in_ranges(char32_t c, const ucd::Range* table, size_t n) {
        size_t lo = 0, hi = n;
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (c < table[mid].lo) {
                hi = mid;
            } else if (c > table[mid].hi) {
                lo = mid + 1;
            } else {
                return true;
            }
        }
        return false;
    }

    const char32_t* mapped(char32_t c, const ucd::Mapping* table, size_t n) {
        size_t lo = 0, hi = n;
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (c < table[mid].cp) {
                hi = mid;
            } else if (c > table[mid].cp) {
                lo = mid + 1;
            } else {
                return table[mid].to;
            }
        }
        return nullptr;
    }

    template<class T, size_t N>
    constexpr size_t count(const T (&)[N]) {
        return N;
    }

    bool is_surrogate(char32_t c) {
        return c >= 0xD800 && c <= 0xDFFF;
    }

    // The skeleton of UTS #39 written a second time, out of the table of
    // the specification and the normalization the module is already held
    // to: NFD, the prototype of each code point, NFD again
    string reference_skeleton(const string& text) {
        std::string out;
        char buf[utf8::max_width];
        auto decomposed = txt::normalize(text, txt::nfd);
        for (auto c : decomposed.runes()) {
            if (auto to = mapped(c, ucd::Confusables, count(ucd::Confusables))) {
                for (size_t i = 0; to[i]; ++i) {
                    out.append(buf, utf8::encode(to[i], buf));
                }
            } else {
                out.append(buf, utf8::encode(c, buf));
            }
        }
        return txt::normalize(string(out.data(), out.size()), txt::nfd);
    }
}

//------------------------------------------------------------------------------
// UAX #31
//------------------------------------------------------------------------------
TEST(Identifier_Tests, XidStartOverEveryCodePoint) {
    size_t held = 0;
    for (char32_t c = 0; c < 0x110000; ++c) {
        bool want = in_ranges(c, ucd::XidStart, count(ucd::XidStart));
        ASSERT_EQ(txt::is_identifier_start(c), want) << "U+" << std::hex << uint32_t(c);
        held += want;
    }
    EXPECT_EQ(held, 141246u);

    // and the profile adds exactly two code points to it
    for (char32_t c = 0; c < 0x110000; ++c) {
        bool want = in_ranges(c, ucd::XidStart, count(ucd::XidStart)) || c == U'_' || c == U'$';
        ASSERT_EQ(txt::is_identifier_start(c, txt::program_syntax), want) << "U+" << std::hex << uint32_t(c);
    }
}

TEST(Identifier_Tests, XidContinueOverEveryCodePoint) {
    size_t held = 0;
    for (char32_t c = 0; c < 0x110000; ++c) {
        bool want = in_ranges(c, ucd::XidContinue, count(ucd::XidContinue));
        ASSERT_EQ(txt::is_identifier_continue(c), want) << "U+" << std::hex << uint32_t(c);
        held += want;
    }
    EXPECT_EQ(held, 144522u);

    // the underscore is a continue already; the dollar is what is added
    for (char32_t c = 0; c < 0x110000; ++c) {
        bool want = in_ranges(c, ucd::XidContinue, count(ucd::XidContinue)) || c == U'$';
        ASSERT_EQ(txt::is_identifier_continue(c, txt::program_syntax), want) << "U+" << std::hex << uint32_t(c);
    }
    EXPECT_TRUE(txt::is_identifier_continue(U'_'));
    EXPECT_FALSE(txt::is_identifier_start(U'_'));
}

TEST(Identifier_Tests, TheAnswersAreKnownWhereTheProgramIsCompiled) {
    static_assert(txt::is_identifier_start(U'a') && txt::is_identifier_start(U'Z'));
    static_assert(!txt::is_identifier_start(U'0') && !txt::is_identifier_start(U'_'));
    static_assert(txt::is_identifier_start(U'_', txt::program_syntax));
    static_assert(txt::is_identifier_start(U'$', txt::program_syntax));
    static_assert(txt::is_identifier_continue(U'0') && txt::is_identifier_continue(U'_'));
    static_assert(!txt::is_identifier_continue(U'$') && txt::is_identifier_continue(U'$', txt::program_syntax));
    static_assert(txt::is_identifier_start(U'ż') && txt::is_identifier_start(U'漢'));
    static_assert(!txt::is_identifier_start(U' ') && !txt::is_identifier_start(U'-'));

    // The code points the X forms are for: ID_Start has them, XID_Start
    // does not, and each normalizes to something that is not an
    // identifier where it stands — a space and an iota, a space and a
    // sound mark, a mark and a vowel
    static_assert(!txt::is_identifier_start(char32_t(0x037A)));
    static_assert(!txt::is_identifier_start(char32_t(0x309B)));
    static_assert(!txt::is_identifier_start(char32_t(0x0E33)));
    static_assert(!txt::is_identifier_continue(char32_t(0x037A)));
    for (auto c : {char32_t(0x037A), char32_t(0x309B), char32_t(0x0E33)}) {
        auto normalized = txt::normalize(utf8_of(c), txt::nfkc);
        EXPECT_FALSE(txt::is_identifier(normalized)) << std::hex << uint32_t(c);
    }

    // whatever XID_Start says of a code point, NFKC of it is still made
    // of code points an identifier may be made of: that is the closure
    // the X buys, and it is what lets a compiler normalize a name
    for (char32_t c = 0; c < 0x110000; ++c) {
        if (is_surrogate(c) || !txt::is_identifier_continue(c)) {
            continue;
        }
        auto folded = txt::normalize(utf8_of(c), txt::nfkc);
        for (auto x : folded.runes()) {
            ASSERT_TRUE(txt::is_identifier_continue(x)) << "U+" << std::hex << uint32_t(c);
        }
    }
}

TEST(Identifier_Tests, TheNamesRefuseACharAndAnInt) {
    static_assert(Starts<char32_t> && !Starts<char> && !Starts<int> && !Starts<unsigned>);
    static_assert(Continues<char32_t> && !Continues<char> && !Continues<int>);
    static_assert(Statuses<char32_t> && !Statuses<char> && !Statuses<int>);

    // and each is still an object, so it passes where a predicate is asked
    string name("wartość_2");
    EXPECT_EQ(name.runes().count_of(txt::is_identifier_continue), 9u);
}

TEST(Identifier_Tests, WholeIdentifiersFollowRuleR1) {
    EXPECT_TRUE(txt::is_identifier(string("name")));
    EXPECT_TRUE(txt::is_identifier(string("nazwa2")));
    EXPECT_TRUE(txt::is_identifier(string("wartość")));
    EXPECT_TRUE(txt::is_identifier(string("переменная")));
    EXPECT_TRUE(txt::is_identifier(string("変数")));
    EXPECT_TRUE(txt::is_identifier(string("café")));
    EXPECT_FALSE(txt::is_identifier(string("")));
    EXPECT_FALSE(txt::is_identifier(string("2name")));
    EXPECT_FALSE(txt::is_identifier(string("_name")));          // not without the profile
    EXPECT_FALSE(txt::is_identifier(string("na me")));
    EXPECT_FALSE(txt::is_identifier(string("na-me")));
    EXPECT_FALSE(txt::is_identifier(string("name!")));
    EXPECT_FALSE(txt::is_identifier(string("\xFF\xFE")));       // not even text

    // a mark may continue a name and may not begin one
    EXPECT_TRUE(txt::is_identifier(txt::normalize(string("café"), txt::nfd)));
    EXPECT_FALSE(txt::is_identifier(utf8_of(char32_t(0x0301))));
}

TEST(Identifier_Tests, TheJoinersOfRuleR1a) {
    // क् + ZWJ + ष: a virama before, which is where a joiner belongs
    const char32_t with_zwj[] = {0x0915, 0x094D, 0x200D, 0x0937, 0};
    const char32_t with_zwnj[] = {0x0915, 0x094D, 0x200C, 0x0937, 0};
    EXPECT_TRUE(txt::is_identifier(utf8_of(with_zwj)));
    EXPECT_TRUE(txt::is_identifier(utf8_of(with_zwnj)));

    // meem + ZWNJ + dal: no virama, but the non-joiner breaks a join
    // that would otherwise happen, which is the other half of the rule
    const char32_t arabic[] = {0x0645, 0x200C, 0x062F, 0};
    EXPECT_TRUE(txt::is_identifier(utf8_of(arabic)));

    // and a joiner that does neither is not allowed anywhere
    const char32_t latin_zwj[] = {U'a', 0x200D, U'b', 0};
    const char32_t latin_zwnj[] = {U'a', 0x200C, U'b', 0};
    const char32_t leading[] = {0x200D, U'a', 0};
    const char32_t trailing[] = {0x0645, 0x200C, 0};
    const char32_t zwj_alone[] = {0x0645, 0x200D, 0x062F, 0};
    EXPECT_FALSE(txt::is_identifier(utf8_of(latin_zwj)));
    EXPECT_FALSE(txt::is_identifier(utf8_of(latin_zwnj)));
    EXPECT_FALSE(txt::is_identifier(utf8_of(leading)));
    EXPECT_FALSE(txt::is_identifier(utf8_of(trailing)));
    EXPECT_FALSE(txt::is_identifier(utf8_of(zwj_alone)));       // a joiner needs a virama, a non-joiner does not
}

TEST(Identifier_Tests, TheProgrammersProfile) {
    EXPECT_TRUE(txt::is_identifier(string("_name"), txt::program_syntax));
    EXPECT_TRUE(txt::is_identifier(string("$name"), txt::program_syntax));
    EXPECT_TRUE(txt::is_identifier(string("__init__"), txt::program_syntax));
    EXPECT_TRUE(txt::is_identifier(string("a$b_c2"), txt::program_syntax));
    EXPECT_TRUE(txt::is_identifier(string("_"), txt::program_syntax));
    EXPECT_FALSE(txt::is_identifier(string("2a"), txt::program_syntax));
    EXPECT_FALSE(txt::is_identifier(string("a-b"), txt::program_syntax));
    EXPECT_FALSE(txt::is_identifier(string(""), txt::program_syntax));

    // Python's own rule is XID_Start with the underscore and XID_Continue,
    // so every name it accepts this one accepts with the profile
    for (auto name : {"x", "_x", "x1", "ż", "_ż1", "變數"}) {
        EXPECT_TRUE(txt::is_identifier(string(name), txt::program_syntax)) << name;
    }
}

//------------------------------------------------------------------------------
// NFKC_Casefold
//------------------------------------------------------------------------------
TEST(Identifier_Tests, NfkcCasefoldOverEveryCodePoint) {
    size_t changed = 0;
    for (char32_t c = 0; c < 0x110000; ++c) {
        if (is_surrogate(c)) {
            continue;
        }
        auto to = mapped(c, ucd::NfkcCasefold, count(ucd::NfkcCasefold));
        string want = to ? utf8_of(to) : utf8_of(c);
        string one = utf8_of(c);
        ASSERT_EQ(txt::nfkc_casefold(one), want) << "U+" << std::hex << uint32_t(c);
        ASSERT_EQ(txt::is_nfkc_casefolded(one), one == want) << "U+" << std::hex << uint32_t(c);
        changed += to != nullptr;
    }
    EXPECT_EQ(changed, count(ucd::NfkcCasefold));
}

TEST(Identifier_Tests, NfkcCasefoldOverText) {
    EXPECT_EQ(txt::nfkc_casefold(string("ABC")), string("abc"));
    EXPECT_EQ(txt::nfkc_casefold(string("ＦＵＬＬ")), string("full"));   // the fullwidth letters
    EXPECT_EQ(txt::nfkc_casefold(string("ﬁle")), string("file"));        // the ligature
    EXPECT_EQ(txt::nfkc_casefold(string("Straße")), string("strasse"));
    EXPECT_EQ(txt::nfkc_casefold(string("①②③")), string("123"));
    EXPECT_EQ(txt::nfkc_casefold(txt::normalize(string("café"), txt::nfd)), string("café"));

    // a soft hyphen is a default ignorable code point and the mapping
    // takes it out, which is the half of NFKC_CF that is not a folding
    const char32_t hidden[] = {U'p', U'a', U'y', 0x00AD, U'p', U'a', U'l', 0};
    EXPECT_EQ(txt::nfkc_casefold(utf8_of(hidden)), string("paypal"));

    // U+0345 is the one code point whose order must not be settled
    // before it is folded: it has combining class 240 and folds to an
    // iota, which has none
    const char32_t ypogegrammeni[] = {U'a', 0x0345, 0x035D, U'b', 0};
    auto folded = txt::nfkc_casefold(utf8_of(ypogegrammeni));
    const char32_t expected[] = {U'a', 0x03B9, 0x035D, U'b', 0};
    EXPECT_EQ(folded, utf8_of(expected));

    // the form is a fixed point of itself, and a text already in it
    // comes back as the very same object
    string already("paypal");
    EXPECT_TRUE(txt::is_nfkc_casefolded(already));
    EXPECT_EQ(txt::nfkc_casefold(already).data(), already.data());
    EXPECT_EQ(txt::nfkc_casefold(folded), folded);
}

TEST(Identifier_Tests, IsNfkcCasefoldedAgreesWithTheFolding) {
    for (auto text : {"abc", "ABC", "café", "ＦＵＬＬ", "ﬁle", "straße", "Straße",
                      "перевод", "変数", "á", "á", "", "x­y"}) {
        string s(text);
        EXPECT_EQ(txt::is_nfkc_casefolded(s), txt::nfkc_casefold(s) == s) << text;
    }
}

//------------------------------------------------------------------------------
// UTS #39
//------------------------------------------------------------------------------
TEST(Identifier_Tests, IdentifierStatusOverEveryCodePoint) {
    size_t allowed = 0;
    for (char32_t c = 0; c < 0x110000; ++c) {
        bool want = in_ranges(c, ucd::IdentifierAllowed, count(ucd::IdentifierAllowed));
        ASSERT_EQ(txt::identifier_status_of(c) == txt::identifier_status::allowed, want)
            << "U+" << std::hex << uint32_t(c);
        allowed += want;
    }
    EXPECT_EQ(allowed, 112778u);
}

TEST(Identifier_Tests, IdentifierTypeOverEveryCodePoint) {
    size_t at = 0;
    for (char32_t c = 0; c < 0x110000; ++c) {
        while (at < count(ucd::IdentifierTypes) && ucd::IdentifierTypes[at].hi < c) {
            ++at;
        }
        uint16_t want = 0;
        if (at < count(ucd::IdentifierTypes) && ucd::IdentifierTypes[at].lo <= c) {
            want = ucd::IdentifierTypes[at].types;
        }
        ASSERT_EQ(uint16_t(txt::identifier_type_of(c)), want) << "U+" << std::hex << uint32_t(c);
    }

    using enum txt::identifier_type;
    // Table 1 of the specification: allowed is recommended or inclusion
    for (char32_t c = 0; c < 0x110000; ++c) {
        bool want = (txt::identifier_type_of(c) & (recommended | inclusion)) != not_character;
        ASSERT_EQ(txt::identifier_status_of(c) == txt::identifier_status::allowed, want)
            << "U+" << std::hex << uint32_t(c);
    }
    EXPECT_EQ(txt::identifier_type_of(U'a'), recommended);
    EXPECT_EQ(txt::identifier_type_of(char32_t(0x0378)), not_character);      // a hole in the Greek block
    EXPECT_NE(txt::identifier_type_of(char32_t(0xFF21)) & not_nfkc, not_character);
}

TEST(Identifier_Tests, TheSkeletonOfEveryConfusable) {
    for (size_t i = 0; i < count(ucd::Confusables); ++i) {
        auto c = ucd::Confusables[i].cp;
        if (is_surrogate(c)) {
            continue;
        }
        string one = utf8_of(c);
        ASSERT_EQ(txt::skeleton(one), reference_skeleton(one)) << "U+" << std::hex << uint32_t(c);
    }

    // and over every code point, the table or no table
    for (char32_t c = 0; c < 0x110000; ++c) {
        if (is_surrogate(c)) {
            continue;
        }
        string one = utf8_of(c);
        ASSERT_EQ(txt::skeleton(one), reference_skeleton(one)) << "U+" << std::hex << uint32_t(c);
    }
}

TEST(Identifier_Tests, RealAttacksOnNames) {
    // "раypal" with a Cyrillic а and р, the one everybody has seen
    EXPECT_TRUE(txt::is_confusable(string("раypal"), string("paypal")));
    // a Greek omicron for the Latin o
    EXPECT_TRUE(txt::is_confusable(string("goοgle"), string("google")));
    // a digit one for an l, and a capital i for it too
    EXPECT_TRUE(txt::is_confusable(string("paypa1"), string("paypal")));
    EXPECT_TRUE(txt::is_confusable(string("paypaI"), string("paypal")));
    // the Roman numeral one, which is a letter of its own
    EXPECT_TRUE(txt::is_confusable(string("ⅼ"), string("l")));
    // and a name that is merely similar is not the same name written
    // differently, which is the limit of what the table knows
    EXPECT_FALSE(txt::is_confusable(string("paypal-inc"), string("paypal")));
    EXPECT_FALSE(txt::is_confusable(string("paypaI"), string("paypai")));
    EXPECT_TRUE(txt::is_confusable(string("paypal"), string("paypal")));

    // a skeleton is not text and is not for showing: the Cyrillic name
    // and the Latin one come to the same bytes
    EXPECT_EQ(txt::skeleton(string("раypal")), txt::skeleton(string("paypal")));
    EXPECT_NE(string("раypal"), string("paypal"));
}

TEST(Identifier_Tests, MixedScriptsAndTheRestrictionLevels) {
    using enum txt::restriction_level;
    EXPECT_EQ(txt::restriction_level_of(string("paypal")), ascii_only);
    EXPECT_EQ(txt::restriction_level_of(string("wartość")), single_script);
    EXPECT_EQ(txt::restriction_level_of(string("переменная")), single_script);
    EXPECT_EQ(txt::restriction_level_of(string("name2_x")), ascii_only);
    // the digits and the punctuation are Common and make no second script
    EXPECT_EQ(txt::restriction_level_of(string("ważne2")), single_script);
    // Japanese: Han, Hiragana, Katakana and Latin are one writing system
    EXPECT_EQ(txt::restriction_level_of(string("変数のカタカナ")), highly_restrictive);
    EXPECT_EQ(txt::restriction_level_of(string("한글x")), highly_restrictive);
    // Latin beside one other recommended script, and not one of the three
    EXPECT_EQ(txt::restriction_level_of(string("xم")), moderately_restrictive);
    EXPECT_EQ(txt::restriction_level_of(string("xא")), moderately_restrictive);
    // and the three whose letters Latin is mistaken for stop a rung below
    EXPECT_EQ(txt::restriction_level_of(string("xр")), minimally_restrictive);
    EXPECT_EQ(txt::restriction_level_of(string("xλ")), minimally_restrictive);
    // a code point nobody allows in a name at all
    EXPECT_EQ(txt::restriction_level_of(string("na me")), unrestricted);
    EXPECT_EQ(txt::restriction_level_of(string("")), unrestricted);

    EXPECT_TRUE(txt::is_single_script(string("paypal")));
    EXPECT_FALSE(txt::is_single_script(string("раypal")));   // half Cyrillic, half Latin
    EXPECT_TRUE(txt::is_single_script(string("2026-09-23")));
    EXPECT_TRUE(txt::is_highly_restrictive(string("変数")));
    EXPECT_FALSE(txt::is_highly_restrictive(string("xр")));
    EXPECT_TRUE(txt::is_moderately_restrictive(string("xم")));
    EXPECT_FALSE(txt::is_moderately_restrictive(string("xλ")));

    EXPECT_TRUE(txt::is_allowed_identifier(string("wartość")));
    EXPECT_FALSE(txt::is_allowed_identifier(string("na me")));
    EXPECT_FALSE(txt::is_allowed_identifier(string("")));
}

// The scripts of a text are counted into eight words of stack, and a
// ninth distinct script stops the walk rather than being dropped. What
// the two callers do with that is the point of this test: nine scripts
// is not one script and is not any of the three sets of Table 2, so
// every rung below the last one is already settled and nothing is
// truncated — the answers must be the same as if the set were whole.
TEST(Identifier_Tests, MoreScriptsThanTheStackHolds) {
    using enum txt::restriction_level;
    // one letter of each, all of them Recommended and so Allowed
    string two("aа");                                        // Latin, Cyrillic
    string four("aаαא");                                     // and Greek, Hebrew
    string eight("aаαאمաაก");                                // and Arabic, Armenian, Georgian, Thai
    string nine("aаαאمաაกअ");                                // and Devanagari: one past the room
    string twelve("aаαאمաაกअ한あ漢அ");                       // well past it

    EXPECT_FALSE(txt::is_single_script(two));
    EXPECT_FALSE(txt::is_single_script(eight));
    EXPECT_FALSE(txt::is_single_script(nine));
    EXPECT_FALSE(txt::is_single_script(twelve));

    // every code point is allowed, so the scripts are what decides, and
    // past the room the answer is the rung where they mix freely
    EXPECT_TRUE(txt::is_allowed_identifier(twelve));
    EXPECT_EQ(txt::restriction_level_of(four), minimally_restrictive);
    EXPECT_EQ(txt::restriction_level_of(eight), minimally_restrictive);
    EXPECT_EQ(txt::restriction_level_of(nine), minimally_restrictive);
    EXPECT_EQ(txt::restriction_level_of(twelve), minimally_restrictive);
    EXPECT_FALSE(txt::is_highly_restrictive(nine));
    EXPECT_FALSE(txt::is_moderately_restrictive(twelve));

    // and a text that overflows the room with a code point nobody
    // allows in it is still unrestricted: the status is asked over the
    // whole text before the scripts are counted at all
    EXPECT_EQ(txt::restriction_level_of(twelve + string(" ")), unrestricted);

    // the room is not a limit on the text: a long text of two scripts
    // answers what two scripts answer, however long it is
    std::string many;
    for (int i = 0; i < 500; ++i) {
        many += "aа";
    }
    string long_two(many.data(), many.size());
    EXPECT_FALSE(txt::is_single_script(long_two));
    EXPECT_EQ(txt::restriction_level_of(long_two), minimally_restrictive);
}

//------------------------------------------------------------------------------
// what is fed in from outside
//------------------------------------------------------------------------------
TEST(Identifier_Tests, TheFuzzerOverSkeletonAndTheIdentifierCheck) {
    std::mt19937 rng(20260923);
    std::uniform_int_distribution<int> byte(0, 255);
    std::uniform_int_distribution<int> length(0, 40);
    for (int round = 0; round < 20000; ++round) {
        std::string raw;
        int n = length(rng);
        for (int i = 0; i < n; ++i) {
            raw.push_back(char(byte(rng)));
        }
        string text(raw.data(), raw.size());
        // nothing throws, nothing reads past the end, and the two forms
        // that are meant to be fixed points are ones
        auto bones = txt::skeleton(text);
        EXPECT_EQ(txt::skeleton(bones), txt::skeleton(bones));
        auto folded = txt::nfkc_casefold(text);
        EXPECT_TRUE(txt::is_nfkc_casefolded(folded));
        EXPECT_EQ(txt::nfkc_casefold(folded), folded);
        (void)txt::is_identifier(text);
        (void)txt::is_identifier(text, txt::program_syntax);
        (void)txt::restriction_level_of(text);
        (void)txt::is_confusable(text, bones);
    }

    // and the same over well formed text made of arbitrary code points
    std::uniform_int_distribution<uint32_t> point(0, 0x10FFFF);
    for (int round = 0; round < 20000; ++round) {
        std::string raw;
        char buf[utf8::max_width];
        int n = length(rng) / 4 + 1;
        for (int i = 0; i < n; ++i) {
            char32_t c = char32_t(point(rng));
            if (is_surrogate(c)) {
                continue;
            }
            raw.append(buf, utf8::encode(c, buf));
        }
        string text(raw.data(), raw.size());
        auto folded = txt::nfkc_casefold(text);
        EXPECT_TRUE(txt::is_nfkc_casefolded(folded)) << raw.size();
        EXPECT_EQ(txt::skeleton(text), reference_skeleton(text));
        (void)txt::is_identifier(text);
        (void)txt::restriction_level_of(text);
    }
}
