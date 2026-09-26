//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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
    EXPECT_EQ(*txt::decode(bytes("za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87"), txt::encoding::utf8, txt::strict), "zażółć");
    EXPECT_EQ(*txt::decode(bytes("a\xEF\xBF\xBD" "b"), txt::encoding::utf8, txt::strict), "a\xEF\xBF\xBD" "b");   // a U+FFFD written is a character
    auto bad = txt::decode(bytes("ab\xC5"), txt::encoding::utf8, txt::strict);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().offset(), 2u);
    EXPECT_EQ(bad.error().message(), "not utf-8");
    EXPECT_EQ(txt::decode(bytes("ab\xC5"), txt::encoding::utf8), "ab\xEF\xBF\xBD");   // the lenient form, as before
    EXPECT_EQ(txt::decode(bytes("a\x80"), txt::encoding::ascii, txt::strict).error().offset(), 1u);
    EXPECT_EQ(txt::decode(bytes(std::string_view("\x00" "a" "\x00", 3)), txt::encoding::utf16be, txt::strict).error().offset(), 2u);   // a unit cut short
    EXPECT_EQ(txt::decode(bytes(std::string_view("\xD8\x00" "\x00" "a", 4)), txt::encoding::utf16be, txt::strict).error().offset(), 0u);   // a lone surrogate
    EXPECT_TRUE(txt::decode(bytes("\xFF"), txt::encoding::latin1, txt::strict));   // every byte is a character there
}
