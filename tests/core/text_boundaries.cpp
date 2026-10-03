//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of DESIGN 408 for the text of core: utf8, unicode, runes,
// the text mixin of string and slice for every character type — the code
// points at the edges of each encoding's widths, the values that are no
// code point, a position that cuts one, the empty text, positions at and
// past the end and npos.
#include "tests/types.h"

#include <compare>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {
    // The units of valid code points in the encoding of CharT
    template<class CharT>
    std::basic_string<CharT> units_of(std::u32string_view points) {
        std::basic_string<CharT> out;
        for (char32_t c : points) {
            if constexpr (sizeof(CharT) == 1) {
                char b[utf8::max_width];
                size_t n = utf8::encode(c, b);
                for (size_t k = 0; k < n; ++k) {
                    out.push_back(CharT(b[k]));
                }
            } else if constexpr (sizeof(CharT) == 2) {
                if (c >= 0x10000) {
                    out.push_back(CharT(0xD800 + ((c - 0x10000) >> 10)));
                    out.push_back(CharT(0xDC00 + ((c - 0x10000) & 0x3FF)));
                } else {
                    out.push_back(CharT(c));
                }
            } else {
                out.push_back(CharT(c));
            }
        }
        return out;
    }

    template<class CharT>
    basic_string<CharT> text_of(std::u32string_view points) {
        auto units = units_of<CharT>(points);
        return basic_string<CharT>(std::basic_string_view<CharT>(units));
    }

    // Every search for a code point, in a text of every width of the
    // encoding: the first and last code point of each width, NUL inside,
    // U+FFFD among them
    template<class CharT>
    void every_code_point_search() {
        using S = basic_string<CharT>;
        const std::u32string points = std::u32string(U"a\u007F\u0080߿ࠀ�￿\U00010000\U0010FFFF") + char32_t(0) + U"z";
        S s = text_of<CharT>(points);
        ASSERT_EQ(s.size(), units_of<CharT>(points).size());
        for (size_t k = 0; k < points.size(); ++k) {
            const char32_t c = points[k];
            const size_t at = units_of<CharT>(std::u32string_view(points).substr(0, k)).size();
            const size_t width = units_of<CharT>(std::u32string_view(&c, 1)).size();
            const size_t before = k == 0 ? 0 : units_of<CharT>(std::u32string_view(&points[k - 1], 1)).size();
            SCOPED_TRACE(testing::Message() << "U+" << std::hex << uint32_t(c) << " in units of " << sizeof(CharT));
            EXPECT_EQ(s.find(c), at);
            EXPECT_EQ(s.rfind(c), at);
            EXPECT_EQ(s.find(c, at + 1), npos);
            EXPECT_EQ(s.rfind(c, at), at);
            EXPECT_EQ(s.rfind(c, at + width - 1), at);   // a position inside it: the code point that covers it
            EXPECT_TRUE(s.contains(c));
            EXPECT_EQ(s.find_first_of(c), at);
            EXPECT_EQ(s.find_last_of(c), at);
            EXPECT_EQ(s.find_first_of(c, at + width), npos);
            EXPECT_EQ(s.find_first_not_of(c, at), at + width == s.size() ? npos : at + width);
            EXPECT_EQ(s.find_last_not_of(c, at + width - 1), k == 0 ? npos : at - before);
            size_t pieces = 0;
            for (auto piece : s.split(c)) {
                EXPECT_EQ(piece.size(), pieces == 0 ? at : s.size() - at - width);
                ++pieces;
            }
            EXPECT_EQ(pieces, 2u);
            EXPECT_EQ(s.replace(c, U'x').size(), s.size() - width + 1);
            EXPECT_EQ(S::join(s.split(c), c), s);
        }
        EXPECT_TRUE(s.starts_with(U'a') && s.ends_with(U'z') && !s.starts_with(U'z') && !s.ends_with(U'a'));
        // the positions at and past the end, and npos
        for (size_t pos : {s.size(), s.size() + 1, npos}) {
            EXPECT_EQ(s.find(U'z', pos), npos);
            EXPECT_EQ(s.find_first_of(U'z', pos), npos);
            EXPECT_EQ(s.find_first_not_of(U'z', pos), npos);
            EXPECT_EQ(s.rfind(U'z', pos), s.size() - 1);
            EXPECT_EQ(s.find_last_of(U'z', pos), s.size() - 1);
            EXPECT_EQ(s.find_last_of(U"za", pos), s.size() - 1);
            EXPECT_EQ(s.find_last_not_of(U"z", pos), s.size() - 2);   // NUL
        }
        EXPECT_EQ(s.find_last_of(U'a', 0), 0u);
        EXPECT_EQ(s.find_last_not_of(U'a', 0), npos);
        // the empty set: nothing is in it, everything is not
        EXPECT_EQ(s.find_first_of(std::u32string_view()), npos);
        EXPECT_EQ(s.find_last_of(std::u32string_view()), npos);
        EXPECT_EQ(s.find_first_not_of(std::u32string_view()), 0u);
        EXPECT_EQ(s.find_last_not_of(std::u32string_view()), s.size() - 1);
        // the empty text
        S empty;
        for (char32_t c : points) {
            EXPECT_EQ(empty.find(c), npos);
            EXPECT_EQ(empty.rfind(c), npos);
            EXPECT_FALSE(empty.contains(c) || empty.starts_with(c) || empty.ends_with(c));
            EXPECT_EQ(empty.find_first_of(c), npos);
            EXPECT_EQ(empty.find_last_of(c), npos);
            EXPECT_EQ(empty.find_first_not_of(c), npos);
            EXPECT_EQ(empty.find_last_not_of(c), npos);
            EXPECT_TRUE(empty.split(c).empty());
            EXPECT_TRUE(empty.replace(c, U'x').empty());
        }
        EXPECT_EQ(empty.find_first_not_of(std::u32string_view()), npos);
        EXPECT_EQ(empty.find_last_not_of(std::u32string_view(), npos), npos);
        // the same searches on a slice of the text, and on the empty slice
        auto whole = s.as_slice();
        EXPECT_EQ(whole.find(U'\U0010FFFF'), s.find(U'\U0010FFFF'));
        EXPECT_EQ(whole.find_last_of(U'\U00010000'), s.find_last_of(U'\U00010000'));
        EXPECT_TRUE(whole.contains(U'￿'));
        slice<const CharT> none;
        EXPECT_FALSE(none.contains(U'a') || none.starts_with(U'a') || none.ends_with(U'a'));
        EXPECT_EQ(none.find(U'a'), npos);
        EXPECT_EQ(none.find_first_not_of(U'a'), npos);
        EXPECT_EQ(none.find_last_of(U'a'), npos);
    }

    // A value that is no code point (a surrogate, past U+10FFFF) is in no
    // text whose code points are encoded, U+FFFD among them: it is not
    // found, split at or replaced. It was encoded as U+FFFD and found
    // where the text had one. As what is written it is U+FFFD, as
    // utf8::encode writes it.
    template<class CharT>
    void no_code_point_is_found() {
        using S = basic_string<CharT>;
        S s = text_of<CharT>(U"a�b\U0001F600");
        S replacement = text_of<CharT>(U"�");
        for (char32_t bad : {char32_t(0xD800), char32_t(0xDBFF), char32_t(0xDC00), char32_t(0xDFFF), char32_t(0x110000), char32_t(0xFFFFFFFF)}) {
            SCOPED_TRACE(testing::Message() << std::hex << uint32_t(bad) << " in units of " << sizeof(CharT));
            EXPECT_EQ(s.find(bad), npos);
            EXPECT_EQ(s.rfind(bad), npos);
            EXPECT_FALSE(s.contains(bad));
            EXPECT_FALSE(replacement.starts_with(bad));
            EXPECT_FALSE(replacement.ends_with(bad));
            EXPECT_FALSE(s.as_slice().contains(bad));
            EXPECT_EQ(s.as_slice().find(bad), npos);
            EXPECT_EQ(s.find_first_of(bad), npos);
            EXPECT_EQ(s.find_last_of(bad), npos);
            EXPECT_EQ(s.find_first_not_of(bad), 0u);
            EXPECT_EQ(s.find_last_not_of(bad), s.size() - units_of<CharT>(U"\U0001F600").size());
            size_t pieces = 0;
            for (auto piece : s.split(bad)) {
                EXPECT_EQ(piece.view(), s.view());
                ++pieces;
            }
            EXPECT_EQ(pieces, 1u);
            EXPECT_TRUE(S().split(bad).empty());
            EXPECT_EQ(s.replace(bad, U'x').object(), s.object());
            EXPECT_EQ(s.replace(U'a', bad), text_of<CharT>(U"��b\U0001F600"));
            EXPECT_EQ(S::join(sgcl::vector<S>{text_of<CharT>(U"a"), text_of<CharT>(U"b")}, bad), text_of<CharT>(U"a�b"));
        }
    }
}

