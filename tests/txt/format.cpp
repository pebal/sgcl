//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::format: the pattern of std::format, read where the program is
// built. What is checked here is that every specification writes what the
// standard says it writes, that a field over text is measured in the
// columns it takes and not in its bytes, that format_to says what the
// whole would have taken when it did not fit, and that a type of the
// caller's teaches this how to write it through a format_value found
// beside it.
#include "tests/types.h"

#include <bit>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <format>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <random>
#include <string>

namespace {
    // A type of the caller's, written by a function in its own namespace
    struct Point {
        int x;
        int y;
    };

    void format_value(txt::format_sink& out, const Point& p, const txt::format_spec& spec) {
        char room[32];
        int n = std::snprintf(room, sizeof room, "(%d, %d)", p.x, p.y);
        txt::detail::put_padded(out, {room, size_t(n)}, spec);
    }

}

TEST(Format_Tests, TheWholeOfTheIntegerSpecifications) {
    EXPECT_EQ(txt::format("{}", 42), "42");
    EXPECT_EQ(txt::format("{:d}", -7), "-7");
    EXPECT_EQ(txt::format("{:b} {:B}", 5, 5), "101 101");
    EXPECT_EQ(txt::format("{:o}", 8), "10");
    EXPECT_EQ(txt::format("{:x} {:X}", 255, 255), "ff FF");
    EXPECT_EQ(txt::format("{:#x} {:#b} {:#o}", 255, 5, 8), "0xff 0b101 010");
    EXPECT_EQ(txt::format("{:#X} {:#B}", 255, 5), "0XFF 0B101");
    EXPECT_EQ(txt::format("{:#o} {:#x}", 0, 0), "0 0x0");   // octal puts no second zero
    EXPECT_EQ(txt::format("{:+} {:+} {: }", 7, -7, 7), "+7 -7  7");
    EXPECT_EQ(txt::format("{:05}", 42), "00042");
    EXPECT_EQ(txt::format("{:05}", -42), "-0042");
    EXPECT_EQ(txt::format("{:#06x}", 255), "0x00ff");
    EXPECT_EQ(txt::format("{:c}", 65), "A");
}

TEST(Format_Tests, TheWholeOfTheFieldSpecifications) {
    EXPECT_EQ(txt::format("[{:<6}]", 42), "[42    ]");
    EXPECT_EQ(txt::format("[{:>6}]", 42), "[    42]");
    EXPECT_EQ(txt::format("[{:^6}]", 42), "[  42  ]");
    EXPECT_EQ(txt::format("[{:*<6}]", 42), "[42****]");
    EXPECT_EQ(txt::format("[{:*>6}]", 42), "[****42]");
    EXPECT_EQ(txt::format("[{:*^6}]", 42), "[**42**]");
    // Text goes to the left of its field and a number to the right
    EXPECT_EQ(txt::format("[{:6}]", "ab"), "[ab    ]");
    EXPECT_EQ(txt::format("[{:6}]", 42), "[    42]");
}

TEST(Format_Tests, TheFloatingSpecifications) {
    EXPECT_EQ(txt::format("{:.3f}", 3.14159), "3.142");
    EXPECT_EQ(txt::format("{:.2f}", -0.5), "-0.50");
    EXPECT_EQ(txt::format("{:.2e}", 1234.5), "1.23e+03");
    EXPECT_EQ(txt::format("{:08.3f}", 3.14159), "0003.142");
    EXPECT_EQ(txt::format("{:+.1f}", 2.0), "+2.0");
    EXPECT_EQ(txt::format("{}", 0.5), "0.5");
    // A named form has a precision even when none is given: six, as
    // printf has always had it. Only {} and the hexadecimal form write
    // the shortest text that reads back as the same number.
    EXPECT_EQ(txt::format("{:f}", 0.5), "0.500000");
    EXPECT_EQ(txt::format("{:e}", 0.5), "5.000000e-01");
    EXPECT_EQ(txt::format("{:E}", 1234.5), "1.234500E+03");
    EXPECT_EQ(txt::format("{:g}", 3.14159265358979), "3.14159");
    EXPECT_EQ(txt::format("{}", 3.14159265358979), "3.14159265358979");
}

