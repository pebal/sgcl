//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of slice and runes (DESIGN 408): a default slice at every
// member; positions and counts at 0, size() and npos; the slice as its
// own argument (s = s, s = s.subslice(1), swap with itself, a trim by the
// slice's own characters); a text all trimmed; arrays with and without
// their NUL; malformed UTF-8 at the ends of a text; an owner kept by
// every piece made of a slice.
#include "tests/types.h"

#include <array>
#include <span>
#include <string_view>
#include <vector>

TEST(SliceBoundaries_Tests, DefaultAtEveryMember) {
    slice<int> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_EQ(s.size_bytes(), 0u);
    EXPECT_EQ(s.data(), nullptr);
    EXPECT_FALSE(s.owned());
    EXPECT_FALSE(s.owner());
    EXPECT_EQ(s.begin(), s.end());
    EXPECT_EQ(s.rbegin(), s.rend());
    EXPECT_EQ(s.cbegin(), s.cend());
    EXPECT_TRUE(s.subslice(0).empty());
    EXPECT_TRUE(s.subslice(0, 0).empty());
    EXPECT_TRUE(s.subslice(0, slice<int>::npos).empty());
    EXPECT_THROW(s.subslice(1), out_of_range);
    EXPECT_THROW(s.subspan(slice<int>::npos), out_of_range);
    EXPECT_TRUE(s.first(0).empty());
    EXPECT_TRUE(s.last(0).empty());
    s.remove_prefix(0);
    s.remove_suffix(0);
    EXPECT_TRUE(s.as_slice().empty());
    EXPECT_TRUE(std::span<int>(s).empty());
    EXPECT_TRUE(as_bytes(s).empty());
    EXPECT_TRUE(as_writable_bytes(s).empty());
    EXPECT_FALSE(s.contains(0));
    EXPECT_TRUE(s == slice<int>());
    EXPECT_TRUE((s <=> slice<int>()) == 0);
    slice<const int> c = s;
    EXPECT_TRUE(c.empty());
    slice<int> moved(std::move(s));
    EXPECT_TRUE(moved.empty());
    EXPECT_TRUE(s.empty());

    slice<const char> t;
    EXPECT_TRUE(t.trim().empty());
    EXPECT_TRUE(t.trim(" ").empty());
    EXPECT_TRUE(t.trim(U"«»").empty());
    EXPECT_TRUE(t.trim_left().empty());
    EXPECT_TRUE(t.trim_right().empty());
    EXPECT_TRUE(t.trim_left(U" ").empty());
    EXPECT_TRUE(t.trim_right(U" ").empty());
    EXPECT_TRUE(t.trim_prefix("").empty());
    EXPECT_TRUE(t.trim_suffix("x").empty());
    EXPECT_TRUE(t.substr().empty());
    EXPECT_FALSE(t.contains("a"));
    EXPECT_TRUE(t.contains(""));
    EXPECT_FALSE(t.contains('a'));
    EXPECT_FALSE(t.contains(U'ż'));
    EXPECT_TRUE(t.runes().empty());
    EXPECT_EQ(t.runes().count(), 0u);
    EXPECT_EQ(t.runes().begin(), t.runes().end());
    slice<const byte> b = t;
    EXPECT_TRUE(b.empty());
}

TEST(SliceBoundaries_Tests, PositionsAtTheEnds) {
    std::array<int, 4> a{1, 2, 3, 4};
    slice<int> s = a;
    EXPECT_TRUE(s.subslice(4).empty());
    EXPECT_EQ(s.subslice(4).data(), a.data() + 4);
    EXPECT_THROW(s.subslice(5), out_of_range);
    EXPECT_EQ(s.subslice(0, SIZE_MAX).size(), 4u);   // n cut to what is left, no overflow of pos + n
    EXPECT_EQ(s.subslice(3, SIZE_MAX).size(), 1u);
    EXPECT_EQ(s.subslice(1, 2).front(), 2);
    EXPECT_EQ(s.subslice(1, 2).back(), 3);
    EXPECT_EQ(s.first(4).size(), 4u);
    EXPECT_EQ(s.last(4).size(), 4u);
    EXPECT_EQ(s.first(0).data(), a.data());
    EXPECT_EQ(s.last(0).data(), a.data() + 4);
    slice<int> r = s;
    r.remove_prefix(4);
    EXPECT_TRUE(r.empty());
    EXPECT_EQ(r.data(), a.data() + 4);
    r = s;
    r.remove_suffix(4);
    EXPECT_TRUE(r.empty());
    EXPECT_EQ(r.data(), a.data());
    EXPECT_EQ(s[0], 1);
    EXPECT_EQ(s[3], 4);
    EXPECT_EQ(*s.rbegin(), 4);
    EXPECT_EQ(s.size_bytes(), sizeof(a));
}