TEST(TextBoundary_Tests, EveryCodePointSearchAtTheEdgesOfEveryEncoding) {
    every_code_point_search<char>();
    every_code_point_search<char8_t>();
    every_code_point_search<wchar_t>();
    every_code_point_search<char16_t>();
    every_code_point_search<char32_t>();
}

TEST(TextBoundary_Tests, AValueThatIsNoCodePointIsNotFound) {
    no_code_point_is_found<char>();
    no_code_point_is_found<char8_t>();
    no_code_point_is_found<wchar_t>();
    no_code_point_is_found<char16_t>();
    // a u32string's units are its characters: a surrogate unit it holds is found
    u32string raw{std::u32string_view(U"a\xD800" "b", 3)};
    EXPECT_EQ(raw.find(char32_t(0xD800)), 1u);
}

// A forward search from a position that cuts a code point starts with the
// next code point, as a backward one starts with the code point that
// covers the position: neither answers a position inside a code point
TEST(TextBoundary_Tests, AForwardSearchFromInsideACodePointStartsWithTheNextOne) {
    string s = "żółw";   // ż at 0, ó at 2, ł at 4, w at 6
    EXPECT_EQ(s.find_first_not_of(U"ż", 1), 2u);
    EXPECT_EQ(s.find_first_of(U"�", 1), npos);
    EXPECT_EQ(s.find_first_of(U"ó", 1), 2u);
    EXPECT_EQ(s.find_first_of(U'ó', 3), npos);
    EXPECT_EQ(s.find_first_not_of(U"ó", 3), 4u);
    EXPECT_EQ(s.find_first_of(U"żółw", 5), 6u);
    EXPECT_EQ(s.find(U'ó', 3), npos);
    EXPECT_EQ(s.rfind(U'ó', 3), 2u);
    EXPECT_EQ(s.find_last_of(U"ó", 3), 2u);
    EXPECT_EQ(s.find_last_not_of(U"ł", 5), 2u);
    EXPECT_EQ(s.as_slice().find_first_not_of(U"ż", 1), 2u);
    string emoji = "a😀b";   // the emoji at 1 to 4
    for (size_t pos = 2; pos <= 4; ++pos) {
        EXPECT_EQ(emoji.find_first_not_of(U"x", pos), 5u) << pos;
        EXPECT_EQ(emoji.find_first_of(U"�😀", pos), npos) << pos;
        EXPECT_EQ(emoji.find_last_of(U"😀", pos), 1u) << pos;
    }
    // stray continuation bytes are a code point each, where a position may begin
    string stray = "\x80\x80x";
    EXPECT_EQ(stray.find_first_not_of(U"�", 1), 2u);
    EXPECT_EQ(stray.find_first_of(U"�", 1), 1u);
    // UTF-16: the low half of a pair
    u16string u = u"a😀b";   // the pair at 1 and 2
    EXPECT_EQ(u.find_first_not_of(U"a", 2), 3u);
    EXPECT_EQ(u.find_first_of(U"�", 2), npos);
    EXPECT_EQ(u.find_first_of(U'😀', 2), npos);
    EXPECT_EQ(u.find_last_of(U'😀', 2), 1u);
    EXPECT_EQ(u.find(U'😀', 2), npos);
    EXPECT_EQ(u.rfind(U'😀', 2), 1u);
    // a lone low surrogate is a code point of its own, where a position may begin
    u16string lone{std::u16string_view(u"a\xDC00" "b", 3)};
    EXPECT_EQ(lone.find_first_of(U"�", 1), 1u);
}

// equal_fold: an ill-formed byte equals the same byte alone, never another
// ill-formed byte nor U+FFFD written out (all of them decode as U+FFFD,
// so two texts of different bytes and no letter were equal)
TEST(TextBoundary_Tests, EqualFoldTakesAnIllFormedByteForItself) {
    EXPECT_FALSE(string("\xC5").equal_fold("\xC4"));
    EXPECT_FALSE(string("\x80").equal_fold("\xFF"));
    EXPECT_FALSE(string("x\xE2\x82").equal_fold("X\xE2\x83"));
    EXPECT_FALSE(string("\xC5").equal_fold("�"));
    EXPECT_FALSE(string("�").equal_fold("\xC5"));
    EXPECT_FALSE(u8string(u8"a").equal_fold(std::u8string_view(reinterpret_cast<const char8_t*>("\xC5"), 1)));
    EXPECT_TRUE(string("\xC5").equal_fold("\xC5"));
    EXPECT_TRUE(string("a\xC5Ł").equal_fold("A\xC5ł"));
    EXPECT_TRUE(string("�").equal_fold("�"));
    EXPECT_TRUE(string().equal_fold(""));
    EXPECT_TRUE(slice<const char>().equal_fold(string()));
    EXPECT_FALSE(string().equal_fold("\xC5"));
    EXPECT_FALSE(string("\xC5").equal_fold(""));
    // a wide text by its units: a lone surrogate equals itself alone
    EXPECT_TRUE(u16string(std::u16string_view(u"\xD800", 1)).equal_fold(std::u16string_view(u"\xD800", 1)));
    EXPECT_FALSE(u16string(std::u16string_view(u"\xD800", 1)).equal_fold(std::u16string_view(u"\xD801", 1)));
}

// utf8 at the edges of each width and past the end
TEST(TextBoundary_Tests, Utf8AtTheEdgesOfEachWidth) {
    struct Case {
        std::string_view bytes;
        char32_t c;
        size_t n;
    };
    for (auto [bytes, c, n] : {
             Case{std::string_view("\0", 1), 0, 1}, Case{"\x7F", 0x7F, 1},
             Case{"\xC2\x80", 0x80, 2}, Case{"\xDF\xBF", 0x7FF, 2},
             Case{"\xE0\xA0\x80", 0x800, 3}, Case{"\xED\x9F\xBF", 0xD7FF, 3}, Case{"\xEE\x80\x80", 0xE000, 3}, Case{"\xEF\xBF\xBF", 0xFFFF, 3},
             Case{"\xF0\x90\x80\x80", 0x10000, 4}, Case{"\xF4\x8F\xBF\xBF", 0x10FFFF, 4}}) {
        EXPECT_EQ(utf8::decode(bytes), (pair<char32_t, size_t>(c, n)));
        EXPECT_EQ(utf8::decode_last(bytes), (pair<char32_t, size_t>(c, n)));
        EXPECT_EQ(utf8::width(c), n);
        EXPECT_EQ(utf8::encoded(c).view(), bytes);
        EXPECT_TRUE(utf8::valid(bytes));
        EXPECT_EQ(utf8::count(bytes), 1u);
    }
    // just past each edge: an overlong form, a surrogate, past U+10FFFF, a lead of five or six bytes
    for (std::string_view bad : {"\xC0\x80", "\xC1\xBF", "\xE0\x9F\xBF", "\xED\xA0\x80", "\xED\xBF\xBF", "\xF0\x8F\xBF\xBF",
                                 "\xF4\x90\x80\x80", "\xF7\xBF\xBF\xBF", "\xF8\x88\x80\x80\x80", "\xFC\x84\x80\x80\x80\x80", "\xFE", "\xFF"}) {
        EXPECT_EQ(utf8::decode(bad), (pair<char32_t, size_t>(utf8::replacement, 1))) << bad.size();
        EXPECT_EQ(utf8::decode_last(bad), (pair<char32_t, size_t>(utf8::replacement, 1))) << bad.size();
        EXPECT_FALSE(utf8::valid(bad));
        EXPECT_EQ(utf8::count(bad), bad.size());   // each byte one code point
    }
    // a sequence cut short, after each of its bytes
    std::string_view four = "\xF0\x9F\x98\x80";
    for (size_t n = 1; n < 4; ++n) {
        EXPECT_EQ(utf8::decode(four.substr(0, n)).second, 1u);
        EXPECT_EQ(utf8::count(four.substr(0, n)), n);
        EXPECT_FALSE(utf8::valid(four.substr(0, n)));
        EXPECT_EQ(utf8::decode_last(four, n).second, 1u);
    }
    // positions at and past the end, npos
    for (size_t at : {size_t(4), size_t(5), npos}) {
        EXPECT_EQ(utf8::decode(four, at), (pair<char32_t, size_t>(utf8::replacement, 0)));
        EXPECT_EQ(utf8::decode_last(four, at), (pair<char32_t, size_t>(U'😀', 4)));
    }
    EXPECT_EQ(utf8::decode_last(four, 0), (pair<char32_t, size_t>(utf8::replacement, 0)));
    EXPECT_EQ(utf8::decode(""), (pair<char32_t, size_t>(utf8::replacement, 0)));
    EXPECT_TRUE(utf8::valid(std::string_view()) && utf8::all_ascii(std::string_view()));
    EXPECT_EQ(utf8::count(std::string_view()), 0u);
    EXPECT_EQ(string(four).decode(npos), (pair<char32_t, size_t>(utf8::replacement, 0)));
    // values that are no code point: no width, written as U+FFFD
    for (char32_t bad : {char32_t(0xD800), char32_t(0xDFFF), char32_t(0x110000), char32_t(0xFFFFFFFF)}) {
        EXPECT_EQ(utf8::width(bad), 0u);
        EXPECT_FALSE(utf8::valid(bad));
        EXPECT_EQ(utf8::encoded(bad).view(), "�");
    }
}