TEST(Format_Tests, TheOtherValues) {
    EXPECT_EQ(txt::format("{} {}", true, false), "true false");
    EXPECT_EQ(txt::format("{:d} {:d}", true, false), "1 0");
    EXPECT_EQ(txt::format("{}", 'x'), "x");
    EXPECT_EQ(txt::format("{}", U'ż'), "ż");    // a code point comes out as UTF-8
    EXPECT_EQ(txt::format("{}", string("nasz")), "nasz");
    EXPECT_EQ(txt::format("{}", std::string_view("std")), "std");
    EXPECT_EQ(txt::format("{}", "literal"), "literal");
    EXPECT_EQ(txt::format("{}", string("abc").as_slice(1, 2)), "bc");
}

TEST(Format_Tests, ABraceThatStandsForItself) {
    EXPECT_EQ(txt::format("{{literal}} {}", true), "{literal} true");
    EXPECT_EQ(txt::format("{{}}"), "{}");
    EXPECT_EQ(txt::format("nothing at all"), "nothing at all");
}

// The point of putting this in txt: a field of text is what it takes on a
// terminal. "zolc" with its Polish marks is eight bytes and four columns,
// and a field of ten pads it by six; two CJK ideographs are four columns
// and not two, as East Asian Wide says.
TEST(Format_Tests, AFieldOfTextIsMeasuredInColumnsAndNotInBytes) {
    EXPECT_EQ(txt::format("[{:>10}]", "żółć"), "[      żółć]");
    EXPECT_EQ(txt::format("[{:>10}]", "zolc"), "[      zolc]");
    EXPECT_EQ(txt::format("[{:>8}]", "日本"), "[    日本]");
    EXPECT_EQ(txt::format("[{:^8}]", "日本"), "[  日本  ]");
    // A combining mark takes no column of its own
    EXPECT_EQ(txt::format("[{:>4}]", "á"), "[   á]");
}

// And a precision cuts to that many columns, stopping on a code point: by
// its bytes it would leave a lead byte with nothing behind it, which is
// not text any more
TEST(Format_Tests, APrecisionStopsOnACodePoint) {
    EXPECT_EQ(txt::format("{:.3}", "żółć"), "żół");
    EXPECT_EQ(txt::format("{:.4}", "日本語"), "日本");
    EXPECT_EQ(txt::format("{:.3}", "日本語"), "日");   // no half an ideograph
    EXPECT_EQ(txt::format("{:.2}", "abcdef"), "ab");
    EXPECT_EQ(txt::format("{:.9}", "abc"), "abc");
    EXPECT_EQ(txt::format("[{:>6.2}]", "abcdef"), "[    ab]");
    EXPECT_TRUE(utf8::valid(txt::format("{:.3}", "żółć").view()));
}

TEST(Format_Tests, WritingIntoABufferOfTheCallers) {
    char room[64];
    auto whole = slice<char>(room, room + sizeof room);
    EXPECT_EQ(txt::format_to(whole, "{} left", 42), 7u);
    EXPECT_EQ(std::string(room, 7), "42 left");
    // Too small: what fits is written and what the whole takes is said
    char small[8];
    auto part = slice<char>(small, small + sizeof small);
    EXPECT_EQ(txt::format_to(part, "{} and {} left", 42, 7), 13u);
    EXPECT_EQ(std::string(small, 8), "42 and 7");
    // Nothing at all still says how much room it would want
    auto none = slice<char>(room, room);
    EXPECT_EQ(txt::format_to(none, "{}", 1234), 4u);
}

TEST(Format_Tests, ATypeOfTheCallersTeachesItselfThroughFormatValue) {
    EXPECT_EQ(txt::format("{}", Point{1, 2}), "(1, 2)");
    EXPECT_EQ(txt::format("[{:>10}]", Point{3, 4}), "[    (3, 4)]");
}

// A pattern is a constant expression, built and checked where the program
// is. The comma forces the evaluation, which is where a pattern that does
// not fit its values throws and so stops the build; that half cannot be
// asserted from inside a translation unit that has to compile, and is
// checked by tests/txt/format_rejects.cpp, which must not.
TEST(Format_Tests, APatternIsReadWhereTheProgramIsBuilt) {
    static_assert((txt::format_pattern<int>("{}"), true));
    static_assert((txt::format_pattern<const char*>("{:s}"), true));
    static_assert((txt::format_pattern<double>("{:.3}"), true));
    static_assert((txt::format_pattern<int, string>("{:#x} {:>4}"), true));
    static_assert((txt::format_pattern<>("{{}}"), true));
}