TEST(SliceBoundaries_Tests, ItselfAsTheArgument) {
    string text = "abcdef";
    slice<const char> s = text;
    s = s;
    EXPECT_EQ(s, "abcdef");
    s = std::move(s);   // a move is a copy: the slice stays
    EXPECT_EQ(s, "abcdef");
    s.swap(s);
    EXPECT_EQ(s, "abcdef");
    s = s.subslice(1);
    EXPECT_EQ(s, "bcdef");
    s = s.first(4);
    EXPECT_EQ(s, "bcde");
    s = s.last(3);
    EXPECT_EQ(s, "cde");
    EXPECT_TRUE(s.owned());
    EXPECT_TRUE(s.trim_prefix(s.view()).empty());   // the slice's own characters as the prefix
    EXPECT_TRUE(s.trim_suffix(s.view()).empty());
    EXPECT_TRUE(s.trim(s.view()).empty());
    EXPECT_TRUE(s.contains(s.view()));
    EXPECT_TRUE(s.starts_with(s.view()));
    EXPECT_TRUE(s == s);
    EXPECT_TRUE((s <=> s) == 0);

    std::array<int, 5> a{5, 1, 4, 2, 3};
    slice<int> n = a;
    n = n.subslice(1, 3);
    EXPECT_EQ(n.size(), 3u);
    EXPECT_TRUE(n.contains(n[0]));
    n.swap(n);
    EXPECT_EQ(n[0], 1);
}

TEST(SliceBoundaries_Tests, TextAllTrimmed) {
    string spaces = " \t\n  ";   // ASCII white space and U+00A0 in UTF-8
    slice<const char> s = spaces;
    for (auto t : {s.trim(), s.trim_left(), s.trim_right(), s.trim(" \t\n\xC2\xA0"), s.trim(U" \t\n "),
                   s.trim_left(U" \t\n "), s.trim_right(U" \t\n ")}) {
        EXPECT_TRUE(t.empty());
        EXPECT_TRUE(t.owned());   // still of the same owner (trim.md)
        EXPECT_EQ(t.owner(), s.owner());
    }
    EXPECT_EQ(s.trim().data(), s.data());
    EXPECT_EQ(s.trim_right().data(), s.data());
    slice<const char> word = " a ";
    EXPECT_EQ(word.trim(), "a");
    EXPECT_EQ(word.trim_left(), "a ");
    EXPECT_EQ(word.trim_right(), " a");
    EXPECT_EQ(word.trim_prefix(" a  "), " a ");   // longer than the text: not there
    EXPECT_EQ(word.trim_suffix(" a  "), " a ");
    EXPECT_EQ(word.trim_prefix(""), " a ");
    EXPECT_EQ(word.trim_suffix(""), " a ");
}

TEST(SliceBoundaries_Tests, MalformedUtf8AtTheEnds) {
    // A lead byte without its continuation at the end, a lone continuation at the start
    const char bad[] = "\x80" "a" "\xE2\x82";
    slice<const char> s = bad;
    EXPECT_EQ(s.size(), 4u);
    EXPECT_EQ(s.trim(), s);   // U+FFFD is not white space
    EXPECT_EQ(s.trim(U"a"), s);
    EXPECT_EQ(s.trim(U"�"), "a");   // each malformed byte is U+FFFD, one byte wide
    EXPECT_EQ(s.trim_left(U"�"), "a\xE2\x82");
    EXPECT_EQ(s.trim_right(U"�"), "\x80" "a");
    EXPECT_EQ(s.runes().count(), 4u);
    size_t n = 0;
    for (auto it = s.runes().begin(); it != s.runes().end(); ++it) {
        EXPECT_EQ(it.width(), 1u);
        ++n;
    }
    EXPECT_EQ(n, 4u);
    const char four[] = "\xF0\x9F\x98\x80";   // one code point of four bytes, at the end
    slice<const char> f = four;
    EXPECT_EQ(f.trim(U"\U0001F600"), "");
    EXPECT_EQ(f.trim_right(U"x"), f);
    EXPECT_EQ(f.runes().count(), 1u);
    EXPECT_EQ(*f.runes().begin(), U'\U0001F600');
    EXPECT_EQ(f.runes().begin().width(), 4u);
    slice<const char> cut = f.first(3);   // the same code point cut: three U+FFFD
    EXPECT_EQ(cut.runes().count(), 3u);
    EXPECT_EQ(cut.trim(U"�"), "");
    sgcl::runes::iterator none;
    EXPECT_EQ(none.pos(), 0u);
    EXPECT_EQ(none.width(), 0u);
}