// ascii_run reads eight bytes at a time: a byte over 127 at each place of
// the word and of the tail, a start at and past the end, npos
TEST(TextBoundary_Tests, AnAsciiRunAtEachPlaceAndPastTheEnd) {
    for (size_t size : {0, 1, 7, 8, 9, 15, 16, 17, 24}) {
        std::string text(size, 'a');
        EXPECT_EQ(utf8::ascii_run(text), size);
        EXPECT_TRUE(utf8::all_ascii(text));
        for (size_t high = 0; high < size; ++high) {
            std::string t = text;
            t[high] = char(0x80);
            EXPECT_EQ(utf8::ascii_run(t), high) << size << " " << high;
            EXPECT_FALSE(utf8::all_ascii(t));
            for (size_t at = 0; at <= size; ++at) {
                EXPECT_EQ(utf8::ascii_run(t, at), at <= high ? high - at : size - at) << size << " " << high << " " << at;
            }
        }
        EXPECT_EQ(utf8::ascii_run(text, size), 0u);
        EXPECT_EQ(utf8::ascii_run(text, size + 1), 0u);
        EXPECT_EQ(utf8::ascii_run(text, npos), 0u);
        EXPECT_EQ(utf8::ascii_run(text, npos - 3), 0u);
    }
}

// unicode at the edges: the first and last code point, the planes, the
// surrogates and the values past the last code point are themselves and
// no letter, no space
TEST(TextBoundary_Tests, TheUnicodeTablesAtTheirEdges) {
    for (char32_t c : {char32_t(0), char32_t(0x7F), char32_t(0x80), char32_t(0xD800), char32_t(0xDFFF), char32_t(0xFFFF),
                       char32_t(0x10FFFF), char32_t(0x110000), char32_t(0xFFFFFFFF)}) {
        EXPECT_EQ(unicode::to_lower(c), c);
        EXPECT_EQ(unicode::to_upper(c), c);
        EXPECT_FALSE(unicode::is_lower(c) || unicode::is_upper(c) || unicode::is_space(c));
        EXPECT_TRUE(unicode::equal_fold(c, c));
    }
    EXPECT_FALSE(unicode::equal_fold(char32_t(0xD800), char32_t(0xDC00)));
    EXPECT_FALSE(unicode::equal_fold(char32_t(0x110000), char32_t(0x110020)));
    // the letters past the Basic Multilingual Plane: Deseret
    EXPECT_EQ(unicode::to_lower(U'\U00010400'), U'\U00010428');
    EXPECT_EQ(unicode::to_upper(U'\U00010428'), U'\U00010400');
    EXPECT_TRUE(unicode::equal_fold(U'\U00010400', U'\U00010428'));
    EXPECT_EQ(string("\U00010400").to_lower(), "\U00010428");
    EXPECT_TRUE(string("\U00010400").equal_fold("\U00010428"));
    // the white space at the edges of its ranges, and the characters
    // beside them that are not
    for (char32_t c : {U'\t', U'\r', U' ', U'\u0085', U' ', U' ', U' ', U' ', U' ', U' ', U' ', U' ', U'　'}) {
        EXPECT_TRUE(unicode::is_space(c)) << uint32_t(c);
    }
    for (char32_t c : {U'\b', U'\x0E', U'\x1F', U'\u0084', U'\u0086', U'᠎', U'῿', U'​', U'‧', U'‪', U'⁠', U'﻿'}) {
        EXPECT_FALSE(unicode::is_space(c)) << uint32_t(c);
    }
}

// runes: the default and the empty range, ill-formed bytes and a slice
// that begins inside a code point, the positions and widths, the end
TEST(TextBoundary_Tests, TheRunesAtTheirBoundaries) {
    runes none;
    EXPECT_TRUE(none.empty() && none.begin() == none.end());
    EXPECT_EQ(none.count(), 0u);
    EXPECT_TRUE(none.text().empty());
    EXPECT_EQ(runes::iterator(), runes::iterator());
    string s = "\xC5\x80\x80\xF0\x9F\x98";   // U+0140, a stray continuation byte, a sequence cut short
    std::vector<std::tuple<char32_t, size_t, size_t>> seen;
    auto r = s.runes();
    for (auto it = r.begin(); it != r.end(); ++it) {
        seen.emplace_back(*it, it.pos(), it.width());
    }
    EXPECT_EQ(seen, (std::vector<std::tuple<char32_t, size_t, size_t>>{
        {U'ŀ', 0, 2}, {utf8::replacement, 2, 1}, {utf8::replacement, 3, 1}, {utf8::replacement, 4, 1}, {utf8::replacement, 5, 1}}));
    EXPECT_EQ(r.count(), 5u);
    EXPECT_EQ(s.rune_count(), 5u);
    EXPECT_EQ(r.end().width(), 0u);
    EXPECT_EQ(r.end().pos(), s.size());
    // a slice that begins inside a code point: its first byte alone is one
    auto inside = string("żx").as_slice(1);
    std::u32string got;
    for (char32_t c : inside.runes()) {
        got += c;
    }
    EXPECT_EQ(got, U"�x");
    // a u8string has runes too, and an empty string none
    std::u32string eight;
    for (char32_t c : u8string(u8"ż😀").runes()) {
        eight += c;
    }
    EXPECT_EQ(eight, U"ż😀");
    EXPECT_TRUE(u8string().runes().empty());
    // a moved-from range walks the same text: it holds a slice
    auto moved = std::move(r);
    EXPECT_EQ(moved.count(), 5u);
    EXPECT_EQ(r.count(), 5u);
}

namespace {
    using Pieces = std::vector<std::string>;

    template<class R>
    Pieces collect(R&& pieces) {
        Pieces out;
        for (auto piece : pieces) {
            out.emplace_back(piece.data(), piece.size());
        }
        return out;
    }
}

