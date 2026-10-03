//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::encoding: text in and out of the shapes other people keep it in.
// The oracle is Python's codecs — the tables here are read from the
// MAPPINGS files of unicode.org and Python's are built into the
// interpreter, so the two roads to the same answer are independent.
#include "tests/types.h"
#include "tests/txt/encoding_tests.h"

#include <string>

namespace {
    string utf8_of(const char32_t* points) {
        std::string out;
        char buf[utf8::max_width];
        for (size_t i = 0; points[i]; ++i) {
            out.append(buf, utf8::encode(points[i], buf));
        }
        return string(out.data(), out.size());
    }

    txt::encoding named(std::string_view name) {
        auto e = txt::encoding_from_name(string(name.data(), name.size()));
        EXPECT_TRUE(e.has_value()) << name;
        return e ? *e : txt::encoding::utf8;
    }

    std::string bytes_of(const vector<byte>& v) {
        std::string out;
        for (auto b : v) {
            out += char(b);
        }
        return out;
    }
}

TEST(Encoding_Tests, EveryByteOfEverySingleByteEncoding) {
    for (const auto& m : ucd::ByteMappings) {
        auto e = named(m.encoding);
        for (uint32_t b = 0; b < 256; ++b) {
            byte one[1] = {byte(b)};
            string got = txt::decode(slice<const byte>(one), e);
            char buf[utf8::max_width];
            string want = string(buf, utf8::encode(m.points[b], buf));
            ASSERT_EQ(got, want) << m.encoding << " bajt " << std::hex << b;
        }
    }
    EXPECT_EQ(std::size(ucd::ByteMappings), 29u);   // every single byte encoding of the set
}

TEST(Encoding_Tests, EveryTextAgainstPython) {
    size_t n = 0;
    for (const auto& t : ucd::TextsInBytes) {
        auto e = named(t.encoding);
        string text = utf8_of(t.text);
        std::string want(t.bytes, t.size);

        // What the library encodes is what Python encodes, the question
        // mark for what the encoding cannot write included
        ASSERT_EQ(bytes_of(txt::encode(text, e)), want) << t.encoding << " : " << text;

        // And back again, where the encoding lost nothing
        if (want.find('?') == std::string::npos || text.find('?') != string::npos) {
            auto raw = txt::encode(text, e);
            ASSERT_EQ(txt::decode(raw.as_slice(), e), text) << t.encoding << " : " << text;
        }
        ++n;
    }
    EXPECT_EQ(n, std::size(ucd::TextsInBytes));
    EXPECT_GT(n, 300u);
}

TEST(Encoding_Tests, TheNamesAHeaderUses) {
    using txt::encoding;
    EXPECT_EQ(txt::encoding_from_name(string("utf-8")), encoding::utf8);
    EXPECT_EQ(txt::encoding_from_name(string("UTF-8")), encoding::utf8);
    EXPECT_EQ(txt::encoding_from_name(string("utf8")), encoding::utf8);
    EXPECT_EQ(txt::encoding_from_name(string("ISO_8859-2")), encoding::iso8859_2);
    EXPECT_EQ(txt::encoding_from_name(string("iso88592")), encoding::iso8859_2);
    EXPECT_EQ(txt::encoding_from_name(string("Latin2")), encoding::iso8859_2);
    EXPECT_EQ(txt::encoding_from_name(string("cp1250")), encoding::windows1250);
    EXPECT_EQ(txt::encoding_from_name(string("WINDOWS-1252")), encoding::windows1252);
    EXPECT_EQ(txt::encoding_from_name(string("us-ascii")), encoding::ascii);

    // A name nobody knows is an answer, not a failure
    EXPECT_FALSE(txt::encoding_from_name(string("klingon")).has_value());
    EXPECT_FALSE(txt::encoding_from_name(string()).has_value());
    EXPECT_FALSE(txt::encoding_from_name(string("utf-9")).has_value());

    // Every encoding's own name is read back as itself
    for (auto e : {encoding::utf8, encoding::utf16le, encoding::utf16be, encoding::utf32le,
                   encoding::utf32be, encoding::ascii, encoding::latin1, encoding::iso8859_2,
                   encoding::windows1250, encoding::windows1252}) {
        EXPECT_EQ(txt::encoding_from_name(string(txt::name_of(e))), e) << txt::name_of(e);
    }
}