// Decimal is written here and not by the standard, without a branch on
// how many digits there are. It has to agree with the standard on every
// value, so it is asked: exhaustively over the small ones, where a digit
// is gained at every power of ten, at the two sides of every power, and
// over a few million taken at random with every bit width.
TEST(Format_Tests, EveryWholeNumberIsWrittenAsTheStandardWritesIt) {
    char ours[64];
    char theirs[64];
    auto agrees = [&](auto v) {
        size_t n = txt::format_to(slice<char>(ours, ours + sizeof ours), "{}", v);
        auto r = std::to_chars(theirs, theirs + sizeof theirs, v);
        return n == size_t(r.ptr - theirs) && !std::memcmp(ours, theirs, n);
    };
    for (uint32_t v = 0; v <= 300000; ++v) {
        ASSERT_TRUE(agrees(v)) << v;
        ASSERT_TRUE(agrees(int32_t(v))) << v;
        ASSERT_TRUE(agrees(-int32_t(v))) << v;
    }
    for (uint64_t p = 1; p <= 10000000000000000000ull; p *= 10) {
        for (int64_t d = -2; d <= 2; ++d) {
            ASSERT_TRUE(agrees(uint64_t(p) + uint64_t(d))) << p << " " << d;
        }
    }
    ASSERT_TRUE(agrees(uint32_t(0xFFFFFFFF)));
    ASSERT_TRUE(agrees(uint64_t(0xFFFFFFFFFFFFFFFFull)));
    ASSERT_TRUE(agrees(int32_t(-2147483647 - 1)));
    ASSERT_TRUE(agrees(int64_t(-9223372036854775807LL - 1)));
    std::mt19937_64 rng(1);
    for (int k = 0; k < 400000; ++k) {
        uint64_t x = rng();
        ASSERT_TRUE(agrees(x >> (rng() % 64)));
        ASSERT_TRUE(agrees(uint32_t(x) >> (rng() % 32)));
        ASSERT_TRUE(agrees(int64_t(x)));
    }
}

// A whole number below 2^53 is written here and not by the standard: the
// interval of values that read back as it is narrower than one, so its
// digits without their trailing zeros are the shortest representation,
// and the shape is the shorter of the plain and the exponential one with
// the plain taking a tie. All of that has to agree with the standard.
TEST(Format_Tests, AWholeNumberIsWrittenAsTheStandardWritesIt) {
    char ours[64];
    char theirs[64];
    auto agrees = [&](double v) {
        size_t n = txt::format_to(slice<char>(ours, ours + sizeof ours), "{}", v);
        auto r = std::to_chars(theirs, theirs + sizeof theirs, v);
        return n == size_t(r.ptr - theirs) && !std::memcmp(ours, theirs, n);
    };
    // Where the shape changes: 1000 stays plain, 100000 goes exponential
    EXPECT_EQ(txt::format("{}", 100.0), "100");
    EXPECT_EQ(txt::format("{}", 1000.0), "1000");
    EXPECT_EQ(txt::format("{}", 10000.0), "10000");
    EXPECT_EQ(txt::format("{}", 100000.0), "1e+05");
    EXPECT_EQ(txt::format("{}", 123456.0), "123456");
    EXPECT_EQ(txt::format("{}", 1200.0), "1200");
    EXPECT_EQ(txt::format("{}", -0.0), "-0");
    EXPECT_EQ(txt::format("{}", 0.0), "0");
    for (long i = 0; i <= 400000; ++i) {
        ASSERT_TRUE(agrees(double(i))) << i;
        ASSERT_TRUE(agrees(-double(i))) << i;
    }
    for (double p = 1; p < 1e18; p *= 10) {
        for (long d = -3; d <= 3; ++d) {
            ASSERT_TRUE(agrees(p + double(d))) << p << " " << d;
            ASSERT_TRUE(agrees(-(p + double(d)))) << p << " " << d;
        }
    }
    std::mt19937_64 rng(2);
    for (int k = 0; k < 300000; ++k) {
        uint64_t x = rng() % 9007199254740992ull;
        ASSERT_TRUE(agrees(double(x))) << x;
        ASSERT_TRUE(agrees(double(x >> (rng() % 53)))) << x;
        ASSERT_TRUE(agrees(double(rng() % 1000) * double(1ull << (rng() % 30))));
    }
    // And the values the fast road must not take
    ASSERT_TRUE(agrees(9007199254740992.0));
    ASSERT_TRUE(agrees(1e300));
    ASSERT_TRUE(agrees(0.5));
    ASSERT_TRUE(agrees(2.5));
    ASSERT_TRUE(agrees(1e22));
}