// The length is kept in 32 bits: every way of making a string past it is
// refused with length_error before a character is read or written, and
// the counts whose sum or product passes 64 bits (a lazy range of one
// large view, a long replacement times many occurrences) are refused, not
// wrapped to a small length. A string of exactly max_size() characters
// takes 4 GiB and is not made here.
TEST(StringBoundary_Tests, TheLengthLimitAndItsArithmetic) {
    static_assert(string::max_size() == UINT32_MAX && u32string::max_size() == UINT32_MAX);
    const char c = 'x';
    const size_t past = size_t(string::max_size()) + 1;
    EXPECT_THROW(string(past, 'x'), length_error);
    EXPECT_THROW(string(npos, 'x'), length_error);
    EXPECT_THROW(u32string(past, U'x'), length_error);
    EXPECT_THROW(string(&c, past), length_error);
    auto counted = std::views::iota(size_t(0), past) | std::views::transform([](size_t) { return 'x'; });
    EXPECT_THROW(string(counted.begin(), counted.end()), length_error);
    const std::string_view at_limit(&c, string::max_size());   // never read: refused by the length it makes
    const std::string_view beyond(&c, past);
    string one = "a";
    EXPECT_THROW(one + at_limit, length_error);
    EXPECT_THROW(at_limit + one, length_error);
    EXPECT_THROW(string::concat(one, at_limit), length_error);
    EXPECT_THROW(string::concat(at_limit, 'x'), length_error);
    EXPECT_THROW(string::join(std::vector<std::string_view>{at_limit, "a"}, ""), length_error);
    EXPECT_THROW(string::join(std::vector<std::string_view>{"a", "b"}, at_limit), length_error);
    EXPECT_THROW(one.replace("a", beyond), length_error);
    EXPECT_THROW(string("ab").repeat(size_t(string::max_size()) / 2 + 1), length_error);
    EXPECT_THROW(one.repeat(past), length_error);
    EXPECT_THROW(one.repeat(npos), length_error);
    EXPECT_TRUE(string().repeat(npos).empty());   // nothing repeated is nothing, at any count
    // sums and products past 64 bits
    const std::string_view vast(&c, size_t(1) << 62);
    EXPECT_THROW(string("aaaa").replace("a", vast), length_error);                // 4 x 2^62
    EXPECT_THROW(string("aaaa").replace('a', 'b').replace("b", vast, 4), length_error);
    auto four = std::views::iota(0, 4) | std::views::transform([&](int) { return vast; });
    EXPECT_THROW(string::join(four, ""), length_error);                           // 4 x 2^62 of parts
    auto five = std::views::iota(0, 5) | std::views::transform([](int) { return std::string_view(); });
    EXPECT_THROW(string::join(five, vast), length_error);                         // 4 separators of 2^62
    EXPECT_EQ(string::join(five, ""), "");
    // a separator past the limit that is never written: no limit passed
    EXPECT_EQ(string::join(std::vector<std::string_view>{"a"}, vast), "a");
    EXPECT_EQ(string::join(sgcl::vector<string>{"a"}, beyond), "a");
}

// Positions at and past the end, and npos
TEST(StringBoundary_Tests, PositionsAtAndPastTheEnd) {
    string s = "żółw";
    EXPECT_TRUE(s.substr(s.size()).empty());
    EXPECT_THROW(s.substr(s.size() + 1), out_of_range);
    EXPECT_THROW(s.substr(npos), out_of_range);
    EXPECT_EQ(s.substr(0, npos).object(), s.object());
    EXPECT_EQ(s.substr(1, npos).size(), s.size() - 1);
    EXPECT_TRUE(s.as_slice(s.size()).empty());
    EXPECT_THROW(s.as_slice(s.size() + 1), out_of_range);
    EXPECT_THROW(s.as_slice(npos), out_of_range);
    EXPECT_EQ(s.as_slice(2, npos).size(), s.size() - 2);
    EXPECT_THROW(s.at(s.size()), out_of_range);
    EXPECT_EQ(s[s.size()], '\0');   // the terminator
    char buf[4];
    EXPECT_EQ(s.copy(buf, 4, s.size()), 0u);
    EXPECT_THROW(s.copy(buf, 4, s.size() + 1), out_of_range);
    EXPECT_THROW(s.compare(s.size() + 1, 1, "x"), out_of_range);
    EXPECT_EQ(s.compare(s.size(), npos, ""), 0);
    // an empty pattern: found at every position up to the end, never past it
    for (size_t pos : {size_t(0), size_t(3), s.size()}) {
        EXPECT_EQ(s.find("", pos), pos);
        EXPECT_EQ(s.find(std::string_view(), pos), pos);
        EXPECT_EQ(s.rfind("", pos), pos);
        EXPECT_EQ(s.find_first_of("", pos), npos);
        EXPECT_EQ(s.find_last_of("", pos), npos);
        EXPECT_EQ(s.find_first_not_of("", pos), pos == s.size() ? npos : pos);
        EXPECT_EQ(s.find_last_not_of("", pos), pos == s.size() ? pos - 1 : pos);
    }
    EXPECT_EQ(s.find("", s.size() + 1), npos);
    EXPECT_EQ(s.find("", npos), npos);
    EXPECT_EQ(s.rfind("", npos), s.size());
    EXPECT_TRUE(s.contains("") && s.starts_with("") && s.ends_with(""));
    EXPECT_TRUE(string().contains("") && string().starts_with("") && string().ends_with(""));
    EXPECT_EQ(string().find(""), 0u);
    EXPECT_EQ(string().rfind(""), 0u);
    EXPECT_EQ(s.find("w", s.size()), npos);
    EXPECT_EQ(s.rfind("ż", 0), 0u);
    EXPECT_EQ(s.find('w', npos), npos);
    EXPECT_EQ(s.rfind('w', npos), s.size() - 1);
}

namespace {
    // The empty string, a moved-from one and one assigned to itself, of
    // every character type
    template<class CharT>
    void empty_and_moved() {
        using S = basic_string<CharT>;
        using V = std::basic_string_view<CharT>;
        S e;
        EXPECT_TRUE(e.empty());
        EXPECT_EQ(e.size(), 0u);
        EXPECT_EQ(e.length(), 0u);
        ASSERT_NE(e.data(), nullptr);
        EXPECT_EQ(e.data()[0], CharT());
        EXPECT_EQ(e.c_str(), e.data());
        EXPECT_EQ(e[0], CharT());
        EXPECT_TRUE(e.begin() == e.end() && e.rbegin() == e.rend());
        EXPECT_EQ(e.object(), nullptr);
        EXPECT_EQ(e.hash(), S::hash_of(V()));
        EXPECT_EQ(std::hash<S>()(e), std::hash<S>()(V()));
        EXPECT_TRUE(e == S() && e == V() && (e <=> S()) == 0);
        EXPECT_EQ(e.compare(V()), 0);
        EXPECT_EQ(e.find(V()), 0u);
        EXPECT_EQ(e.rfind(V()), 0u);
        EXPECT_EQ(e.find(CharT('a')), npos);
        EXPECT_TRUE(e.split(CharT(',')).empty() && e.split(V()).empty() && e.fields().empty());
        EXPECT_TRUE(e.trim().empty() && e.trim_left().empty() && e.trim_right().empty());
        EXPECT_TRUE(e.trim_prefix(V()).empty() && e.trim_suffix(V()).empty());
        EXPECT_TRUE(e.to_lower().empty() && e.to_upper().empty());
        EXPECT_TRUE(e.repeat(3).empty() && e.substr().empty() && e.as_slice().empty());
        EXPECT_EQ(e.rune_count(), 0u);
        EXPECT_TRUE(e.equal_fold(V()));
        EXPECT_EQ(e.str(), std::basic_string<CharT>());
        EXPECT_TRUE(S::join(sgcl::vector<S>{}, CharT(',')).empty());
        EXPECT_TRUE((e + e).empty() && S::concat(e, e).empty());
        EXPECT_TRUE(S(e.as_slice()).empty());
        EXPECT_TRUE(e.replace(V(), V()).empty());
        // every way of making no characters makes the empty string, null
        EXPECT_EQ(S(0, CharT('x')).object(), nullptr);
        EXPECT_EQ(S(std::initializer_list<CharT>{}).object(), nullptr);
        EXPECT_EQ(S(e.data(), 0).object(), nullptr);
        std::basic_string<CharT> none;
        EXPECT_EQ(S(none.begin(), none.end()).object(), nullptr);
        // a move is a copy of the word: the moved-from string keeps its value
        S s = text_of<CharT>(U"Ab😀");
        S moved = std::move(s);
        EXPECT_EQ(s, moved);
        EXPECT_EQ(s.object(), moved.object());
        S assigned;
        assigned = std::move(moved);
        EXPECT_EQ(moved, assigned);
        S empty_moved = std::move(e);
        EXPECT_TRUE(e.empty() && empty_moved.empty());
        // assigned, moved and swapped with itself: unchanged
        S self = text_of<CharT>(U"self");
        const void* object = self.object();
        S& alias = self;
        self = alias;
        EXPECT_EQ(self.object(), object);
        self = std::move(alias);
        EXPECT_EQ(self.object(), object);
        self.swap(alias);
        EXPECT_EQ(self.object(), object);
        swap(self, alias);
        EXPECT_EQ(self.object(), object);
        EXPECT_EQ(self, text_of<CharT>(U"self"));
    }
}

