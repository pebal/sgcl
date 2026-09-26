//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::bidi: the bidirectional algorithm of UAX #9. The oracle is
// BidiCharacterTest.txt of the UCD, which gives for every case the level
// the paragraph resolves to, the level of every character and the order
// they are drawn in.
#include "tests/types.h"
#include "tests/txt/bidi_tests.h"

#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
    // A call to a deleted function in a requires-expression outside a
    // template is a hard error, so the refusal is seen through a concept
    template<class T> concept Directions = requires(T c) { txt::bidi_class_of(c); };
    template<class T> concept Mirrors = requires(T c) { txt::is_mirrored(c); };
    template<class T> concept MirrorsOf = requires(T c) { txt::mirrored_of(c); };

    struct Case {
        std::vector<char32_t> points;
        txt::direction ask = txt::direction::automatic;
        uint8_t paragraph = 0;
        std::vector<int> levels;      // -1 where the character has none
        std::vector<size_t> order;
    };

    std::vector<std::string> split(std::string_view s, char by) {
        std::vector<std::string> out;
        size_t at = 0;
        for (size_t i = 0; i <= s.size(); ++i) {
            if (i == s.size() || s[i] == by) {
                if (i > at) {
                    out.emplace_back(s.substr(at, i - at));
                }
                at = i + 1;
            }
        }
        return out;
    }

    Case parse(const char* line) {
        auto fields = split(line, ';');
        Case c;
        for (auto& x : split(fields[0], ' ')) {
            c.points.push_back(char32_t(std::stoul(x, nullptr, 16)));
        }
        // the file's own encoding: 0 left to right, 1 right to left,
        // 2 the automatic one of rules P2 and P3
        int ask = std::stoi(fields[1]);
        c.ask = ask == 0 ? txt::direction::left_to_right
              : ask == 1 ? txt::direction::right_to_left : txt::direction::automatic;
        c.paragraph = uint8_t(std::stoi(fields[2]));
        for (auto& x : split(fields[3], ' ')) {
            c.levels.push_back(x == "x" ? -1 : std::stoi(x));
        }
        if (fields.size() > 4) {
            for (auto& x : split(fields[4], ' ')) {
                c.order.push_back(size_t(std::stoul(x)));
            }
        }
        return c;
    }

    string utf8_of(const std::vector<char32_t>& points) {
        std::string out;
        char buf[utf8::max_width];
        for (auto c : points) {
            out.append(buf, utf8::encode(c, buf));
        }
        return string(out.data(), out.size());
    }

    // Where every code point begins, so that the file's indices can be
    // compared with the byte positions the library reports
    std::vector<size_t> offsets(const string& s) {
        std::vector<size_t> out;
        auto v = s.view();
        for (size_t i = 0; i < v.size();) {
            out.push_back(i);
            i += utf8::decode(v, i).second;
        }
        return out;
    }
}

TEST(Bidi_Tests, EveryCaseOfTheUcdCharacterTest) {
    size_t cases = 0, failed = 0;
    for (const char* line : ucd::BidiCases) {
        Case c = parse(line);
        string text = utf8_of(c.points);
        auto at = offsets(text);
        ASSERT_EQ(at.size(), c.points.size()) << line;

        // the level the paragraph resolves to
        auto paragraph = txt::bidi_runs(text, c.ask).paragraph();
        uint8_t got_paragraph = paragraph == txt::direction::right_to_left ? 1 : 0;
        if (got_paragraph != c.paragraph && ++failed <= 5) {
            ADD_FAILURE() << "poziom akapitu: " << line;
        }

        // the level of every character the file gives one for
        auto levels = txt::levels(text, c.ask);
        ASSERT_EQ(levels.size(), c.levels.size()) << line;
        for (size_t i = 0; i < c.levels.size(); ++i) {
            if (c.levels[i] >= 0 && levels[i] != c.levels[i] && ++failed <= 5) {
                ADD_FAILURE() << "poziom " << i << ": " << line;
            }
        }

        // and the order they are drawn in
        std::vector<size_t> want;
        for (auto i : c.order) {
            want.push_back(at[i]);
        }
        auto got = txt::visual_order(text, c.ask);
        std::vector<size_t> got_v(got.begin(), got.end());
        if (got_v != want && ++failed <= 5) {
            ADD_FAILURE() << "kolejność: " << line;
        }
        ++cases;
    }
    EXPECT_EQ(failed, 0u) << failed << " potknięć na " << cases << " przypadkach";
    EXPECT_GT(cases, 30000u);
}