// A float is a type of its own and not a narrow double. The shortest
// text that reads back as 1.1f is "1.1"; the shortest that reads back as
// the double 1.1f widens to is "1.100000023841858" — the same number, a
// different question, and the standard answers the first one.
TEST(Format_Tests, AFloatIsWrittenAsAFloat) {
    EXPECT_EQ(txt::format("{}", 1.1f), "1.1");
    EXPECT_EQ(txt::format("{}", 0.1f), "0.1");
    EXPECT_EQ(txt::format("{}", 3.14f), "3.14");
    EXPECT_EQ(txt::format("{}", 1.0f / 3.0f), "0.33333334");
    EXPECT_EQ(txt::format("{}", 1e20f), "1e+20");
    EXPECT_EQ(txt::format("{}", 16777216.0f), "16777216");
    // And the same value as a double is the other answer
    EXPECT_EQ(txt::format("{}", double(1.1f)), "1.100000023841858");
    EXPECT_EQ(txt::format("{}", 1.1), "1.1");

    char ours[64];
    char theirs[64];
    auto agrees = [&](float v) {
        size_t n = txt::format_to(slice<char>(ours, ours + sizeof ours), "{}", v);
        auto r = std::to_chars(theirs, theirs + sizeof theirs, v);
        return n == size_t(r.ptr - theirs) && !std::memcmp(ours, theirs, n);
    };
    // The whole numbers take the road that writes them without the
    // standard, and for a float it ends at 2^24 and not at 2^53
    for (long i = 0; i <= 200000; ++i) {
        ASSERT_TRUE(agrees(float(i))) << i;
        ASSERT_TRUE(agrees(-float(i))) << i;
    }
    for (float p = 1; p < 1e18f; p *= 10) {
        for (long d = -3; d <= 3; ++d) {
            ASSERT_TRUE(agrees(p + float(d))) << p << " " << d;
        }
    }
    ASSERT_TRUE(agrees(16777216.0f));
    ASSERT_TRUE(agrees(16777215.0f));
    ASSERT_TRUE(agrees(33554432.0f));
    std::mt19937_64 rng(3);
    for (int k = 0; k < 400000; ++k) {
        float v = std::bit_cast<float>(uint32_t(rng()));
        if (!std::isfinite(v)) {
            continue;
        }
        ASSERT_TRUE(agrees(v)) << double(v);
    }
}

// The six the review found. Each is a case the standard answers one way
// and this answered another, and none of them was reached by the oracle
// or by the tests before: infinity written in the uppercase fixed form,
// the alternate form over a floating value, a character asked of a
// number that is not one, a width past what its accumulator holds, and
// a conversion the buffer on the stack could not hold.
TEST(Format_Tests, TheCornersTheReviewFound) {
    double inf = std::numeric_limits<double>::infinity();
    double nan = std::numeric_limits<double>::quiet_NaN();

    // {:F} upper-cases what it writes, which for a finite value is
    // nothing and for these two is everything
    EXPECT_EQ(txt::format("{:F}", inf), "INF");
    EXPECT_EQ(txt::format("{:F}", -inf), "-INF");
    EXPECT_EQ(txt::format("{:F}", nan), "NAN");
    EXPECT_EQ(txt::format("{:f}", inf), "inf");
    EXPECT_EQ(txt::format("{:E}", inf), "INF");

    // '#' keeps the point, and over the general form the trailing zeros
    EXPECT_EQ(txt::format("{:#}", 1.0), "1.");
    EXPECT_EQ(txt::format("{:#}", 100.0), "100.");
    EXPECT_EQ(txt::format("{:#}", 1e20), "1.e+20");
    EXPECT_EQ(txt::format("{:#}", 1.5), "1.5");
    EXPECT_EQ(txt::format("{:#g}", 1.0), "1.00000");
    EXPECT_EQ(txt::format("{:#g}", 1e20), "1.00000e+20");
    EXPECT_EQ(txt::format("{:#.3g}", 1.0), "1.00");
    EXPECT_EQ(txt::format("{:#.1g}", 1.0), "1.");
    EXPECT_EQ(txt::format("{:#.0f}", 1.0), "1.");
    EXPECT_EQ(txt::format("{:#.0e}", 1.0), "1.e+00");
    EXPECT_EQ(txt::format("{:#010}", 1.0), "000000001.");
    EXPECT_EQ(txt::format("{:#}", inf), "inf");

    // A precision the room on the stack cannot hold is not a reason to
    // write what the stack happened to contain
    EXPECT_EQ(txt::format("{:.100f}", 1.0).size(), 102u);
    EXPECT_EQ(txt::format("{:.100f}", 1.0).view().substr(0, 3), "1.0");
    EXPECT_EQ(txt::format("{:.40f}", 1e30).size(), 72u);
    EXPECT_EQ(txt::format("{:.200e}", 1.0).size(), 206u);   // "1." + 200 digits + "e+00"

    // {:c} of a number no character holds is refused, not narrowed
    EXPECT_EQ(txt::format("{:c}", 65), "A");
    EXPECT_EQ(txt::format("{:c}", -1).size(), 1u);
    EXPECT_THROW((void)txt::format("{:c}", 300), std::out_of_range);
    EXPECT_THROW((void)txt::format("{:c}", 128), std::out_of_range);
    EXPECT_THROW((void)txt::format("{:c}", -129), std::out_of_range);
    EXPECT_THROW((void)txt::format("{:c}", 1000000), std::out_of_range);

    // A width that would wrap around its accumulator is refused where
    // the program is built — {:4294967297} used to pad to one — and a
    // width that merely passes what a step holds is not: it falls back
    // to reading the pattern where it runs. Only the second half can be
    // asserted from a translation unit that has to compile.
    static_assert((txt::format_pattern<int>("{:70000}"), true));
    EXPECT_EQ(txt::format("{:70000}", 1).size(), 70000u);
}