TEST(Encoding_Tests, WhatAByteOrderMarkSays) {
    auto bom = [](std::initializer_list<uint32_t> bytes) {
        static std::vector<byte> keep;
        keep.assign(bytes.size(), byte(0));
        size_t i = 0;
        for (auto b : bytes) {
            keep[i++] = byte(b);
        }
        return txt::detect_bom(slice<const byte>(std::span<const byte>(keep)));
    };
    EXPECT_EQ(bom({0xEF, 0xBB, 0xBF, 'a'}).says, txt::encoding::utf8);
    EXPECT_EQ(bom({0xEF, 0xBB, 0xBF, 'a'}).size, 3u);
    EXPECT_EQ(bom({0xFF, 0xFE, 'a', 0}).says, txt::encoding::utf16le);
    EXPECT_EQ(bom({0xFF, 0xFE, 'a', 0}).size, 2u);
    EXPECT_EQ(bom({0xFE, 0xFF, 0, 'a'}).says, txt::encoding::utf16be);
    EXPECT_EQ(bom({0xFF, 0xFE, 0x00, 0x00}).says, txt::encoding::utf32le);   // not the sixteen bit one
    EXPECT_EQ(bom({0xFF, 0xFE, 0x00, 0x00}).size, 4u);
    EXPECT_EQ(bom({0x00, 0x00, 0xFE, 0xFF}).says, txt::encoding::utf32be);
    EXPECT_FALSE(bom({'a', 'b', 'c'}));
    EXPECT_FALSE(bom({}));
    EXPECT_FALSE(bom({0xEF, 0xBB}));                                         // half of one is none
    EXPECT_EQ(bom({'a'}).size, 0u);
}

TEST(Encoding_Tests, TheOtherTwoEncodingsOfUnicode) {
    string s = "Zaż 日 \U0001F600";
    auto u16 = txt::to_utf16(s);
    auto u32 = txt::to_utf32(s);
    EXPECT_EQ(u16.size(), 8u);          // the emoji is a surrogate pair
    EXPECT_EQ(u32.size(), 7u);
    EXPECT_EQ(txt::from_utf16(u16.as_slice()), s);
    EXPECT_EQ(txt::from_utf32(u32.as_slice()), s);
    EXPECT_EQ(txt::to_utf16(string()).size(), 0u);
    EXPECT_EQ(txt::from_utf16(slice<const char16_t>()), string());

    // A surrogate on its own is one replacement, not a hole in the text
    char16_t lone[] = {0xD800, u'a'};
    EXPECT_EQ(txt::from_utf16(slice<const char16_t>(lone)), string("�a"));
    char32_t bad[] = {0x110000, u'a'};
    EXPECT_EQ(txt::from_utf32(slice<const char32_t>(bad)), string("�a"));

    // An odd number of bytes ends in a replacement rather than in a guess
    byte odd[] = {byte('a'), byte(0), byte('b')};
    EXPECT_EQ(txt::decode(slice<const byte>(odd), txt::encoding::utf16le), string("a�"));
}

TEST(Encoding_Tests, WhatCannotBeWrittenAndWhatCannotBeRead) {
    // A character the encoding has no room for is a question mark
    EXPECT_EQ(bytes_of(txt::encode(string("日"), txt::encoding::iso8859_2)), "?");
    EXPECT_EQ(bytes_of(txt::encode(string("Ł"), txt::encoding::latin1)), "?");
    EXPECT_EQ(bytes_of(txt::encode(string("Ł"), txt::encoding::iso8859_2)), "\xA3");
    EXPECT_EQ(bytes_of(txt::encode(string("€"), txt::encoding::windows1250)), "\x80");
    EXPECT_EQ(bytes_of(txt::encode(string("€"), txt::encoding::iso8859_2)), "?");

    // A byte that means nothing in the encoding is one replacement
    byte hole[] = {byte(0x81)};
    EXPECT_EQ(txt::decode(slice<const byte>(hole), txt::encoding::windows1250), string("�"));
    EXPECT_EQ(txt::decode(slice<const byte>(hole), txt::encoding::latin1), string("\u0081"));
    EXPECT_EQ(txt::decode(slice<const byte>(hole), txt::encoding::ascii), string("�"));

    // Invalid UTF-8 comes through as the replacement it already is
    byte broken[] = {byte('a'), byte(0xFF), byte('b')};
    EXPECT_EQ(txt::decode(slice<const byte>(broken), txt::encoding::utf8), string("a�b"));
    EXPECT_EQ(txt::decode(slice<const byte>(), txt::encoding::utf8), string());
}