TEST(StringBoundary_Tests, TheEmptyStringAndAMovedFromOneOfEveryCharacterType) {
    empty_and_moved<char>();
    empty_and_moved<char8_t>();
    empty_and_moved<wchar_t>();
    empty_and_moved<char16_t>();
    empty_and_moved<char32_t>();
}

// A literal is read to its first NUL; a view, a std::string and a
// character NUL are characters like any other
TEST(StringBoundary_Tests, ANulInsideALiteral) {
    string cut = "a\0b";
    EXPECT_EQ(cut.size(), 1u);
    string whole(std::string_view("a\0b", 3));
    EXPECT_EQ(whole.size(), 3u);
    EXPECT_EQ(string(std::string("a\0b", 3)).size(), 3u);
    EXPECT_TRUE(cut == "a\0b");          // the literal again to its NUL
    EXPECT_FALSE(whole == "a\0b");
    EXPECT_TRUE(whole == std::string_view("a\0b", 3));
    EXPECT_NE(whole, cut);
    EXPECT_LT(cut, whole);
    EXPECT_NE(whole.hash(), cut.hash());
    EXPECT_EQ(std::hash<string>()("a\0b"), cut.hash());
    EXPECT_EQ(whole.find('\0'), 1u);
    EXPECT_EQ(whole.find(U'\0'), 1u);
    EXPECT_EQ(whole.find_first_of(U'\0'), 1u);
    EXPECT_TRUE(whole.contains(std::string_view("\0b", 2)));
    EXPECT_EQ(whole.find("\0b"), 0u);   // the literal is "": found where the search starts
    EXPECT_EQ(whole.find("b\0c"), 2u);  // the literal is "b"
    EXPECT_TRUE(whole.starts_with("\0"));
    EXPECT_EQ(collect(whole.split('\0')), (Pieces{"a", "b"}));
    EXPECT_EQ(collect(whole.split("\0")), (Pieces{"a", std::string(1, '\0'), "b"}));   // "": the code points
    EXPECT_EQ(whole.replace("\0", "x").object(), whole.object());   // an empty `from` changes nothing
    EXPECT_EQ(whole.replace('\0', 'x'), "axb");
    EXPECT_EQ(whole.replace(U'\0', U'ż'), "ażb");
    EXPECT_EQ(string(std::string_view("\0a\0", 3)).trim(std::string_view("\0", 1)), "a");
    EXPECT_EQ(whole.trim("\0").object(), whole.object());   // an empty set trims nothing
    EXPECT_EQ(whole.trim_prefix("a\0x"), std::string_view("\0b", 2));
    EXPECT_EQ(whole + "c\0d", std::string_view("a\0bc", 4));
    EXPECT_EQ(string::concat(whole, "\0z"), whole);
    EXPECT_EQ(string::concat(whole, '\0').size(), 4u);
    EXPECT_EQ(string::join(sgcl::vector<string>{"x", "y"}, "\0-"), "xy");
    EXPECT_EQ(string::join(sgcl::vector<string>{"x", "y"}, '\0').size(), 3u);
    // a NUL is not white space
    EXPECT_EQ(string(std::string_view("\0 a \0", 5)).trim().size(), 5u);
    EXPECT_EQ(collect(string(std::string_view("\0 \0", 3)).fields()).size(), 2u);
    // the wide strings the same
    EXPECT_EQ(u16string(u"a\0b").size(), 1u);
    u16string wide(std::u16string_view(u"a\0b", 3));
    EXPECT_EQ(wide.size(), 3u);
    EXPECT_EQ(wide.find(U'\0'), 1u);
    EXPECT_EQ(u32string(std::u32string_view(U"a\0b", 3)).find(U'\0'), 1u);
}

// A string made from itself: appended, concatenated, replaced in, joined
// with and assigned a view or a slice of itself, at sizes across the size
// classes and past them (the old object is read while the new is written)
TEST(StringBoundary_Tests, AStringMadeFromItself) {
    for (size_t n : {1, 7, 100, 250, 1000, 5000}) {
        std::string base;
        for (size_t i = 0; i < n; ++i) {
            base += char('a' + i % 26);
        }
        const std::string twice = base + base;
        SCOPED_TRACE(n);
        string s(base);
        s = s + s;
        EXPECT_EQ(s, twice);
        s = string(base);
        s = s + s.view();
        EXPECT_EQ(s, twice);
        s = string(base);
        s = string::concat(s, s.as_slice(), s.view(), '!');
        EXPECT_EQ(s, base + base + base + "!");
        s = string(base);
        s = s.replace(s, s + s);
        EXPECT_EQ(s, twice);
        s = string(base);
        s = s.replace(s.view().substr(0, 1), s);
        std::string expected;
        for (char ch : base) {
            expected += ch == base[0] ? base : std::string(1, ch);
        }
        EXPECT_EQ(s, expected);
        s = string(base);
        EXPECT_TRUE(s.replace(s, "").empty());
        s = s.view().substr(1);
        EXPECT_EQ(s, base.substr(1));
        s = string(base);
        s = s.as_slice(1);
        EXPECT_EQ(s, base.substr(1));
        s = string(base);
        s = s.repeat(2);
        EXPECT_EQ(s, twice);
        s = string(base);
        EXPECT_TRUE(s.trim_prefix(s).empty() && s.trim_suffix(s).empty());
        s = string::join(s.split(s.view().substr(0, 1)), s.view().substr(0, 1));
        EXPECT_EQ(s, base);
        s = string::join(sgcl::vector<string>{s, s}, s);
        EXPECT_EQ(s, base + base + base);
        s = string(base);
        EXPECT_EQ(collect(s.split(s)), (Pieces{"", ""}));
        EXPECT_EQ(s.substr(0).object(), s.object());
        EXPECT_TRUE(s.equal_fold(s) && s.compare(s) == 0 && s.starts_with(s) && s.ends_with(s) && s.find(s) == 0);
    }
}