// The standard as the oracle, over the values and the specifications the
// earlier ones left out. Every one of the seven faults of DESIGN note 169
// survived a suite of 719 cases and sixty-two tests, and none of them
// survived because the suite was small: it had no infinity in it, no NaN,
// no alternate form and no precision past what a buffer on the stack
// holds — that is, none of the things nobody had thought to ask about.
//
// The alternate general form is weighed against printf and not against
// std::format, because libc++ is wrong there: for a negative decimal
// exponent it uses a fixed precision of P - 1 where the C standard says
// P - 1 - X, so {:#g} of 0.5 comes out 0.50000 where printf writes
// 0.500000. That is checked in TheCornersTheReviewFound.
TEST(Format_Tests, TheStandardIsTheOracleOverTheCornersAsWell) {
    auto same = [](string ours, const std::string& theirs, const char* pat, double v) {
        EXPECT_EQ(ours.view(), theirs) << " pattern " << pat << " of " << v;
    };
    auto by_printf = [&](string ours, const char* form, const char* pat, double v) {
        char theirs[600];
        std::snprintf(theirs, sizeof theirs, form, v);
        EXPECT_EQ(ours.view(), std::string_view(theirs)) << " pattern " << pat << " of " << v;
    };
    auto check = [&](double v) {
#define SAME(pat) same(txt::format(pat, v), std::format(pat, v), pat, v)
        SAME("{}"); SAME("{:f}"); SAME("{:F}"); SAME("{:e}"); SAME("{:E}");
        SAME("{:g}"); SAME("{:G}"); SAME("{:a}"); SAME("{:A}");
        SAME("{:+}"); SAME("{: }"); SAME("{:-}");
        SAME("{:12}"); SAME("{:<12}"); SAME("{:>12}"); SAME("{:^12}"); SAME("{:012}");
        SAME("{:*^14f}"); SAME("{:+014.3f}");
        SAME("{:.0f}"); SAME("{:.1f}"); SAME("{:.3f}"); SAME("{:.30f}"); SAME("{:.100f}");
        SAME("{:.0e}"); SAME("{:.20e}"); SAME("{:.200e}");
        SAME("{:.1g}"); SAME("{:.6g}"); SAME("{:.17g}"); SAME("{:.120g}");
        SAME("{:#f}"); SAME("{:#e}"); SAME("{:#a}"); SAME("{:#.0f}"); SAME("{:#.0e}");
        SAME("{:#12}"); SAME("{:#012}"); SAME("{:#}");
#undef SAME
        // where libc++ errs, the C standard through printf
#define BYPRINTF(pat, form) by_printf(txt::format(pat, v), form, pat, v)
        BYPRINTF("{:#g}", "%#g");
        BYPRINTF("{:#G}", "%#G");
        BYPRINTF("{:#.1g}", "%#.1g");
        BYPRINTF("{:#.3g}", "%#.3g");
        BYPRINTF("{:#.6g}", "%#.6g");
        BYPRINTF("{:#.17g}", "%#.17g");
#undef BYPRINTF
    };
    double inf = std::numeric_limits<double>::infinity();
    double nan = std::numeric_limits<double>::quiet_NaN();
    for (double v : {0.0, 1.0, -1.0, 0.5, -0.5, 1.5, 100.0, 1e5, 123456.0, 991400.0,
                     0.1, 0.0001, 1e-5, 1e-7, 1e20, 1e-20, 3.14159265358979, 9.99999,
                     2.5, 1e300, 1e-300, 0.33333333333333331}) {
        check(v);
    }
    std::mt19937_64 rng(11);
    for (int i = 0; i < 400; ++i) {
        double v;
        if (i % 3 == 0) {
            do {
                v = std::bit_cast<double>(rng());
            } while (!std::isfinite(v));
        } else if (i % 3 == 1) {
            v = double(rng() % 1000000) / std::pow(10.0, double(rng() % 8));
        } else {
            v = double(int64_t(rng() % 2000000) - 1000000);
        }
        check(v);
    }
    // The two that have no digits, and which every earlier oracle left out
    for (double v : {inf, -inf, nan, -0.0}) {
        same(txt::format("{}", v), std::format("{}", v), "{}", v);
        same(txt::format("{:f}", v), std::format("{:f}", v), "{:f}", v);
        same(txt::format("{:F}", v), std::format("{:F}", v), "{:F}", v);
        same(txt::format("{:e}", v), std::format("{:e}", v), "{:e}", v);
        same(txt::format("{:E}", v), std::format("{:E}", v), "{:E}", v);
        same(txt::format("{:g}", v), std::format("{:g}", v), "{:g}", v);
        same(txt::format("{:G}", v), std::format("{:G}", v), "{:G}", v);
        same(txt::format("{:#}", v), std::format("{:#}", v), "{:#}", v);
        same(txt::format("{:#g}", v), std::format("{:#g}", v), "{:#g}", v);
        same(txt::format("{:>10}", v), std::format("{:>10}", v), "{:>10}", v);
        same(txt::format("{:+}", v), std::format("{:+}", v), "{:+}", v);
        same(txt::format("{:010}", v), std::format("{:010}", v), "{:010}", v);
    }
    // And the same for a float, which is its own type here
    for (float f : {1.1f, 0.1f, 3.14f, 1e20f, 1.0f, 100000.0f,
                    std::numeric_limits<float>::infinity()}) {
        EXPECT_EQ(txt::format("{}", f).view(), std::format("{}", f));
        EXPECT_EQ(txt::format("{:F}", f).view(), std::format("{:F}", f));
        EXPECT_EQ(txt::format("{:#}", f).view(), std::format("{:#}", f));
        EXPECT_EQ(txt::format("{:.60f}", f).view(), std::format("{:.60f}", f));
    }
}

