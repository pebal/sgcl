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

#include <string>
#include <vector>

namespace {
    // A call to a deleted function in a requires-expression outside a
    // template is a hard error, so the refusal is seen through a concept
    template<class T> concept Directions = requires(T c) { txt::direction_of(c); };

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
    static_assert(txt::direction_of(U'a') == bidi::l);
    static_assert(txt::direction_of(U'א') == bidi::r);          // Hebrew alef
    static_assert(txt::direction_of(U'ا') == bidi::al);         // Arabic alef
    static_assert(txt::direction_of(U'1') == bidi::en);
    static_assert(txt::direction_of(U'٠') == bidi::an);         // Arabic-Indic zero
    static_assert(txt::direction_of(U' ') == bidi::ws);
    static_assert(txt::direction_of(U'.') == bidi::cs);
    static_assert(txt::direction_of(U'(') == bidi::on);
    static_assert(txt::direction_of(U'́') == bidi::nsm);
    static_assert(txt::direction_of(U'‏') == bidi::r);          // the right to left mark
    static_assert(txt::direction_of(U'⁦') == bidi::lri);
    static_assert(txt::direction_of(U'\n') == bidi::b);

    // An unassigned code point of the Hebrew block is right to left
    // before anything is put there
    static_assert(txt::direction_of(char32_t(0x05EB)) == bidi::r);
    static_assert(txt::direction_of(char32_t(0x0870)) == bidi::al);

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