// The strict form: the text, or the first byte that means nothing in the
// encoding, where the lenient form puts a replacement character
TEST(Encoding_Tests, StrictDecodeRefusesWhatLenientReplaces) {
    auto bytes = [](std::string_view s) { return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size()); };
    EXPECT_EQ(value_of(txt::decode(bytes("za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87"), txt::encoding::utf8, txt::strict)), "zażółć");
    EXPECT_EQ(value_of(txt::decode(bytes("a\xEF\xBF\xBD" "b"), txt::encoding::utf8, txt::strict)), "a\xEF\xBF\xBD" "b");   // a U+FFFD written is a character
    auto bad = txt::decode(bytes("ab\xC5"), txt::encoding::utf8, txt::strict);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().offset(), 2u);
    EXPECT_EQ(bad.error().message(), "not utf-8");
    EXPECT_EQ(txt::decode(bytes("ab\xC5"), txt::encoding::utf8), "ab\xEF\xBF\xBD");   // the lenient form, as before
    EXPECT_EQ(error_of(txt::decode(bytes("a\x80"), txt::encoding::ascii, txt::strict)).offset(), 1u);
    EXPECT_EQ(error_of(txt::decode(bytes(std::string_view("\x00" "a" "\x00", 3)), txt::encoding::utf16be, txt::strict)).offset(), 2u);   // a unit cut short
    EXPECT_EQ(error_of(txt::decode(bytes(std::string_view("\xD8\x00" "\x00" "a", 4)), txt::encoding::utf16be, txt::strict)).offset(), 0u);   // a lone surrogate
    EXPECT_TRUE(txt::decode(bytes("\xFF"), txt::encoding::latin1, txt::strict));   // every byte is a character there
}

namespace {
    slice<const byte> raw(std::string_view s) {
        return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
    }

    constexpr txt::encoding EveryEncoding[] = {
        txt::encoding::utf8, txt::encoding::utf16le, txt::encoding::utf16be, txt::encoding::utf32le,
        txt::encoding::utf32be, txt::encoding::ascii, txt::encoding::latin1, txt::encoding::ibm866,
        txt::encoding::iso8859_2, txt::encoding::windows1250, txt::encoding::windows1258, txt::encoding::x_mac_cyrillic};
}

// DESIGN 408: nothing, one unit, a unit cut short at the end, and the
// values at the edges of each encoding of Unicode
TEST(Encoding_Tests, EmptyInputInEveryEncoding) {
    for (auto e : EveryEncoding) {
        EXPECT_EQ(txt::decode(slice<const byte>(), e), string()) << txt::name_of(e);
        EXPECT_EQ(value_of(txt::decode(slice<const byte>(), e, txt::strict)), string()) << txt::name_of(e);
        EXPECT_TRUE(txt::encode(string(), e).empty()) << txt::name_of(e);
    }
    EXPECT_TRUE(txt::to_utf16(string()).empty());
    EXPECT_TRUE(txt::to_utf32(string()).empty());
    EXPECT_EQ(txt::from_utf32(slice<const char32_t>()), string());
    EXPECT_FALSE(txt::detect_bom(slice<const byte>()));
    EXPECT_EQ(txt::detect_bom(slice<const byte>()).size, 0u);
}