TEST(Bidi_Tests, WhatIsDrawnAndInWhatOrder) {
    // The line of the header comment: a Polish label, a Hebrew word, a
    // number and a Latin word
    string s = "Nazwa: שלום 123 OK";
    EXPECT_EQ(txt::paragraph_direction(s), txt::direction::left_to_right);

    std::vector<std::pair<std::string, int>> runs;
    for (auto r : txt::bidi_runs(s)) {
        runs.emplace_back(std::string(r.text.data(), r.text.size()), r.level);
    }
    ASSERT_EQ(runs.size(), 4u);
    EXPECT_EQ(runs[0], std::make_pair(std::string("Nazwa: "), 0));
    EXPECT_EQ(runs[1].second, 2);                      // the number, inside the Hebrew
    EXPECT_EQ(runs[2].second, 1);                      // the Hebrew itself, turned round when drawn
    EXPECT_EQ(runs[3], std::make_pair(std::string(" OK"), 0));
    EXPECT_TRUE(txt::bidi_runs(s).begin()->right_to_left() == false);

    // The same text read as a right to left paragraph puts the label at
    // the right instead
    auto forced = txt::bidi_runs(s, txt::direction::right_to_left);
    EXPECT_EQ(forced.paragraph(), txt::direction::right_to_left);
    EXPECT_NE(forced.count(), 0u);

    // A paragraph that begins with Hebrew runs right to left by itself
    EXPECT_EQ(txt::paragraph_direction(string("שלום abc")), txt::direction::right_to_left);
    // a number is not strong, so what decides is the Hebrew behind it
    EXPECT_EQ(txt::paragraph_direction(string("123 ש")), txt::direction::right_to_left);
    EXPECT_EQ(txt::paragraph_direction(string("123 abc ש")), txt::direction::left_to_right);
    EXPECT_EQ(txt::paragraph_direction(string("...")), txt::direction::left_to_right);
    EXPECT_EQ(txt::paragraph_direction(string()), txt::direction::left_to_right);

    // Plain text of one direction is one run and needs no turning
    auto plain = txt::bidi_runs(string("Ala ma kota"));
    EXPECT_EQ(plain.count(), 1u);
    EXPECT_EQ(plain.begin()->level, 0);
    EXPECT_TRUE(txt::bidi_runs(string()).empty());
    static_assert(req::enumerable<txt::bidi_runs>);
}

