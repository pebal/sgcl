//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// utf8 and unicode: the encoding's primitives, the case and white-space
// tables, and the text of string and slice as Unicode — runes, the
// char32_t overloads, trim and fields over Unicode white space, to_lower
// beyond ASCII, equal_fold.
#include "tests/types.h"

#include <random>
#include <string>

#include <string>
#include <string_view>

namespace {
    // The int overloads are deleted: a call is ill-formed, which a
    // concept sees where a static_assert on a requires-expression would not
    template<class S> concept FindsAnInt = requires(const S& x) { x.find(int(1)); };
    template<class S> concept ContainsAnInt = requires(const S& x) { x.contains(int(1)); };
    template<class S> concept SplitsOnAnInt = requires(const S& x) { x.split(int(1)); };
    template<class S> concept ReplacesAnInt = requires(const S& x) { x.replace(int(1), int(2)); };

    // unicode's names are objects that take a char32_t and refuse
    // everything else, a byte of UTF-8 and an int included
    template<class T> concept Lowers = requires(T c) { unicode::to_lower(c); };
    template<class T> concept Uppers = requires(T c) { unicode::to_upper(c); };
    template<class T> concept AsksUpper = requires(T c) { unicode::is_upper(c); };
    template<class T> concept AsksLower = requires(T c) { unicode::is_lower(c); };
    template<class T> concept AsksSpace = requires(T c) { unicode::is_space(c); };
    template<class T> concept Folds = requires(T c) { unicode::equal_fold(c, c); };
}

TEST(Utf8_Tests, DecodeEncodeCountValidate) {
    static_assert(utf8::decode("a").first == U'a' && utf8::decode("a").second == 1);
    static_assert(utf8::decode("ż").first == U'ż' && utf8::decode("ż").second == 2);
    static_assert(utf8::decode("€").second == 3 && utf8::decode("😀").first == U'😀' && utf8::decode("😀").second == 4);
    static_assert(utf8::decode("ab", 1).first == U'b' && utf8::decode("ab", 2).second == 0);   // past the end: nothing
    static_assert(utf8::count("ŁÓDŹ") == 4 && utf8::count("") == 0 && utf8::count("a😀b") == 3);
    static_assert(utf8::width(U'a') == 1 && utf8::width(U'ż') == 2 && utf8::width(U'€') == 3 && utf8::width(U'😀') == 4);
    static_assert(utf8::width(0xD800) == 0 && utf8::width(0x110000) == 0 && !utf8::valid(char32_t(0xDFFF)) && utf8::valid(U'\U0010FFFF'));
    static_assert(utf8::starts_rune('a') && utf8::starts_rune('\xC5') && !utf8::starts_rune('\xBC'));

    // Every invalid byte is one code point, the replacement character
    static_assert(utf8::decode("\xFF").first == utf8::replacement && utf8::decode("\xFF").second == 1);
    static_assert(utf8::decode("\xC5").first == utf8::replacement);           // truncated
    static_assert(utf8::decode("\xC0\x80").first == utf8::replacement);       // overlong NUL
    static_assert(utf8::decode("\xE0\x80\x80").first == utf8::replacement);   // overlong
    static_assert(utf8::decode("\xED\xA0\x80").first == utf8::replacement);   // a surrogate
    static_assert(utf8::decode("\xF4\x90\x80\x80").first == utf8::replacement);   // past U+10FFFF
    static_assert(utf8::decode("\xBC" "a").second == 1 && utf8::decode("\xBC" "a", 1).first == U'a');   // a stray continuation byte
    static_assert(utf8::count("\xFF\xFE") == 2 && utf8::count("a\xC5") == 2);
    static_assert(!utf8::valid("\xFF") && !utf8::valid("a\xC5") && utf8::valid("ok ż 😀") && utf8::valid(""));
    static_assert(utf8::valid("\xEF\xBF\xBD"));   // U+FFFD itself is valid text

    // The last code point
    static_assert(utf8::decode_last("ab😀").first == U'😀' && utf8::decode_last("ab😀").second == 4);
    static_assert(utf8::decode_last("żab", 2).first == U'ż');
    static_assert(utf8::decode_last("ab\x80").first == utf8::replacement && utf8::decode_last("ab\x80").second == 1);
    static_assert(utf8::decode_last("").second == 0);

    // Encoding
    static_assert(utf8::encoded(U'a').size == 1 && utf8::encoded(U'ż').view() == "ż" && utf8::encoded(U'😀').view() == "😀");
    static_assert(utf8::encoded(char32_t(0xD800)).view() == "\xEF\xBF\xBD");   // not a scalar value: the replacement
    char out[utf8::max_width];
    EXPECT_EQ(utf8::encode(U'€', out), 3u);
    EXPECT_EQ(std::string_view(out, 3), "€");
}