// split and join with empty pieces: where separators meet, begin and end
// the string, the separator the whole string, longer than the string;
// max_parts at 1, at the count and past it; join of no part, of empty
// parts, with an empty separator; join of split is the string again
TEST(StringBoundary_Tests, SplitAndJoinWithEmptyPieces) {
    EXPECT_EQ(collect(string(",").split(',')), (Pieces{"", ""}));
    EXPECT_EQ(collect(string(",,").split(',')), (Pieces{"", "", ""}));
    EXPECT_EQ(collect(string("a").split("a")), (Pieces{"", ""}));
    EXPECT_EQ(collect(string("ab").split("abc")), (Pieces{"ab"}));
    EXPECT_EQ(collect(string(",").split(',', 1)), (Pieces{","}));
    EXPECT_EQ(collect(string(",,").split(',', 2)), (Pieces{"", ","}));
    EXPECT_EQ(collect(string("a,b").split(',', 100)), (Pieces{"a", "b"}));
    EXPECT_EQ(collect(string("a,b").split(',', npos)), (Pieces{"a", "b"}));
    EXPECT_EQ(collect(string("żó").split("", 1)), (Pieces{"żó"}));
    EXPECT_EQ(collect(string("żó").split("", 2)), (Pieces{"ż", "ó"}));
    EXPECT_EQ(collect(string("żó").split("", 3)), (Pieces{"ż", "ó"}));
    EXPECT_EQ(collect(string("\xC5x").split("")), (Pieces{"\xC5", "x"}));   // an ill-formed byte a piece of its own
    EXPECT_EQ(collect(string("a😀b").split(U'😀', 1)), (Pieces{"a😀b"}));
    // the separator kept inside up to 16 bytes, in a string of its own past them
    const std::string sep16(16, '-'), sep17(17, '-');
    EXPECT_EQ(collect(string("a" + sep16 + "b" + sep16).split(std::string_view(sep16))), (Pieces{"a", "b", ""}));
    EXPECT_EQ(collect(string("a" + sep17 + "b").split(std::string_view(sep17))), (Pieces{"a", "b"}));
    std::vector<size_t> sizes;
    for (auto piece : u32string(U"a----b-----c").split(std::u32string_view(U"-----"))) {   // past the 4 kept inside
        sizes.push_back(piece.size());
    }
    EXPECT_EQ(sizes, (std::vector<size_t>{6, 1}));
    // join
    EXPECT_TRUE(string::join(sgcl::vector<string>{}, ",").empty());
    EXPECT_TRUE(string::join(sgcl::vector<string>{""}, ",").empty());
    EXPECT_EQ(string::join(sgcl::vector<string>{"", ""}, ","), ",");
    EXPECT_EQ(string::join(sgcl::vector<string>{"", "", ""}, ""), "");
    EXPECT_EQ(string::join(std::vector<std::string_view>{"a", "", "b"}, ""), "ab");
    EXPECT_EQ(string::join(sgcl::vector<string>{"a"}, std::string_view(sep17)), "a");
    std::istringstream words("x y");
    EXPECT_EQ(string::join(std::views::istream<std::string>(words), ","), "x,y");   // a single pass
    std::istringstream nothing("");
    EXPECT_TRUE(string::join(std::views::istream<std::string>(nothing), ",").empty());
    for (std::string_view text : {"", ",", ",,", "a,", ",a", "a,,b", ",,a,,"}) {
        string s(text);
        EXPECT_EQ(string::join(s.split(','), ','), s) << text;
        EXPECT_EQ(string::join(s.split(","), ","), s) << text;
        EXPECT_EQ(string::join(s.split(U','), U','), s) << text;
    }
    // fields: none of white space alone, none empty, ill-formed bytes no space
    EXPECT_TRUE(string("   ").fields().empty());
    EXPECT_TRUE(string("　 \t").fields().empty());
    EXPECT_EQ(collect(string(" a ").fields()), (Pieces{"a"}));
    EXPECT_EQ(collect(string("　a b ").fields()), (Pieces{"a", "b"}));
    EXPECT_EQ(collect(string("\xC5 \x80").fields()), (Pieces{"\xC5", "\x80"}));
    // the pieces as a value: a copy and a moved-from range walk the same
    auto p = string("a,b").split(',');
    auto copy = p;
    auto moved = std::move(p);
    EXPECT_EQ(collect(copy), (Pieces{"a", "b"}));
    EXPECT_EQ(collect(moved), (Pieces{"a", "b"}));
    EXPECT_EQ(collect(p), (Pieces{"a", "b"}));
    EXPECT_EQ(moved.text(), "a,b");
    EXPECT_TRUE(string().split(',').text().empty());
    EXPECT_EQ(string::pieces::iterator(), string::pieces::iterator());
}

// A slice of a string (string_slice) and the empty slice
TEST(StringBoundary_Tests, TheSlicesOfAString) {
    string s = "żółw";
    string_slice none;
    EXPECT_TRUE(none.empty());
    EXPECT_TRUE(string(none).empty());
    EXPECT_EQ(std::hash<string_slice>()(none), string().hash());
    EXPECT_TRUE(none == string());
    EXPECT_TRUE(none.trim().empty() && none.trim(U"x").empty() && none.trim_prefix("x").empty());
    EXPECT_EQ(none.rune_count(), 0u);
    EXPECT_TRUE(none.runes().empty());
    EXPECT_TRUE(none.is_valid_utf8());
    // an empty slice of a string holds it and makes the empty string
    auto end = s.as_slice(s.size());
    EXPECT_TRUE(end.empty());
    EXPECT_EQ(end.owner().get(), s.object());
    EXPECT_EQ(string(end).object(), nullptr);
    // the whole slice is the same object again; a part is a copy
    EXPECT_EQ(string(s.as_slice()).object(), s.object());
    EXPECT_EQ(string(s.as_slice(0, npos)).object(), s.object());
    EXPECT_NE(string(s.as_slice(0, s.size() - 1)).object(), s.object());
    EXPECT_EQ(string(s.as_slice(2)), "ółw");
    EXPECT_TRUE(s.as_slice().subslice(s.size()).empty());
    EXPECT_THROW(s.as_slice().subslice(s.size() + 1), out_of_range);
    EXPECT_THROW(s.as_slice().substr(npos), out_of_range);
    // a slice that cuts a code point is a text of its own, ill-formed at the cut
    auto cut = s.as_slice(1, 2);   // the second byte of ż, the first of ó
    EXPECT_FALSE(cut.is_valid_utf8());
    EXPECT_EQ(cut.rune_count(), 2u);
    EXPECT_EQ(string(cut).size(), 2u);
    // compared and hashed as a string of its characters
    EXPECT_EQ(std::hash<string_slice>()(s.as_slice(2)), string("ółw").hash());
    EXPECT_TRUE(s.as_slice(2) == string("ółw"));
    EXPECT_EQ(s <=> s.as_slice(), std::strong_ordering::equal);
    EXPECT_EQ(s <=> s.as_slice(0, 2), std::strong_ordering::greater);
    // trimmed: a slice of the same owner, empty when all of it goes
    string padded = "  x  ";
    auto t = padded.as_slice().trim();
    EXPECT_EQ(t, "x");
    EXPECT_EQ(t.owner().get(), padded.object());
    EXPECT_TRUE(string("   ").as_slice().trim().empty());
    EXPECT_TRUE(string("«»").as_slice().trim(U"«»").empty());
}

// The hash at its edges: the empty string's constant, a NUL, lengths
// around the steps of hash_bytes
TEST(StringBoundary_Tests, TheHashAtItsEdges) {
    EXPECT_EQ(string().hash(), string::hash_of(""));
    EXPECT_EQ(string().hash(), string::hash_of(std::string_view()));
    EXPECT_EQ(string().hash(), std::hash<string>()(""));
    EXPECT_EQ(std::hash<string>()("\0"), string().hash());   // the literal to its NUL
    string nul(std::string_view("\0", 1));
    EXPECT_NE(nul.hash(), string().hash());
    EXPECT_EQ(nul.hash(), string::hash_of(std::string_view("\0", 1)));
    EXPECT_EQ(detail::hash_bytes(nullptr, 0), detail::hash_bytes("", 0));   // no byte read
    for (size_t n : {1, 3, 4, 7, 8, 15, 16, 17, 47, 48, 49, 96, 97, 1000}) {
        std::string t(n, 'q');
        t[n / 2] = 'r';
        string a(t), b(t);
        EXPECT_NE(a.object(), b.object());
        EXPECT_EQ(a.hash(), b.hash()) << n;
        EXPECT_EQ(a.hash(), string::hash_of(t)) << n;
        EXPECT_EQ(a.hash(), a.hash()) << n;
        EXPECT_EQ(a, b);
    }
    EXPECT_EQ(u16string(u"ż").hash(), u16string::hash_of(u"ż"));
    EXPECT_EQ(u32string(U"😀").hash(), std::hash<u32string>()(U"😀"));
}

// The case of a text whose letters change width, of ill-formed bytes (kept
// as they are), of the wide strings (a code point at a time, a surrogate
// pair one)
TEST(StringBoundary_Tests, TheCaseAtItsEdges) {
    EXPECT_EQ(string("Ⱥ").to_lower(), "ⱥ");          // two bytes to three
    EXPECT_EQ(string("K").to_lower(), "k");     // the Kelvin sign: three bytes to one
    EXPECT_EQ(string("\xC5" "A\x80").to_lower(), "\xC5" "a\x80");
    EXPECT_EQ(string("ß").to_upper(), "ß");
    string lower = "żółw";
    EXPECT_EQ(lower.to_lower().object(), lower.object());
    string ascii = "abc";
    EXPECT_EQ(ascii.to_lower().object(), ascii.object());
    EXPECT_EQ(u16string(u"ŁÓDŹ😀").to_lower(), u"łódź😀");
    EXPECT_EQ(u16string(u"\U00010400").to_lower(), u"\U00010428");   // a surrogate pair is one letter
    EXPECT_EQ(u32string(U"\U00010400").to_lower(), U"\U00010428");
    EXPECT_EQ(wstring(L"\U00010400").to_lower(), L"\U00010428");
}