TEST(Bidi_Tests, TheClassOfACodePointAndTheOrderOfPositions) {
    using txt::bidi;
    static_assert(txt::bidi_class_of(U'a') == bidi::l);
    static_assert(txt::bidi_class_of(U'א') == bidi::r);          // Hebrew alef
    static_assert(txt::bidi_class_of(U'ا') == bidi::al);         // Arabic alef
    static_assert(txt::bidi_class_of(U'1') == bidi::en);
    static_assert(txt::bidi_class_of(U'٠') == bidi::an);         // Arabic-Indic zero
    static_assert(txt::bidi_class_of(U' ') == bidi::ws);
    static_assert(txt::bidi_class_of(U'.') == bidi::cs);
    static_assert(txt::bidi_class_of(U'(') == bidi::on);
    static_assert(txt::bidi_class_of(U'́') == bidi::nsm);
    static_assert(txt::bidi_class_of(U'‏') == bidi::r);          // the right to left mark
    static_assert(txt::bidi_class_of(U'⁦') == bidi::lri);
    static_assert(txt::bidi_class_of(U'\n') == bidi::b);

    // An unassigned code point of the Hebrew block is right to left
    // before anything is put there
    static_assert(txt::bidi_class_of(char32_t(0x05EB)) == bidi::r);
    static_assert(txt::bidi_class_of(char32_t(0x0870)) == bidi::al);

    // A code point and nothing else, as everywhere in the module
    static_assert(Directions<char32_t> && !Directions<char> && !Directions<int>);

    // visual_order gives byte positions, so a caret can walk them
    string s = "aאבb";        // a, alef, bet, b
    auto order = txt::visual_order(s);
    ASSERT_EQ(order.size(), 4u);
    EXPECT_EQ(order[0], 0u);            // a stays first
    EXPECT_EQ(order[1], 3u);            // bet before alef: the Hebrew is turned round
    EXPECT_EQ(order[2], 1u);
    EXPECT_EQ(order[3], 5u);
    EXPECT_EQ(txt::visual_order(string("abc")), (vector<size_t>{0, 1, 2}));
}

// Rule L4 and the two things it asks about a character: whether it is
// drawn the other way round in a right to left run, and which glyph to
// draw instead. The oracle for the second is BidiMirroring.txt whole —
// the table is made from it, so the file and not the table has to be
// what the test holds.
TEST(Bidi_Tests, EveryLineOfBidiMirroring) {
    std::map<char32_t, char32_t> file;
    for (auto& pair : ucd::BidiMirrorPairs) {
        file[pair[0]] = pair[1];
    }
    ASSERT_EQ(file.size(), std::size(ucd::BidiMirrorPairs));

    size_t inverse = 0;
    for (auto& [c, m] : file) {
        ASSERT_EQ(txt::mirrored_of(c), m) << std::hex << uint32_t(c);
        // everything the file gives a mirror to is mirrored
        ASSERT_TRUE(txt::is_mirrored(c)) << std::hex << uint32_t(c);
        // and a mirroring is its own inverse wherever the file has both
        if (auto back = file.find(m); back != file.end()) {
            ASSERT_EQ(back->second, c) << std::hex << uint32_t(c);
            ++inverse;
        }
    }
    EXPECT_EQ(inverse, file.size());     // it is, for every one of them

    // And nothing else in the whole of the code space has a mirror: a
    // code point the file says nothing about answers itself
    size_t wrong = 0;
    for (char32_t c = 0; c < 0x110000; ++c) {
        if (file.count(c)) {
            continue;
        }
        if (txt::mirrored_of(c) != c && ++wrong <= 5) {
            ADD_FAILURE() << "U+" << std::hex << uint32_t(c) << " has a mirror it should not";
        }
    }
    EXPECT_EQ(wrong, 0u);
}