TEST(Utf8_Tests, TheCaseAndSpaceOfACodePoint) {
    static_assert(unicode::to_lower(U'A') == U'a' && unicode::to_upper(U'z') == U'Z' && unicode::to_lower(U'1') == U'1');
    static_assert(unicode::to_lower(U'Ł') == U'ł' && unicode::to_upper(U'ź') == U'Ź' && unicode::to_lower(U'Ó') == U'ó');
    static_assert(unicode::to_lower(U'Σ') == U'σ' && unicode::to_upper(U'ς') == U'Σ' && unicode::to_lower(U'Я') == U'я');
    static_assert(unicode::to_lower(U'İ') == U'i');                       // the simple mapping, not the full one with U+0307
    static_assert(unicode::to_upper(U'ß') == U'ß' && unicode::to_upper(U'ŉ') == U'ŉ');   // no one-to-one upper case
    static_assert(unicode::to_lower(U'Ǆ') == U'ǆ' && unicode::to_lower(U'ǅ') == U'ǆ' && unicode::to_upper(U'ǅ') == U'Ǆ');   // titlecase digraphs
    static_assert(unicode::to_lower(U'Ā') == U'ā' && unicode::to_lower(U'ā') == U'ā' && unicode::to_upper(U'ă') == U'Ă');   // the alternating ranges
    static_assert(unicode::to_lower(U'Ⅻ') == U'ⅻ' && unicode::to_lower(U'Ａ') == U'ａ' && unicode::to_lower(U'𐐀') == U'𐐨');   // number forms, fullwidth, Deseret
    static_assert(unicode::to_lower(U'中') == U'中' && unicode::to_upper(U'😀') == U'😀');
    static_assert(unicode::is_upper(U'Ł') && !unicode::is_upper(U'ł') && unicode::is_lower(U'ł') && !unicode::is_lower(U'1'));
    static_assert(unicode::equal_fold(U'a', U'A') && unicode::equal_fold(U'σ', U'Σ') && unicode::equal_fold(U'ς', U'σ') && !unicode::equal_fold(U'a', U'b'));
    static_assert(unicode::is_space(U' ') && unicode::is_space(U'\t') && unicode::is_space(U'\n') && unicode::is_space(U'\r'));
    static_assert(unicode::is_space(U'\u00A0') && unicode::is_space(U'\u2003') && unicode::is_space(U'\u3000') && unicode::is_space(U'\u0085'));
    static_assert(!unicode::is_space(U'x') && !unicode::is_space(U'\u200B') && !unicode::is_space(U'ż'));   // a zero-width space is not White_Space
    EXPECT_STREQ(unicode::version, detail::UnicodeVersion);

    // A code point and nothing else: a char is a byte of UTF-8
    // (is_space(s[0]) on a no-break space asked about 0xC2 and answered
    // no, to_lower(s[0]) gave 0xFFFFFFC2) and an int is a
    // multi-character literal
    static_assert(Lowers<char32_t> && !Lowers<char> && !Lowers<int> && !Lowers<char8_t> && !Lowers<char16_t> && !Lowers<wchar_t> && !Lowers<size_t>);
    static_assert(Uppers<char32_t> && !Uppers<char> && !Uppers<int>);
    static_assert(AsksUpper<char32_t> && !AsksUpper<char> && !AsksUpper<int>);
    static_assert(AsksLower<char32_t> && !AsksLower<char> && !AsksLower<int>);
    static_assert(AsksSpace<char32_t> && !AsksSpace<char> && !AsksSpace<int> && !AsksSpace<unsigned char>);
    static_assert(Folds<char32_t> && !Folds<char> && !Folds<int>);

    // And each of them is still a predicate, passed by name
    string s = "ŁÓDŹ nad Wisłą\u00A0";
    EXPECT_EQ(s.runes().count_of(unicode::is_upper), 5u);
    EXPECT_EQ(s.runes().count_of(unicode::is_space), 3u);
    EXPECT_TRUE(s.runes().exists(unicode::is_lower));
    EXPECT_EQ(string("ŁÓDŹ").runes().find_index(unicode::is_lower), npos);
}