TEST(Encoding_Tests, UTF16CutShortAndUnpaired) {
    using enum txt::encoding;
    // A high surrogate as the last unit, in both orders
    EXPECT_EQ(txt::decode(raw(std::string_view("a\0\0\xD8", 4)), utf16le), "a�");
    EXPECT_EQ(txt::decode(raw(std::string_view("\0a\xD8\0", 4)), utf16be), "a�");
    EXPECT_EQ(error_of(txt::decode(raw(std::string_view("a\0\0\xD8", 4)), utf16le, txt::strict)).offset(), 2u);
    // A high surrogate followed by a byte that is not a whole unit: two replacements, the first error at it
    EXPECT_EQ(txt::decode(raw(std::string_view("\0\xD8" "A", 3)), utf16le), "��");
    EXPECT_EQ(error_of(txt::decode(raw(std::string_view("\0\xD8" "A", 3)), utf16le, txt::strict)).offset(), 0u);
    // A low surrogate first, then a high one: neither is a pair
    EXPECT_EQ(txt::decode(raw(std::string_view("\xDC\0\xD8\0", 4)), utf16be), "��");
    // A high surrogate followed by an ordinary unit: the unit survives
    EXPECT_EQ(txt::decode(raw(std::string_view("\xD8\0\0a", 4)), utf16be), "�a");
    // One byte alone, and the edges of the pairs
    EXPECT_EQ(txt::decode(raw("A"), utf16le), "�");
    EXPECT_EQ(error_of(txt::decode(raw("A"), utf16le, txt::strict)).offset(), 0u);
    EXPECT_EQ(value_of(txt::decode(raw(std::string_view("\xD8\0\xDC\0", 4)), utf16be, txt::strict)), "\U00010000");
    EXPECT_EQ(value_of(txt::decode(raw(std::string_view("\xDB\xFF\xDF\xFF", 4)), utf16be, txt::strict)), "\U0010FFFF");
    EXPECT_EQ(value_of(txt::decode(raw(std::string_view("\xFF\xFF", 2)), utf16le, txt::strict)), "￿");
    EXPECT_EQ(value_of(txt::decode(raw(std::string_view("\0\0", 2)), utf16le, txt::strict)), string(1, '\0'));

    // The same from units
    char16_t high_last[] = {u'a', 0xDBFF};
    EXPECT_EQ(txt::from_utf16(slice<const char16_t>(high_last)), "a�");
    char16_t reversed[] = {0xDC00, 0xD800};
    EXPECT_EQ(txt::from_utf16(slice<const char16_t>(reversed)), "��");
    char16_t edges[] = {0, 0x7F, 0x80, 0xD7FF, 0xE000, 0xFFFF, 0xDBFF, 0xDFFF};
    EXPECT_EQ(txt::from_utf16(slice<const char16_t>(edges, std::size(edges))),
              string(std::string("\0", 1) + "\x7F\u0080퟿￿\U0010FFFF"));
    EXPECT_EQ(txt::to_utf16(string("\U0010FFFF")), (vector<char16_t>{char16_t(0xDBFF), char16_t(0xDFFF)}));
}

TEST(Encoding_Tests, UTF32CutShortAndOutOfRange) {
    using enum txt::encoding;
    // A unit cut short: one replacement for the bytes left, the error at the first of them
    for (size_t extra = 1; extra < 4; ++extra) {
        std::string bytes = std::string("a\0\0\0", 4) + std::string(extra, 'b');
        EXPECT_EQ(txt::decode(raw(bytes), utf32le), "a�") << extra;
        EXPECT_EQ(error_of(txt::decode(raw(bytes), utf32le, txt::strict)).offset(), 4u) << extra;
        EXPECT_EQ(txt::decode(raw(std::string(extra, 'b')), utf32be), "�") << extra;
        EXPECT_EQ(error_of(txt::decode(raw(std::string(extra, 'b')), utf32be, txt::strict)).offset(), 0u);
    }
    // Values that are no code point: a surrogate, one past the last, the largest of 32 bits
    EXPECT_EQ(txt::decode(raw(std::string_view("\0\0\xD8\0\0\x11\0\0\xFF\xFF\xFF\xFF", 12)), utf32be), "���");
    EXPECT_EQ(error_of(txt::decode(raw(std::string_view("\0\0\0a\0\x11\0\0", 8)), utf32be, txt::strict)).offset(), 4u);
    EXPECT_EQ(value_of(txt::decode(raw(std::string_view("\xFF\xFF\x10\0", 4)), utf32le, txt::strict)), "\U0010FFFF");

    char32_t points[] = {0, 0x10FFFF, 0xD800, 0xDFFF, 0x110000, 0xFFFFFFFF};
    EXPECT_EQ(txt::from_utf32(slice<const char32_t>(points, std::size(points))), string("\0\U0010FFFF����", 17));
    auto back = txt::to_utf32(string("\0\U0010FFFF", 5));
    EXPECT_EQ(back, (vector<char32_t>{U'\0', U'\U0010FFFF'}));
}

