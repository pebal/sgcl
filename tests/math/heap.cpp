//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What formatting a big_integer leaves on the managed heap (the audit of
// 2026-09-26): the digits are written from the limbs into plain memory,
// where they went through abs() and to_string(), a managed string made and
// dropped for every value. The test failed on the header before the
// change: two objects a value then, one now.
#include "tests/managed_pages.h"
#include "sgcl/math/math.h"

#include <sstream>
#include <string>

using math::big_integer;

TEST(MathHeap_Tests, AFormattedValueIsItsStringAlone) {
    std::string hex;
    for (int i = 0; i < 128; ++i) {
        hex += "0123456789abcdef"[(i * 7) % 16];
    }
    hex[0] = 'f';
    auto big = *big_integer::parse(string(hex), 16);
    auto negative = -big;
    string decimal = txt::format("{}", big);
    EXPECT_EQ(decimal, big.to_string());
    EXPECT_EQ(txt::format("{:x}", negative), "-" + hex);
    EXPECT_EQ(txt::format("{:#X}", big).view().substr(0, 4), "0XF7");
    EXPECT_EQ(txt::format("{:>8x}", big_integer(-255)), "     -ff");
    EXPECT_EQ(txt::format("{}", big_integer(int64_t(INT64_MIN))), "-9223372036854775808");
    EXPECT_EQ(txt::format("{:b}", big_integer(0)), "0");
    std::ostringstream os;
    os << std::hex << negative;
    EXPECT_EQ(os.str(), "-" + hex);
    size_t answer = heap_count::pages_of(20000, [&] {
        EXPECT_EQ(string(decimal.data(), decimal.size()).size(), decimal.size());
    });
    size_t pages = heap_count::pages_of(20000, [&] {
        EXPECT_EQ(txt::format("{}", big).size(), decimal.size());
    });
    EXPECT_LE(pages, answer + 1);
    pages = heap_count::pages_of(20000, [&] {
        EXPECT_EQ(txt::format("{:x}", negative).size(), hex.size() + 1);
    });
    EXPECT_LE(pages, answer + 1);
}