TEST(Utf8_Tests, TheRunesOfAString) {
    string s = "żółw 😀";
    EXPECT_EQ(s.size(), 12u);
    EXPECT_EQ(s.rune_count(), 6u);
    std::u32string seen;
    std::vector<size_t> positions;
    for (auto it = s.runes().begin(); it != s.runes().end(); ++it) {
        seen += *it;
        positions.push_back(it.pos());
    }
    EXPECT_EQ(seen, U"żółw 😀");
    EXPECT_EQ(positions, (std::vector<size_t>{0, 2, 4, 6, 7, 8}));
    EXPECT_EQ(s.runes().count(), 6u);
    EXPECT_TRUE(s.runes().contains(U'ł') && !s.runes().contains(U'l'));
    EXPECT_EQ(s.runes().count_of([](char32_t c) { return c > 0x7F; }), 4u);
    EXPECT_EQ(s.decode(2).first, U'ó');
    EXPECT_EQ(s.decode(2).second, 2u);
    EXPECT_TRUE(s.is_valid_utf8());
    static_assert(req::enumerable<runes>);

    // A range over a temporary holds the string
    size_t n = 0;
    for (char32_t c : string("ąę").runes()) {
        n += c == U'ę';
    }
    EXPECT_EQ(n, 1u);

    // An invalid byte is one rune, the replacement, and the string says so
    string bad = "a\xFF" "b";
    EXPECT_FALSE(bad.is_valid_utf8());
    EXPECT_EQ(bad.rune_count(), 3u);
    EXPECT_EQ(*++bad.runes().begin(), utf8::replacement);
    EXPECT_TRUE(string().runes().empty() && string().runes().begin() == string().runes().end());

    // A wide string's units are its code points
    wstring w = L"żółw";
    EXPECT_EQ(w.rune_count(), 4u);
}

TEST(Utf8_Tests, ACharacterIsACodePoint) {
    string s = "  ŁÓDŹ nad Wisłą\u3000";
    EXPECT_EQ(s.find(U'Ó'), 4u);
    EXPECT_EQ(s.find(U'Ź'), 7u);
    EXPECT_EQ(s.rfind(U'ł'), s.find("ł"));
    EXPECT_EQ(s.find(U'x'), npos);
    EXPECT_TRUE(s.contains(U'ą') && !s.contains(U'😀') && s.contains('W'));
    EXPECT_TRUE(s.starts_with(' ') && string("żółw").starts_with(U'ż') && string("żółw").ends_with(U'w') && !string("żółw").ends_with(U'ł'));
    EXPECT_EQ(s.find_first_of(U"ÓŹ"), 4u);
    EXPECT_EQ(s.find_last_of(U"ÓŹ"), 7u);
    EXPECT_EQ(s.find_first_not_of(U" Ł"), 4u);
    EXPECT_EQ(s.find_last_not_of(U"\u3000ą"), s.find("ł"));
    EXPECT_EQ(s.find_first_of(U"xyz"), npos);
    EXPECT_EQ(string("aXbXc").find_first_of(U"X", 2), 3u);   // from a position

    // split, replace, join by a code point
    vector<string> parts(string("a·b·c").split(U'·'));
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(string("a·b·c").replace(U'·', U'—'), "a—b—c");
    EXPECT_EQ(string("a·b·c").replace(U'·', U'-', 1), "a-b·c");
    EXPECT_EQ(string::join(parts, U'→'), "a→b→c");

    // A set of code points to trim
    EXPECT_EQ(string("«x»").trim(U"«»"), "x");
    EXPECT_EQ(string("«x»").trim_left(U"«»"), "x»");
    EXPECT_EQ(string("«x»").trim_right(U"«»"), "«x");
    EXPECT_TRUE(string("«»").trim(U"«»").empty());
    EXPECT_EQ(string("»»x").trim(U"«»"), "x");

    // The same on a slice, nothing copied
    slice<const char> t = s.as_slice();
    EXPECT_EQ(t.find(U'Ź'), 7u);
    EXPECT_TRUE(t.contains(U'Ó') && !t.contains(U'x'));
    EXPECT_EQ(t.trim(U" \u3000Ł").str(), "ÓDŹ nad Wisłą");
    EXPECT_EQ(t.trim(U" \u3000Ł").owner(), t.owner());
    EXPECT_EQ(t.find_first_of(U"ÓŹ"), 4u);

    // The overloads refuse an int: 'ż' in a UTF-8 source is a
    // multi-character literal of that type
    static_assert(!FindsAnInt<string> && !ContainsAnInt<string> && !SplitsOnAnInt<string> && !ReplacesAnInt<string>);
    static_assert(!ContainsAnInt<slice<const char>> && !FindsAnInt<slice<const char>> && !FindsAnInt<wstring>);
    static_assert(ContainsAnInt<slice<int>>);   // a slice of ints keeps its contains
    static_assert(requires(const string& x) { x.find('a'); x.find(U'a'); x.find("a"); });
}