// A pattern keeps four steps, which covers a message; one with more of
// them, or with a number too large for the narrow fields of a step, keeps
// none and is read again where it runs. Both roads must write the same.
TEST(Format_Tests, APatternTooLongForItsTableIsReadWhereItRuns) {
    EXPECT_EQ(txt::format("{} {} {} {} {}", 1, 2, 3, 4, 5), "1 2 3 4 5");
    EXPECT_EQ(txt::format("a{}b{}c{}d{}e{}f{}g", 1, 2, 3, 4, 5, 6), "a1b2c3d4e5f6g");
    EXPECT_EQ(txt::format("{0} {1} {0} {1} {0}", "x", "y"), "x y x y x");
    // A width past what a step can hold
    EXPECT_EQ(txt::format("{:70000}", 1).size(), 70000u);
    EXPECT_EQ(txt::format("{:>70000}", 1).view().substr(69999), "1");
    // And a doubled brace, whose two runs do not touch
    EXPECT_EQ(txt::format("{{{}}} {{{}}} {{{}}} {{{}}} {{{}}}", 1, 2, 3, 4, 5),
              "{1} {2} {3} {4} {5}");
}

// The text is long enough to pass the room the call keeps on the stack
TEST(Format_Tests, ATextLongerThanTheRoomOnTheStack) {
    string long_one(std::string(400, 'x'));
    auto made = txt::format("[{}]", long_one);
    EXPECT_EQ(made.size(), 402u);
    EXPECT_EQ(made[0], '[');
    EXPECT_EQ(made[401], ']');
    EXPECT_EQ(made.view().substr(1, 400), std::string(400, 'x'));
}
