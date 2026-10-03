//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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

// Rule P1: a text is cut into paragraphs after every paragraph separator,
// the separator kept with the paragraph it ends, and each is resolved on
// its own. The file's cases are single paragraphs, so two of them joined
// by a separator must come out as each did alone: the first case's levels,
// then the separator at the first paragraph's level (L1), then the second
// case's; and the order of the first line, the separator at its end (or
// at its start when the line runs right to left, the whole line turned
// round), then the order of the second. The direction of the text is the
// first paragraph's. The text was taken as one paragraph, and the second
// case took its direction, its embeddings and its sequences from the first.
TEST(Bidi_Tests, EveryParagraphIsResolvedOnItsOwn) {
    size_t cases = 0, failed = 0;
    const char32_t separators[] = {0x2029, U'\n', U'\r', 0x1C, 0x85};
    size_t count = std::size(ucd::BidiCases);
    for (size_t k = 0; k + 2 < count; ++k) {
        // the case after it asked the same, or the one after that
        Case a = parse(ucd::BidiCases[k]);
        size_t next = k + 1;
        Case b = parse(ucd::BidiCases[next]);
        if (a.ask != b.ask) {
            b = parse(ucd::BidiCases[++next]);
            if (a.ask != b.ask) {
                continue;
            }
        }
        std::vector<char32_t> points = a.points;
        points.push_back(separators[k % std::size(separators)]);
        points.insert(points.end(), b.points.begin(), b.points.end());
        string text = utf8_of(points);
        auto at = offsets(text);
        size_t n = a.points.size();

        std::vector<int> want_levels = a.levels;
        want_levels.push_back(a.paragraph);
        want_levels.insert(want_levels.end(), b.levels.begin(), b.levels.end());
        auto levels = txt::levels(text, a.ask);
        ASSERT_EQ(levels.size(), want_levels.size());
        bool same = true;
        for (size_t i = 0; i < want_levels.size(); ++i) {
            if (want_levels[i] >= 0 && levels[i] != want_levels[i]) {
                same = false;
            }
        }

        std::vector<size_t> want_order;
        if (a.paragraph % 2) {
            want_order.push_back(at[n]);
        }
        for (auto i : a.order) {
            want_order.push_back(at[i]);
        }
        if (a.paragraph % 2 == 0) {
            want_order.push_back(at[n]);
        }
        for (auto i : b.order) {
            want_order.push_back(at[n + 1 + i]);
        }
        auto order = txt::visual_order(text, a.ask);
        same = same && std::vector<size_t>(order.begin(), order.end()) == want_order;

        auto runs = txt::bidi_runs(text, a.ask);
        same = same && runs.paragraph() == (a.paragraph % 2 ? txt::direction::right_to_left
                                                            : txt::direction::left_to_right);
        // no piece holds characters of both paragraphs: none reaches
        // over the end of the separator
        size_t boundary = at[n] + utf8::width(points[n]);
        for (auto r : runs) {
            size_t from = size_t(r.text.data() - text.data());
            if (from < boundary && from + r.text.size() > boundary) {
                same = false;
            }
        }
        if (!same && ++failed <= 5) {
            ADD_FAILURE() << "akapity: " << ucd::BidiCases[k] << " | " << ucd::BidiCases[next];
        }
        ++cases;
    }
    EXPECT_EQ(failed, 0u) << failed << " potknięć na " << cases << " przypadkach";
    EXPECT_GT(cases, 5000u);

    // a paragraph of each direction, decided by its own first strong letter
    string two = "abc \u05D0\u05D1\u2029\u05D2\u05D3 def";
    auto levels = txt::levels(two);
    EXPECT_EQ(txt::paragraph_direction(two), txt::direction::left_to_right);
    EXPECT_EQ(levels.front(), 0);       // a: the first paragraph runs left to right
    EXPECT_EQ(levels[7], 1);            // the first letter of the second, which runs right to left
    EXPECT_EQ(levels.back(), 2);        // f: Latin inside it
    // CR LF is one separator: the LF ends the right to left paragraph the
    // CR is in, and does not make one of its own, which would run left
    // to right, having no letter
    auto crlf = txt::levels(string("\u05D0\r\n\u05D1"));
    ASSERT_EQ(crlf.size(), 4u);
    EXPECT_EQ(crlf[1], 1);
    EXPECT_EQ(crlf[2], 1);
    EXPECT_EQ(txt::levels(string("\u05D0\n\n\u05D1"))[2], 0);   // an empty paragraph does
    // an empty paragraph between two separators
    EXPECT_EQ(txt::visual_order(string("a\n\nb")).size(), 4u);
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

// DESIGN 408: past the code space, the empty text in every direction, one
// code point, broken UTF-8, embeddings past the depth of 125, unmatched
// pops and brackets, and the range default-constructed, copied and moved
TEST(Bidi_Tests, TheEdges) {
    for (char32_t c : {char32_t(0x110000), char32_t(0xFFFFFFFF), char32_t(0xD800)}) {
        EXPECT_FALSE(txt::is_mirrored(c)) << std::hex << uint32_t(c);
        EXPECT_EQ(txt::mirrored_of(c), c);
    }
    EXPECT_EQ(txt::bidi_class_of(char32_t(0x10FFFF)), txt::bidi_class_of(char32_t(0x10FFFE)));
    static_assert(txt::mirrored_of(U'\0') == U'\0' && txt::mirrored_of(U'(') == U')');
    static_assert(txt::mirrored_of(U'｣') == U'｢');                 // the last pair of the file

    // The empty text, in every direction
    for (auto d : {txt::direction::automatic, txt::direction::left_to_right, txt::direction::right_to_left}) {
        EXPECT_TRUE(txt::levels(string(), d).empty());
        EXPECT_TRUE(txt::visual_order(string(), d).empty());
        EXPECT_EQ(txt::mirrored(string(), d), string());
        EXPECT_TRUE(txt::bidi_runs(string(), d).empty());
    }
    EXPECT_EQ(txt::bidi_runs(string(), txt::direction::right_to_left).paragraph(), txt::direction::right_to_left);
    EXPECT_EQ(txt::bidi_runs(string()).paragraph(), txt::direction::left_to_right);

    // One code point, and one forced the other way
    EXPECT_EQ(txt::levels(string("a")), (vector<uint8_t>{0}));
    EXPECT_EQ(txt::levels(string("\xD7\x90")), (vector<uint8_t>{1}));
    EXPECT_EQ(txt::levels(string("a"), txt::direction::right_to_left), (vector<uint8_t>{2}));
    EXPECT_EQ(txt::visual_order(string("\xD7\x90")), (vector<size_t>{0}));
    EXPECT_EQ(txt::mirrored(string("("), txt::direction::right_to_left), string(")"));

    // Broken UTF-8: a code point a byte, its position the byte's
    string broken("\xD7\x90\xFF\xD7\x91");
    EXPECT_EQ(txt::levels(broken).size(), 3u);
    auto order = txt::visual_order(broken);
    EXPECT_EQ(order, (vector<size_t>{3, 2, 0}));
    size_t bytes = 0;
    for (auto r : txt::bidi_runs(broken)) {
        bytes += r.text.size();
    }
    EXPECT_EQ(bytes, broken.size());
    EXPECT_EQ(txt::mirrored(string("\xFF("), txt::direction::right_to_left), string("\xFF)"));

    // Deeper than the 125 levels the standard allows: the extra
    // embeddings overflow and are ignored, nothing past 125 is reached
    for (const char* opener : {"‫", "‪", "⁧", "⁦", "‮"}) {
        std::string deep;
        for (int i = 0; i < 300; ++i) {
            deep += opener;
        }
        deep += "a\xD7\x90";
        for (int i = 0; i < 300; ++i) {
            deep += opener[2] == '\xA7' || opener[2] == '\xA6' ? "⁩" : "‬";
        }
        string text(deep.data(), deep.size());
        auto levels = txt::levels(text);
        EXPECT_EQ(levels.size(), 602u) << opener;
        for (auto l : levels) {
            ASSERT_LE(l, 126) << opener;                               // 125, and one more for a number or a letter on it (I1, I2)
        }
        EXPECT_EQ(txt::visual_order(text).size() <= 602u, true);
        EXPECT_FALSE(txt::bidi_runs(text).empty());
    }

    // Pops with nothing to pop, and brackets that do not pair
    string pops("‬⁩a‬⁩");
    EXPECT_EQ(txt::levels(pops).size(), 5u);
    EXPECT_EQ(txt::paragraph_direction(pops), txt::direction::left_to_right);
    std::string brackets(200, '(');
    brackets += "\xD7\x90";
    brackets += std::string(200, ']');
    string unpaired(brackets.data(), brackets.size());
    EXPECT_EQ(txt::levels(unpaired).size(), 401u);
    EXPECT_EQ(txt::mirrored(unpaired, txt::direction::right_to_left).size(), unpaired.size());

    // The range default-constructed, copied, moved
    txt::bidi_runs none;
    EXPECT_TRUE(none.empty());
    EXPECT_EQ(none.count(), 0u);
    EXPECT_TRUE(none.text().empty());
    EXPECT_EQ(none.paragraph(), txt::direction::left_to_right);
    txt::bidi_runs some(string("abc \xD7\x90"));
    auto copy = some;
    EXPECT_EQ(copy.count(), some.count());
    auto moved = std::move(copy);
    EXPECT_EQ(moved.count(), some.count());
    (void)copy.count();
    EXPECT_FALSE(txt::bidi_runs::run{}.right_to_left());
    const char* null = nullptr;
    EXPECT_TRUE(txt::bidi_runs(null).empty());

    // mirrored with levels in hand: none, shorter, into itself
    string self("(a)");
    EXPECT_EQ(txt::mirrored(self, vector<uint8_t>{}).data(), self.data());
    EXPECT_EQ(txt::mirrored(self, vector<uint8_t>{1}), string(")a)"));
    self = txt::mirrored(self, txt::direction::right_to_left);
    EXPECT_EQ(self, string(")a("));
}

namespace {
    // a = std::move(a) without the compiler's warning about it
    template<class T>
    void move_into_itself(T& a) {
        T& same = a;
        a = std::move(same);
    }
}

// Runs moved from are the empty ones, as the ones made with nothing (after
// DESIGN 429)
TEST(Bidi_Tests, MovedFromRunsAreTheEmptyOnes) {
    txt::bidi_runs runs(string("abc \xD7\x90\xD7\x91"), txt::direction::right_to_left);
    auto moved = std::move(runs);
    EXPECT_EQ(moved.count(), 2u);
    EXPECT_EQ(moved.paragraph(), txt::direction::right_to_left);
    EXPECT_TRUE(runs.empty());                                 // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(runs.count(), 0u);
    EXPECT_TRUE(runs.text().empty());
    EXPECT_EQ(runs.paragraph(), txt::direction::left_to_right);
    EXPECT_TRUE(runs.begin() == runs.end());
    runs = txt::bidi_runs(string("abc"));
    EXPECT_EQ(runs.count(), 1u);
    move_into_itself(runs);
    EXPECT_EQ(runs.count(), 1u);
    auto copy = moved;
    EXPECT_EQ(copy.count(), 2u);
    EXPECT_EQ(moved.count(), 2u);
}