TEST(Utf8_Tests, TheWhiteSpaceAndTheCaseOfAString) {
    // trim and fields skip Unicode white space
    string s = "\u00A0 ŁÓDŹ nad Wisłą\u3000\t";
    EXPECT_EQ(s.trim(), "ŁÓDŹ nad Wisłą");
    EXPECT_EQ(s.trim_left(), "ŁÓDŹ nad Wisłą\u3000\t");
    EXPECT_EQ(s.trim_right(), "\u00A0 ŁÓDŹ nad Wisłą");
    EXPECT_TRUE(string("\u2003\u3000 ").trim().empty() && string("\u2003").trim_left().empty() && string("\u2003").trim_right().empty());
    string plain = "plain";
    EXPECT_EQ(plain.trim().object(), plain.object());              // nothing to trim: the same object
    vector<string> f(string("a\u00A0b\tc\u3000\u3000d").fields());
    ASSERT_EQ(f.size(), 4u);
    EXPECT_EQ(f[1], "b");
    EXPECT_EQ(f[3], "d");
    EXPECT_TRUE(string("\u00A0\u2003").fields().empty());
    slice<const char> t = s.as_slice();
    EXPECT_EQ(t.trim().str(), "ŁÓDŹ nad Wisłą");
    EXPECT_EQ(t.trim_left().str(), "ŁÓDŹ nad Wisłą\u3000\t");
    EXPECT_EQ(t.trim_right().str(), "\u00A0 ŁÓDŹ nad Wisłą");
    EXPECT_EQ(t.trim().owner(), t.owner());

    // The case by the simple mapping, the length free to change
    EXPECT_EQ(s.to_lower(), "\u00A0 łódź nad wisłą\u3000\t");
    EXPECT_EQ(s.to_upper(), "\u00A0 ŁÓDŹ NAD WISŁĄ\u3000\t");
    EXPECT_EQ(string("ΣΊΣΥΦΟΣ").to_lower(), "σίσυφοσ");                 // the simple mapping: no final sigma
    EXPECT_EQ(string("straße").to_upper(), "STRAßE");                    // ß has no one-to-one upper case
    EXPECT_EQ(string("İstanbul").to_lower(), "istanbul");                // 2 bytes to 1
    EXPECT_EQ(string("ǆ").to_upper(), "Ǆ");
    EXPECT_EQ(string("中文 123").to_lower(), "中文 123");
    string lower = "łódź";
    EXPECT_EQ(lower.to_lower().object(), lower.object());            // no letter changes: the same object
    EXPECT_EQ(string("Hello, World").to_lower(), "hello, world");       // ASCII as before
    wstring w = L"  Łódź ";
    EXPECT_EQ(w.trim(), L"Łódź");
    EXPECT_EQ(w.to_upper(), L"  ŁÓDŹ ");

    // equal_fold: the same letters in either case
    EXPECT_TRUE(string("Łódź").equal_fold("ŁÓDŹ") && string("Σίσυφος").equal_fold("ΣΊΣΥΦΟΣ") && string("abc").equal_fold("ABC"));
    EXPECT_FALSE(string("Straße").equal_fold("STRASSE") || string("a").equal_fold("ab") || string("ab").equal_fold("a"));
    EXPECT_TRUE(t.equal_fold(s.to_upper()));
    EXPECT_TRUE(w.equal_fold(L"  łÓDŹ "));
}

TEST(Utf8_Tests, CountingOverARunOfAscii) {
    // count walks a run of ASCII eight bytes at a time and decodes what
    // is left one code point at a time. The answer has to be the same as
    // the plain walk byte by byte, for valid text and for invalid alike:
    // an invalid byte is one code point, and so is each byte of a
    // truncated sequence.
    auto reference = [](std::string_view v) {
        size_t n = 0;
        for (size_t i = 0; i < v.size(); ++n) {
            unsigned char c = (unsigned char)v[i];
            i += c < 0x80 ? 1 : utf8::decode(v, i).second;
        }
        return n;
    };
    std::mt19937 rng(11);
    size_t cases = 0;
    for (int round = 0; round < 20000; ++round) {
        std::string v;
        for (size_t i = 0, n = rng() % 40; i < n; ++i) {
            int k = rng() % 100;
            if (k < 55) v += char('a' + rng() % 26);
            else if (k < 80) v += "ż";
            else if (k < 90) v += "日";
            else if (k < 95) v += "😀";
            else v += char(rng() % 256);          // anything, valid or not
        }
        ASSERT_EQ(utf8::count(v), reference(v)) << v.size() << " bajtów";
        ++cases;
    }
    EXPECT_EQ(cases, 20000u);

    // and the runs that cross the eight-byte step
    EXPECT_EQ(utf8::count("abcdefghijklmnop日"), 17u);
    EXPECT_EQ(utf8::count("日abcdefghijklmnop"), 17u);
    EXPECT_EQ(utf8::count(std::string(1000, 'a')), 1000u);
    EXPECT_EQ(utf8::count("\xE6\x97"), 2u);      // a truncated sequence is a code point a byte
}