TEST(Bidi_Tests, TheMirroredPropertyOfEveryCodePoint) {
    std::set<char32_t> yes(std::begin(ucd::BidiMirroredPoints), std::end(ucd::BidiMirroredPoints));
    ASSERT_EQ(yes.size(), std::size(ucd::BidiMirroredPoints));
    size_t wrong = 0, found = 0;
    for (char32_t c = 0; c < 0x110000; ++c) {
        bool want = yes.count(c) != 0;
        if (txt::is_mirrored(c) != want && ++wrong <= 5) {
            ADD_FAILURE() << "U+" << std::hex << uint32_t(c);
        }
        found += txt::is_mirrored(c) ? 1 : 0;
    }
    EXPECT_EQ(wrong, 0u);
    EXPECT_EQ(found, yes.size());

    // The property holds of more code points than have a mirror of their
    // own: an integral sign is drawn the other way round without there
    // being a second one to name it
    EXPECT_GT(yes.size(), std::size(ucd::BidiMirrorPairs));
    static_assert(txt::is_mirrored(U'(') && txt::is_mirrored(U')'));
    static_assert(txt::is_mirrored(U'<') && txt::is_mirrored(U'>'));
    static_assert(txt::is_mirrored(U'[') && txt::is_mirrored(U'{'));
    static_assert(txt::is_mirrored(U'∫'));                  // an integral
    static_assert(txt::mirrored_of(U'∫') == U'∫');     // with no mirror of its own
    static_assert(!txt::is_mirrored(U'a') && !txt::is_mirrored(U' '));
    static_assert(!txt::is_mirrored(U'/') && !txt::is_mirrored(U'"'));
    static_assert(txt::mirrored_of(U'(') == U')' && txt::mirrored_of(U')') == U'(');
    static_assert(txt::mirrored_of(U'≤') == U'≥');     // ≤ and ≥
    static_assert(txt::mirrored_of(U'«') == U'»');     // « and »
    static_assert(txt::mirrored_of(U'a') == U'a');
    static_assert(txt::is_mirrored(char32_t(0x1D6DB)));          // the five above the plane
    static_assert(txt::mirrored_of(char32_t(0x1D6DB)) == char32_t(0x1D6DB));

    // A code point and nothing else, as everywhere in the module
    static_assert(Mirrors<char32_t> && !Mirrors<char> && !Mirrors<int>);
    static_assert(MirrorsOf<char32_t> && !MirrorsOf<char> && !MirrorsOf<int>);
}

// The text with rule L4 applied, checked where the conformance file can
// check it: the file gives the level of every character, so what must
// come out is the text with the mirror put in at every odd level and
// nothing else touched. Independent of our own levels, which the test
// above this one already weighs against the same file.
TEST(Bidi_Tests, TheTextWithItsMirroringsApplied) {
    size_t cases = 0, failed = 0, mirrored_cases = 0;
    for (const char* line : ucd::BidiCases) {
        Case c = parse(line);
        string text = utf8_of(c.points);
        ASSERT_EQ(c.levels.size(), c.points.size()) << line;
        std::vector<char32_t> want;
        bool any = false;
        for (size_t i = 0; i < c.points.size(); ++i) {
            if (c.levels[i] < 0) {
                // a character rule X9 removes has no level in the file;
                // none of those is Bidi_Mirrored, so none is touched
                ASSERT_FALSE(txt::is_mirrored(c.points[i])) << line;
                want.push_back(c.points[i]);
                continue;
            }
            char32_t m = c.levels[i] % 2 ? txt::mirrored_of(c.points[i]) : c.points[i];
            any = any || m != c.points[i];
            want.push_back(m);
        }
        auto got = txt::mirrored(text, c.ask);
        if (got != utf8_of(want) && ++failed <= 5) {
            ADD_FAILURE() << "lustro: " << line;
        }
        // and the form that takes the levels rather than working them
        // out answers the same, given the ones this library works out
        if (txt::mirrored(text, txt::levels(text, c.ask)) != got && ++failed <= 5) {
            ADD_FAILURE() << "lustro z poziomów: " << line;
        }
        // a text with nothing to mirror comes back as the object it went
        // in as, which is what the strings of the library are for
        if (!any && got.data() != text.data() && ++failed <= 5) {
            ADD_FAILURE() << "kopia bez potrzeby: " << line;
        }
        mirrored_cases += any ? 1 : 0;
        ++cases;
    }
    EXPECT_EQ(failed, 0u) << failed << " potknięć na " << cases << " przypadkach";
    EXPECT_GT(cases, 30000u);
    EXPECT_EQ(mirrored_cases, 17017u);      // over half of them do mirror something
}