TEST(Encoding_Tests, StrictUTF8FindsTheFirstBadByteAnywhere) {
    using enum txt::encoding;
    EXPECT_EQ(error_of(txt::decode(raw("\x80" "abc"), utf8, txt::strict)).offset(), 0u);          // at the start
    EXPECT_EQ(error_of(txt::decode(raw("abc\xF0\x9F\x98"), utf8, txt::strict)).offset(), 3u);      // cut at the end
    EXPECT_EQ(error_of(txt::decode(raw("\xC0\xAF"), utf8, txt::strict)).offset(), 0u);             // overlong
    EXPECT_EQ(error_of(txt::decode(raw("a\xED\xA0\x80"), utf8, txt::strict)).offset(), 1u);        // an encoded surrogate
    EXPECT_EQ(error_of(txt::decode(raw("\xF4\x90\x80\x80"), utf8, txt::strict)).offset(), 0u);     // past U+10FFFF
    EXPECT_EQ(error_of(txt::decode(raw("ab\xFF"), utf8, txt::strict)).offset(), 2u);              // the last byte
    // The lenient form: one replacement a byte that starts no sequence
    EXPECT_EQ(txt::decode(raw("abc\xF0\x9F\x98"), utf8), "abc���");
    // A single byte encoding: the hole at the end
    EXPECT_EQ(error_of(txt::decode(raw("abc\x81"), windows1250, txt::strict)).offset(), 3u);
    EXPECT_EQ(error_of(txt::decode(raw("abc\x81"), windows1250, txt::strict)).from(), windows1250);
}

TEST(Encoding_Tests, EncodeTheEdges) {
    using enum txt::encoding;
    // An invalid sequence of the text is a U+FFFD in every encoding of Unicode and a '?' in the rest
    string broken("a\xFF");
    EXPECT_EQ(bytes_of(txt::encode(broken, utf8)), "a\xEF\xBF\xBD");
    EXPECT_EQ(bytes_of(txt::encode(broken, utf16be)), std::string("\0a\xFF\xFD", 4));
    EXPECT_EQ(bytes_of(txt::encode(broken, utf32le)), std::string("a\0\0\0\xFD\xFF\0\0", 8));
    EXPECT_EQ(bytes_of(txt::encode(broken, ascii)), "a?");
    EXPECT_EQ(bytes_of(txt::encode(broken, latin1)), "a?");
    EXPECT_EQ(bytes_of(txt::encode(broken, windows1250)), "a?");
    // The edges: U+0000, U+007F, U+0080, U+00FF, U+0100, U+FFFF, U+10000, U+10FFFF
    string edges("\0\x7F\u0080ÿĀ￿\U00010000\U0010FFFF", 19);
    EXPECT_EQ(bytes_of(txt::encode(edges, ascii)), std::string("\0\x7F??????", 8));
    EXPECT_EQ(bytes_of(txt::encode(edges, latin1)), std::string("\0\x7F\x80\xFF????", 8));
    EXPECT_EQ(bytes_of(txt::encode(edges, utf16le)),
              std::string("\0\0\x7F\0\x80\0\xFF\0\0\x01\xFF\xFF\0\xD8\0\xDC\xFF\xDB\xFF\xDF", 20));
    EXPECT_EQ(bytes_of(txt::encode(edges, utf32be)).size(), 32u);
    EXPECT_EQ(txt::decode(txt::encode(edges, utf32be).as_slice(), utf32be), edges);
    EXPECT_EQ(txt::decode(txt::encode(edges, utf16le).as_slice(), utf16le), edges);
    // to_utf16 sizes by the bytes: invalid bytes are a unit each, no overrun
    EXPECT_EQ(txt::to_utf16(string("\xFF\xFF\xFF\xFF")).size(), 4u);
    EXPECT_EQ(txt::to_utf32(string("\xF0\x9F\x98")).size(), 3u);
}