namespace {
    // The code points of UTF-16 units as the case functions take them: a
    // surrogate pair one, any other unit (a lone surrogate included) its
    // own value, as a u32string holds it
    std::u32string points_of(std::u16string_view units) {
        std::u32string out;
        for (size_t i = 0; i < units.size(); ++i) {
            char32_t u = units[i];
            if (u >= 0xD800 && u < 0xDC00 && i + 1 < units.size() && units[i + 1] >= 0xDC00 && units[i + 1] < 0xE000) {
                out.push_back(0x10000 + ((u - 0xD800) << 10) + (char32_t(units[i + 1]) - 0xDC00));
                ++i;
            } else {
                out.push_back(u);
            }
        }
        return out;
    }

    // The case of UTF-16 units against the case of the same code points in
    // UTF-32 (and in UTF-8 when the units are well-formed): the same code
    // points, the units of each kept where they were
    void same_case_in_every_encoding(std::u16string_view units, bool well_formed = true) {
        SCOPED_TRACE(testing::Message() << "units " << units.size());
        u16string s{units};
        std::u32string points = points_of(units);
        u32string wide{std::u32string_view(points)};
        EXPECT_EQ(points_of(s.to_lower().view()), wide.to_lower().view());
        EXPECT_EQ(points_of(s.to_upper().view()), wide.to_upper().view());
        EXPECT_EQ(s.to_lower().size(), s.size());
        EXPECT_EQ(s.to_upper().size(), s.size());
        if (well_formed) {
            EXPECT_EQ(text_of<char>(points).to_lower(), text_of<char>(wide.to_lower().view()));
            EXPECT_EQ(text_of<char>(points).to_upper(), text_of<char>(wide.to_upper().view()));
        }
        // a text is its own lower and upper case, and its case changes are
        // each other's
        EXPECT_TRUE(s.equal_fold(s));
        EXPECT_TRUE(s.equal_fold(s.to_lower()));
        EXPECT_TRUE(s.equal_fold(s.to_upper()));
        EXPECT_TRUE(s.to_lower().equal_fold(s.to_upper()));
        EXPECT_TRUE(s.to_upper().equal_fold(s));
    }
}

// u16string's case is the case of its code points: a surrogate pair is one
// letter (Deseret, Osage, Adlam and the other cased scripts past U+FFFF),
// mapped as UTF-8 and UTF-32 map it; a lone surrogate is left as it is, as
// an ill-formed byte of UTF-8 is
TEST(StringBoundary_Tests, TheCaseOfUtf16SurrogatePairs) {
    // Deseret, Osage, Old Hungarian, Warang Citi, Medefaidrin, Adlam, Vithkuqi, Garay
    const std::u32string upper = U"\U00010400\U000104B0\U00010C80\U000118A0\U00016E40\U0001E900\U00010570\U00010D50";
    const std::u32string lower = U"\U00010428\U000104D8\U00010CC0\U000118C0\U00016E60\U0001E922\U00010597\U00010D70";
    for (size_t k = 0; k < upper.size(); ++k) {
        SCOPED_TRACE(testing::Message() << "U+" << std::hex << uint32_t(upper[k]));
        auto big = text_of<char16_t>(std::u32string_view(&upper[k], 1));
        auto small = text_of<char16_t>(std::u32string_view(&lower[k], 1));
        ASSERT_EQ(big.size(), 2u);
        EXPECT_EQ(big.to_lower(), small);
        EXPECT_EQ(small.to_upper(), big);
        EXPECT_EQ(big.to_upper().object(), big.object());
        EXPECT_EQ(small.to_lower().object(), small.object());
        EXPECT_TRUE(big.equal_fold(small));
        EXPECT_TRUE(small.equal_fold(big));
    }
    EXPECT_EQ(text_of<char16_t>(upper).to_lower(), text_of<char16_t>(lower));
    EXPECT_EQ(text_of<char16_t>(lower).to_upper(), text_of<char16_t>(upper));
    // a pair at the start, in the middle and at the end, beside ASCII and
    // the letters of the Basic Multilingual Plane
    EXPECT_EQ(u16string(u"𐐀bc").to_lower(), u"𐐨bc");
    EXPECT_EQ(u16string(u"aŁ𐐀ż").to_upper(), u"AŁ𐐀Ż");
    EXPECT_EQ(u16string(u"aŁ𐐀ż").to_lower(), u"ał𐐨ż");
    EXPECT_EQ(u16string(u"ABC𐒰").to_lower(), u"abc𐓘");
    EXPECT_EQ(u16string(u"𐐀𐐨𞤀").to_lower(), u"𐐨𐐨𞤢");
    EXPECT_EQ(u16string(u"Zażółć 𐐀 GĘŚLĄ 𞤀 jaźń 𐒰").to_upper(), u"ZAŻÓŁĆ 𐐀 GĘŚLĄ 𞤀 JAŹŃ 𐒰");
    EXPECT_EQ(u16string(u"Zażółć 𐐀 GĘŚLĄ 𞤀 jaźń 𐒰").to_lower(), u"zażółć 𐐨 gęślą 𞤢 jaźń 𐓘");
    // a pair without case, and nothing to change: the same object
    u16string emoji = u"😀ab😀";
    EXPECT_EQ(emoji.to_lower().object(), emoji.object());
    EXPECT_EQ(emoji.to_upper(), u"😀AB😀");
    for (std::u16string_view text : {std::u16string_view(u"𐐀bc"), std::u16string_view(u"aŁ𐐀ż"), std::u16string_view(u"ABC𐒰"),
                                      std::u16string_view(u"𐐀𐐨𞤀"), std::u16string_view(u"Zażółć 𐐀 GĘŚLĄ 𞤀 jaźń 𐒰"),
                                      std::u16string_view(u"😀ab😀"), std::u16string_view(u"")}) {
        same_case_in_every_encoding(text);
    }
    // the lone surrogates: a high one at the end, after a pair, before a
    // letter, before another high one; a low one at the start, after a
    // letter; a low one before a high one (no pair)
    const std::u16string_view lone[] = {
        std::u16string_view(u"A𐐀\xD801", 4),
        std::u16string_view(u"\xD801", 1),
        std::u16string_view(u"\xD801" "A", 2),
        std::u16string_view(u"\xD801" "\xD801" "\xDC00", 3),
        std::u16string_view(u"\xDC00" "A", 2),
        std::u16string_view(u"A\xDC00", 2),
        std::u16string_view(u"\xDC00" "\xD801" "A", 3),
        std::u16string_view(u"\xDC28" "\xDC00" "𐐀", 4),
    };
    for (auto units : lone) {
        same_case_in_every_encoding(units, false);
    }
    EXPECT_EQ(u16string(lone[0]).to_lower().view(), std::u16string_view(u"a𐐨\xD801", 4));
    EXPECT_EQ(u16string(lone[2]).to_lower().view(), std::u16string_view(u"\xD801" "a", 2));
    EXPECT_EQ(u16string(lone[3]).to_lower().view(), std::u16string_view(u"\xD801" "\xD801" "\xDC28", 3));
    EXPECT_EQ(u16string(lone[4]).to_lower().view(), std::u16string_view(u"\xDC00" "a", 2));
    EXPECT_EQ(u16string(lone[6]).to_lower().view(), std::u16string_view(u"\xDC00" "\xD801" "a", 3));
    EXPECT_EQ(u16string(lone[7]).to_lower().view(), std::u16string_view(u"\xDC28" "\xDC00" "𐐨", 4));
    u16string alone{lone[1]};
    EXPECT_EQ(alone.to_lower().object(), alone.object());
    EXPECT_EQ(alone.to_upper().object(), alone.object());
}