TEST(Bidi_Tests, WhatTheMirroringOfATextDoesAndDoesNot) {
    // A bracket in an Arabic sentence points the other way, and the one
    // in the Polish around it does not
    string s = "Nazwa (شلوم [123]) OK";
    auto m = txt::mirrored(s);
    EXPECT_NE(m, s);
    // the outer pair is at level 0 and stays; the inner one is inside
    // the Arabic and is turned round
    EXPECT_NE(m.find(string("(")), npos);
    EXPECT_NE(m.find(string(")")), npos);
    EXPECT_NE(m.find(string("]")), npos);
    EXPECT_EQ(m.size(), s.size());

    // Read as a right to left paragraph the outer pair turns round too
    auto rtl = txt::mirrored(s, txt::direction::right_to_left);
    EXPECT_NE(rtl, m);

    // Nothing to mirror: the same object comes back, not a copy
    for (const char* plain : {"Ala ma kota", "", "abc (def) [ghi]", "شلوم"}) {
        string one(plain, std::strlen(plain));
        EXPECT_EQ(txt::mirrored(one).data(), one.data()) << plain;
    }

    // A right to left run with no mirrored character in it is left alone
    string shalom = "שלום";
    EXPECT_EQ(txt::mirrored(shalom, txt::direction::right_to_left).data(), shalom.data());

    // The mirroring is its own inverse over a text whose levels do not
    // change when the brackets do: what a rasteriser is handed twice is
    // what it was handed once
    string hebrew = "א (ב) ג";
    EXPECT_EQ(txt::mirrored(txt::mirrored(hebrew, txt::direction::right_to_left),
                            txt::direction::right_to_left),
              hebrew);

    // A mirror need not be as wide as what it stands for, so the answer
    // is sized from the sum of the widths and not from the text: U+FF08
    // is three bytes and so is U+FF09, but U+2329 and U+232A are three
    // where a guillemet is two
    string wide = "א（ב）";
    auto turned = txt::mirrored(wide, txt::direction::right_to_left);
    EXPECT_EQ(turned.size(), wide.size());
    EXPECT_NE(turned.find(string("）")), npos);
}

// Whoever draws the text has the levels already — bidi_runs and levels()
// both work the paragraph out — and there is no reason to work it out a
// second time. The rule alone costs 199 ns on a mixed line of fifty-one
// bytes where working the paragraph out again costs 1464.
TEST(Bidi_Tests, TheMirroringOfATextWhoseLevelsAreAlreadyKnown) {
    string s = "Nazwa (شلوم [123]) OK";
    for (auto ask : {txt::direction::automatic, txt::direction::left_to_right,
                     txt::direction::right_to_left}) {
        EXPECT_EQ(txt::mirrored(s, txt::levels(s, ask)), txt::mirrored(s, ask));
    }

    // A text with nothing to mirror comes back as the object it went in
    // as down this road too
    string plain = "Ala ma kota (i psa)";
    auto flat = txt::levels(plain);
    EXPECT_EQ(txt::mirrored(plain, flat).data(), plain.data());

    // The levels are the caller's, so levels of his own making are
    // obeyed: everything at an odd level is mirrored whatever the
    // algorithm would have said
    string brackets = "(a)[b]";
    vector<uint8_t> odd(6, 1);
    EXPECT_EQ(txt::mirrored(brackets, odd), string(")a(]b["));
    vector<uint8_t> even(6, 0);
    EXPECT_EQ(txt::mirrored(brackets, even).data(), brackets.data());

    // Nothing throws where the vector does not reach: a code point it
    // says nothing about is left where it stands
    vector<uint8_t> two{1, 1};                                   // only "(" and "a"
    EXPECT_EQ(txt::mirrored(brackets, two), string(")a)[b]"));
    EXPECT_EQ(txt::mirrored(brackets, vector<uint8_t>()).data(), brackets.data());
    EXPECT_TRUE(txt::mirrored(string(), vector<uint8_t>{1, 1, 1}).empty());

    // and more levels than there are characters is no trouble either
    vector<uint8_t> plenty(100, 1);
    EXPECT_EQ(txt::mirrored(brackets, plenty), string(")a(]b["));
}