// A valid text is its own UTF-8: the bytes come back as they went in, for
// a character of every width (and a written U+FFFD) at every length the
// copy takes a different road at; an invalid byte anywhere — first, last,
// after a long valid run, every byte — is a U+FFFD of three
TEST(Encoding_Tests, ValidTextIsItsOwnUTF8) {
    using enum txt::encoding;
    const std::string_view widths[] = {"a", "\xC5\xBC", "\xE2\x82\xAC", "\xF0\x9F\x98\x80", "\xEF\xBF\xBD", std::string_view("\0", 1)};
    for (auto w : widths) {
        std::string text;
        for (size_t n = 0; n <= 700; n = n < 70 ? n + 1 : n * 2) {
            while (text.size() < n * w.size()) {
                text.append(w);
            }
            string s(text.data(), n * w.size());
            EXPECT_EQ(bytes_of(txt::encode(s, utf8)), std::string(s.data(), s.size())) << n << " x " << w.size();
        }
    }
    std::string mixed("a\xC5\xBC\xE2\x82\xAC\xF0\x9F\x98\x80\xEF\xBF\xBD\x7F\xC2\x80\xF4\x8F\xBF\xBF");
    for (size_t k = 0; k < 8; ++k) {
        mixed += mixed;
    }
    EXPECT_EQ(bytes_of(txt::encode(string(mixed.data(), mixed.size()), utf8)), mixed);

    const std::string fffd("\xEF\xBF\xBD");
    EXPECT_EQ(bytes_of(txt::encode(string("\xFF" "abc"), utf8)), fffd + "abc");
    EXPECT_EQ(bytes_of(txt::encode(string(mixed.data(), mixed.size()) + string("\xC5"), utf8)), mixed + fffd);
    std::string bad(300, '\x80'), want;
    for (size_t i = 0; i < bad.size(); ++i) {
        want += fffd;
    }
    EXPECT_EQ(bytes_of(txt::encode(string(bad.data(), bad.size()), utf8)), want);
}

TEST(Encoding_Tests, ByteOrderMarksCutShort) {
    auto bom = [](std::string_view s) { return txt::detect_bom(raw(s)); };
    EXPECT_FALSE(bom("\xEF"));
    EXPECT_FALSE(bom("\xFF"));
    EXPECT_FALSE(bom("\xFE"));
    EXPECT_FALSE(bom(std::string_view("\0\0\xFE", 3)));                       // the UTF-32 one cut short
    EXPECT_FALSE(bom(std::string_view("\0\0\xFF\xFE", 4)));                   // the wrong order
    EXPECT_FALSE(bom("\xBB\xBF"));
    EXPECT_EQ(bom("\xEF\xBB\xBF").size, 3u);                                  // the mark and nothing else
    EXPECT_EQ(bom(std::string_view("\xFF\xFE\0", 3)).says, txt::encoding::utf16le);   // three bytes are no UTF-32 mark
    EXPECT_EQ(bom(std::string_view("\xFF\xFE\0", 3)).size, 2u);
    EXPECT_EQ(bom(std::string_view("\0\0\xFE\xFF", 4)).size, 4u);
    EXPECT_EQ(bom("\xFE\xFF").says, txt::encoding::utf16be);
}

