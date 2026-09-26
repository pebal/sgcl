//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::error: the code and its category, the message, and the line
// and the column counted after the fact from the offset.
#include "common.h"

using namespace sgcl::encoding;

#include <string>

using namespace enc_test;

TEST(EncodingError_Tests, CodeAndCategory) {
    std::error_code c = encoding::errc::invalid_character;
    EXPECT_EQ(c.category().name(), std::string_view("encoding"));
    EXPECT_EQ(c.message(), "invalid character");
    EXPECT_TRUE(bool(c));   // the codes start at 1: none of them is success
    EXPECT_EQ(make_error_code(encoding::errc::syntax).value(), 1);
    for (int i = 1; i <= int(encoding::errc::io); ++i) {
        EXPECT_NE(encoding_category().message(i), "unknown encoding error") << i;
    }
    // the same code travels inside an io::error
    io::error e(encoding::errc::out_of_range, "read", "varint");
    EXPECT_EQ(std::string_view(e.message()), "read varint: value out of range");
}

TEST(EncodingError_Tests, Message) {
    encoding::error e(encoding::errc::invalid_character, 17, "invalid character '*'");
    EXPECT_EQ(e.code(), encoding::errc::invalid_character);
    EXPECT_EQ(e.offset(), 17u);
    EXPECT_EQ(e.line(), 0u);
    EXPECT_EQ(e.column(), 0u);
    EXPECT_TRUE(e.path().empty());
    EXPECT_FALSE(e.io_error().has_value());
    EXPECT_EQ(e.message(), "offset 17: invalid character '*'");
    // no detail: the code's own words
    EXPECT_EQ(encoding::error(encoding::errc::unexpected_end, 3).message(), "offset 3: unexpected end of input");
    // a position and a path
    encoding::error typed(encoding::errc::type_mismatch, 40, "expected a number, found a string");
    typed.set_position(3, 14).set_path("/users/3/age");
    EXPECT_EQ(typed.message(), "3:14 /users/3/age: expected a number, found a string");
    // a failure of the stream under it
    encoding::error failed(io::error(std::make_error_code(std::errc::io_error), "read", "log.json"), 9);
    EXPECT_EQ(failed.code(), encoding::errc::io);
    ASSERT_TRUE(failed.io_error().has_value());
    EXPECT_EQ(std::string_view(failed.message()).substr(0, 43), "offset 9: input/output error: read log.json");
    EXPECT_EQ(encoding::error(encoding::errc::syntax, 1), encoding::error(encoding::errc::syntax, 1));
    EXPECT_NE(encoding::error(encoding::errc::syntax, 1), encoding::error(encoding::errc::syntax, 2));
    // everything it says counts: the words, the stream's error
    EXPECT_NE(encoding::error(encoding::errc::syntax, 1, "a"), encoding::error(encoding::errc::syntax, 1, "b"));
    auto io_of = [](std::errc c) { return io::error(std::make_error_code(c), "read"); };
    EXPECT_NE(encoding::error(io_of(std::errc::io_error), 1), encoding::error(io_of(std::errc::timed_out), 1));
    EXPECT_EQ(encoding::error(io_of(std::errc::io_error), 1), encoding::error(io_of(std::errc::io_error), 1));
}

// The line and the column of an offset, counted when asked: a line ends at
// '\n', its '\r' the last character of the line; the column counts code
// points, not bytes
TEST(EncodingError_Tests, LineAndColumnAfterTheFact) {
    struct Case {
        const char* text;
        uint64_t offset;
        uint32_t line;
        uint32_t column;
    };
    const Case cases[] = {
        {"", 0, 1, 1},
        {"abc", 0, 1, 1},
        {"abc", 2, 1, 3},
        {"abc", 3, 1, 4},
        {"abc", 100, 1, 4},              // past the end: the end
        {"a\nb", 1, 1, 2},               // the '\n' is the end of its line
        {"a\nb", 2, 2, 1},
        {"a\r\nb", 1, 1, 2},
        {"a\r\nb", 3, 2, 1},
        {"\n\n\nx", 3, 4, 1},
        {"zażółć x", 10, 1, 7},          // six letters, four of them two bytes
        {"ab\n€€x", 9, 2, 3},            // the euro is three bytes
        {"😀x", 4, 1, 2},
    };
    for (auto& c : cases) {
        encoding::error e(encoding::errc::syntax, c.offset);
        e.locate(c.text);
        EXPECT_EQ(e.line(), c.line) << c.text << " at " << c.offset;
        EXPECT_EQ(e.column(), c.column) << c.text << " at " << c.offset;
    }
    encoding::error e(encoding::errc::syntax, 5, "unexpected ','");
    EXPECT_EQ(e.locate("[1,\n2,,3]").message(), "2:2: unexpected ','");
}