TEST(SliceBoundaries_Tests, ArraysWithAndWithoutTheirNul) {
    const char full[3] = {'a', 'b', 'c'};   // no NUL: all of it, not past
    slice<const char> t = full;
    EXPECT_EQ(t.size(), 3u);
    slice<const byte> tb = full;
    EXPECT_EQ(tb.size(), 3u);
    const char mid[5] = {'a', 0, 'b', 0, 0};   // up to the first NUL
    slice<const char> m = mid;
    EXPECT_EQ(m.size(), 1u);
    slice<const byte> mb = mid;
    EXPECT_EQ(mb.size(), 1u);
    const char empty[1] = {0};
    EXPECT_TRUE(slice<const char>(empty).empty());
    char writable[4] = {'x', 0, 'y', 0};   // a slice of char: every element, NULs included
    slice<char> w = writable;
    EXPECT_EQ(w.size(), 4u);
    const unsigned char bytes[3] = {0, 0, 0};   // bytes: all of them
    slice<const byte> ub = bytes;
    EXPECT_EQ(ub.size(), 3u);
    std::array<unsigned char, 2> ua{0, 1};
    slice<const byte> uab = ua;
    EXPECT_EQ(uab.size(), 2u);
    std::array<int, 0> none{};
    slice<int> n = none;
    EXPECT_TRUE(n.empty());
    std::vector<int> ev;
    slice<int> e = ev;
    EXPECT_TRUE(e.empty());
    slice<const char> from_view = std::string_view();
    EXPECT_TRUE(from_view.empty());
    slice<const byte> from_view_bytes = std::string_view("a\0b", 3);
    EXPECT_EQ(from_view_bytes.size(), 3u);
}

TEST(SliceBoundaries_Tests, EveryPieceKeepsTheOwner) {
    slice<const char> s;
    slice<const char> pieces[8];
    off_frame([&] {
        string text = string("  piece of text  ") + "!";
        s = text;
        pieces[0] = s.subslice(2);
        pieces[1] = s.first(1);
        pieces[2] = s.last(1);
        pieces[3] = s.trim();
        pieces[4] = s.trim(U" !");
        pieces[5] = s.trim_prefix("  ");
        pieces[6] = s.substr(2, 5);
        pieces[7] = s.runes().text();
    });
    collector::clear_stack();
    collector::force_collect(true);
    for (auto& p : pieces) {
        EXPECT_TRUE(p.owned());
        EXPECT_EQ(p.owner(), s.owner());
    }
    EXPECT_EQ(pieces[3], "piece of text  !");
    EXPECT_EQ(pieces[4], "piece of text");
    EXPECT_EQ(pieces[6], "piece");
    slice<const byte> b = pieces[4];
    EXPECT_EQ(b.owner(), s.owner());
    EXPECT_EQ(as_bytes(pieces[4]).owner(), s.owner());
}

TEST(SliceBoundaries_Tests, OwnedSliceOfAVectorSorted) {
    vector<int> v = {3, 1, 2};
    slice<int> s = v;
    EXPECT_TRUE(s.owned());
    s.subslice(1).sort();
    EXPECT_EQ(v[0], 3);
    EXPECT_EQ(v[1], 1);
    EXPECT_EQ(v[2], 2);
    s.subslice(0, 0).sort();
    s.sort();
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[2], 3);
}