TEST(Encoding_Tests, OddNames) {
    using txt::encoding;
    EXPECT_EQ(txt::encoding_from_name(string(" utf_8 ")), encoding::utf8);      // the separators anywhere
    EXPECT_EQ(txt::encoding_from_name(string("U-T-F-8")), encoding::utf8);
    EXPECT_FALSE(txt::encoding_from_name(string("-")).has_value());
    EXPECT_FALSE(txt::encoding_from_name(string("---___   ")).has_value());
    EXPECT_FALSE(txt::encoding_from_name(string("utf-8", 6)).has_value());     // a NUL is not a separator
    EXPECT_FALSE(txt::encoding_from_name(string("utf-8x")).has_value());
    EXPECT_FALSE(txt::encoding_from_name(string("utf")).has_value());          // a prefix is not a name
    EXPECT_FALSE(txt::encoding_from_name(string(std::string(100000, 'a'))).has_value());
    EXPECT_FALSE(txt::encoding_from_name(string("ütf-8")).has_value());        // only ASCII folds

    // decode_error: a value, equal by its fields
    txt::decode_error a(3, encoding::ascii), b(3, encoding::ascii), c(4, encoding::ascii), d(3, encoding::utf8);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
    EXPECT_EQ(a.message(), "not us-ascii");
    txt::decode_error far(SIZE_MAX, encoding::x_mac_cyrillic);
    EXPECT_EQ(far.offset(), SIZE_MAX);
    EXPECT_EQ(far.message(), string("not ") + txt::name_of(encoding::x_mac_cyrillic));
}

// The bound a text is written into is three bytes an input byte or unit:
// past a third of a string's max_size() the call refuses before reading
// anything, so a slice that only claims the size shows it
TEST(Encoding_Tests, TheLengthLimit) {
    static byte one[1] = {};
    static char16_t unit[1] = {};
    const size_t over = string::max_size() / 3 + 1;
    EXPECT_THROW((void)txt::decode(slice<const byte>(one, over), txt::encoding::utf8), length_error);
    EXPECT_THROW((void)txt::decode(slice<const byte>(one, over), txt::encoding::latin1, txt::strict), length_error);
    EXPECT_THROW((void)txt::from_utf16(slice<const char16_t>(unit, over)), length_error);
}

// decode and encode by a name: what the encoding the name stands for
// gives, under any of its spellings; a name nobody knows (empty, a NUL in
// it, a prefix) nothing at all; no bytes and no text; the length limit
// of decode's; and the way back
TEST(Encoding_Tests, ByAName) {
    using txt::encoding;
    const byte latin2[] = {byte(0xB1), byte(0x62), byte(0xEA)};   // "ąbę" in ISO-8859-2
    for (const char* name : {"iso-8859-2", "ISO_8859-2", "latin2", " iso88592 "}) {
        auto text = txt::decode(slice<const byte>(latin2, 3), string(name));
        ASSERT_TRUE(text) << name;
        EXPECT_EQ(*text, "ąbę") << name;
        auto back = txt::encode(*text, string(name));
        ASSERT_TRUE(back) << name;
        EXPECT_EQ(*back, txt::encode(*text, encoding::iso8859_2));
    }
    for (auto e : {encoding::utf8, encoding::utf16le, encoding::utf32be, encoding::ascii, encoding::windows1250}) {
        string name(txt::name_of(e));
        auto bytes = txt::encode(string("zażółć ?"), e);
        EXPECT_EQ(txt::decode(bytes, name), txt::decode(bytes, e)) << name.view();
        EXPECT_EQ(txt::encode(string("zażółć ?"), name), bytes) << name.view();
    }
    for (const char* name : {"", "klingon", "utf", "utf-8x"}) {
        EXPECT_FALSE(txt::decode(slice<const byte>(latin2, 3), string(name))) << name;
        EXPECT_FALSE(txt::encode(string("x"), string(name))) << name;
    }
    EXPECT_FALSE(txt::decode(slice<const byte>(latin2, 3), string("utf-8", 6)));
    EXPECT_EQ(txt::decode(slice<const byte>(), string("utf-8")), string());
    EXPECT_TRUE(txt::encode(string(), string("utf-16le"))->empty());
    auto bad = txt::decode(slice<const byte>(latin2, 3), string("utf-8"));   // not UTF-8: replaced, as decode does
    ASSERT_TRUE(bad);
    EXPECT_EQ(*bad, txt::decode(slice<const byte>(latin2, 3), encoding::utf8));
    static byte one[1] = {};
    EXPECT_THROW((void)txt::decode(slice<const byte>(one, string::max_size() / 3 + 1), string("utf-8")), length_error);
    EXPECT_FALSE(txt::decode(slice<const byte>(one, string::max_size() / 3 + 1), string("utf-9")));   // the name first
    static_assert(noexcept(txt::encode(string(), string())));
}