// equal_fold of UTF-16 by code points: a pair equals the other case of its
// letter and no other letter; a lone surrogate equals only itself
TEST(StringBoundary_Tests, EqualFoldOfUtf16SurrogatePairs) {
    EXPECT_TRUE(u16string(u"𐐀").equal_fold(u"𐐨"));
    EXPECT_TRUE(u16string(u"x𐐀Ł").equal_fold(u"X𐐨ł"));
    EXPECT_TRUE(u16string(u"𞤀𐒰").equal_fold(u"𞤢𐓘"));
    EXPECT_FALSE(u16string(u"𐐀").equal_fold(u"𐐩"));      // the next letter
    EXPECT_FALSE(u16string(u"𐐀").equal_fold(u"𞤢"));
    EXPECT_FALSE(u16string(u"𐐀").equal_fold(u"𐐀𐐀"));
    EXPECT_FALSE(u16string(u"a𐐀").equal_fold(u"A𐐨b"));
    EXPECT_FALSE(u16string(u"𐐀").equal_fold(u"a"));
    // a lone high surrogate before a letter, at the end
    EXPECT_TRUE(u16string(std::u16string_view(u"\xD801" "A", 2)).equal_fold(std::u16string_view(u"\xD801" "a", 2)));
    EXPECT_TRUE(u16string(std::u16string_view(u"𐐀\xD801", 3)).equal_fold(std::u16string_view(u"𐐨\xD801", 3)));
    EXPECT_FALSE(u16string(std::u16string_view(u"𐐀\xD801", 3)).equal_fold(std::u16string_view(u"𐐨\xD802", 3)));
    // a lone low surrogate equals only itself, not the low half of the
    // other case of a letter
    EXPECT_FALSE(u16string(std::u16string_view(u"\xDC00", 1)).equal_fold(std::u16string_view(u"\xDC28", 1)));
    EXPECT_FALSE(u16string(std::u16string_view(u"a\xDC00", 2)).equal_fold(std::u16string_view(u"A\xDC28", 2)));
    EXPECT_TRUE(u16string(std::u16string_view(u"a\xDC00", 2)).equal_fold(std::u16string_view(u"A\xDC00", 2)));
    // a pair on one side, a lone high surrogate and a pair one unit on on
    // the other: the same units but one, never equal
    EXPECT_FALSE(u16string(std::u16string_view(u"𐐀x", 3)).equal_fold(std::u16string_view(u"\xD801" "𐐨", 3)));
    EXPECT_FALSE(u16string(std::u16string_view(u"\xD801" "𐐨", 3)).equal_fold(std::u16string_view(u"𐐀x", 3)));
    // a pair against its halves reversed
    EXPECT_FALSE(u16string(u"𐐀").equal_fold(std::u16string_view(u"\xDC28" "\xD801", 2)));
    // the slice compares as the string does
    u16string s = u"a𐐀b";
    EXPECT_TRUE(s.as_slice().substr(1, 2).equal_fold(u"𐐨"));
}

// No simple case mapping crosses the end of the Basic Multilingual Plane, so
// a letter keeps its number of UTF-16 units and a u16string its size; and
// every cased code point maps in UTF-16 as it does alone
TEST(StringBoundary_Tests, EveryCasedCodePointInUtf16) {
    size_t cased = 0;
    for (char32_t c = 0; c <= 0x10FFFF; ++c) {
        if (c >= 0xD800 && c <= 0xDFFF) {
            continue;
        }
        char32_t lo = unicode::to_lower(c), up = unicode::to_upper(c);
        ASSERT_EQ(lo >= 0x10000, c >= 0x10000) << uint32_t(c);
        ASSERT_EQ(up >= 0x10000, c >= 0x10000) << uint32_t(c);
        if (lo == c && up == c) {
            continue;
        }
        ++cased;
        auto s = text_of<char16_t>(std::u32string_view(&c, 1));
        ASSERT_EQ(s.to_lower(), text_of<char16_t>(std::u32string_view(&lo, 1))) << uint32_t(c);
        ASSERT_EQ(s.to_upper(), text_of<char16_t>(std::u32string_view(&up, 1))) << uint32_t(c);
        ASSERT_TRUE(s.equal_fold(s.to_lower()) && s.equal_fold(s.to_upper())) << uint32_t(c);
    }
    EXPECT_GT(cased, 2000u);
}

// trim at its edges: all of the string, none of it, an empty set, a
// prefix or suffix that is the string or longer
TEST(StringBoundary_Tests, TrimAtItsEdges) {
    string s = "«x»";
    EXPECT_EQ(s.trim(U"").object(), s.object());
    EXPECT_EQ(s.trim("").object(), s.object());
    EXPECT_EQ(s.trim(U"«»"), "x");
    EXPECT_TRUE(s.trim(U"«»x").empty());
    EXPECT_TRUE(s.trim_left(U"«»x").empty());
    EXPECT_TRUE(s.trim_right(U"«»x").empty());
    EXPECT_EQ(s.trim_prefix("").object(), s.object());
    EXPECT_EQ(s.trim_suffix("").object(), s.object());
    EXPECT_TRUE(s.trim_prefix(s).empty());
    EXPECT_EQ(s.trim_prefix(s + "x").object(), s.object());
    EXPECT_EQ(s.trim_suffix("x" + s).object(), s.object());
    u16string u = u"😀x😀";
    EXPECT_EQ(u.trim(U"😀"), u"x");
    EXPECT_TRUE(u.trim(U"😀x").empty());
    EXPECT_EQ(string("\xC5x\xC5").trim(U"�"), "x");   // an ill-formed byte is U+FFFD to a set
}

// parse and to_string at the limits of each type
TEST(StringBoundary_Tests, ParseAndToStringAtTheLimitsOfEachType) {
    using reason = number_error::reason;
    EXPECT_EQ(parse<int8_t>("-128"), INT8_MIN);
    EXPECT_EQ(parse<int8_t>("127"), INT8_MAX);
    EXPECT_EQ(error_of(parse<int8_t>("-129")).why(), reason::out_of_range);
    EXPECT_EQ(error_of(parse<int8_t>("128")).offset(), 3u);
    EXPECT_EQ(parse<int64_t>("-9223372036854775808"), INT64_MIN);
    EXPECT_EQ(error_of(parse<int64_t>("9223372036854775808")).why(), reason::out_of_range);
    EXPECT_EQ(parse<uint64_t>("18446744073709551615"), UINT64_MAX);
    EXPECT_EQ(error_of(parse<uint64_t>("18446744073709551616")).offset(), 20u);
    EXPECT_EQ(parse<int>("-0"), 0);
    EXPECT_EQ(error_of(parse<int>("+1")).why(), reason::not_a_number);
    EXPECT_EQ(error_of(parse<int>("-")).why(), reason::not_a_number);
    EXPECT_EQ(parse<int>("7fffffff", 16), INT32_MAX);
    EXPECT_EQ(parse<int>("-80000000", 16), INT32_MIN);
    EXPECT_EQ(error_of(parse<int>("80000000", 16)).why(), reason::out_of_range);
    EXPECT_EQ(error_of(parse<int>("0x10", 16)).why(), reason::trailing);
    EXPECT_EQ(error_of(parse<int>(std::string_view("1\0", 2))).offset(), 1u);
    EXPECT_EQ(error_of(parse<double>("1e309")).why(), reason::out_of_range);
    EXPECT_EQ(parse<double>("1e308"), 1e308);
    EXPECT_EQ(error_of(parse<bool>("")).why(), reason::empty);
    EXPECT_EQ(error_of(parse<bool>("TRUE")).why(), reason::not_a_number);
    EXPECT_EQ(to_string(INT64_MIN), "-9223372036854775808");
    EXPECT_EQ(to_string(UINT64_MAX), "18446744073709551615");
    EXPECT_EQ(to_string(int8_t(-128)), "-128");
    EXPECT_EQ(to_string('\0').size(), 1u);
    EXPECT_EQ(parse<int8_t>(to_string(int8_t(INT8_MIN)).view()), INT8_MIN);
    EXPECT_EQ(parse<uint16_t>(to_string(uint16_t(UINT16_MAX)).view()), UINT16_MAX);
    EXPECT_EQ(parse<int32_t>(to_string(INT32_MIN).view()), INT32_MIN);
}
