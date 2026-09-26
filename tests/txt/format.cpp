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
#include <cstddef>
#include <cstdio>
#include <format>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <map>
#include <random>
#include <set>
#include <thread>
#include <vector>
#include <array>
#include <utility>
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

    // The three shapes an enumeration comes in, and a signed one, since
    // the underlying type is what decides whether there is a sign
    enum class Colour { red = 0, green = 7, blue = 255 };
    enum Weekday { monday, friday = 4, sunday = 6 };
    enum class Flags : unsigned char { none = 0, all = 0xFF };
    enum class Offset : int { back = -42, forward = 42 };

    // And one whose own namespace says better, which must win over the
    // formatter that writes the number
    enum class Suit { hearts, spades };

    void format_value(txt::format_sink& out, Suit s, const txt::format_spec& spec) {
        txt::detail::write_text(out, s == Suit::hearts ? "hearts" : "spades", spec);
    }

    // A value longer than a field's first room, which counts how many
    // times it has been asked to write itself
    struct Tally {};

    long tallied = 0;

    const std::string TallyText = [] {
        std::string s;
        while (s.size() < 300) {
            s += "zażółć gęślą jaźń ";
        }
        return s;
    }();

    void format_value(txt::format_sink& out, const Tally&, const txt::format_spec& spec) {
        ++tallied;
        txt::detail::write_text(out, TallyText, spec);
    }

    // Whether a pattern can be built at all, asked without it being an
    // error that it cannot. A format_pattern that does not fit its values
    // throws inside a consteval constructor, so it yields no constant; a
    // requires-expression asks for one and answers false rather than
    // stopping the build, which is what lets a translation unit that has
    // to compile assert that a pattern does not.
    template<class Fn>
    concept builds_as_a_constant = requires { typename std::bool_constant<(Fn{}(), true)>::type; };

#define BUILDS(...) builds_as_a_constant<decltype([] { return __VA_ARGS__; })>
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
    EXPECT_EQ(txt::format("{} [{:>8}]", sgcl::duration(std::chrono::minutes(90)), sgcl::duration(std::chrono::milliseconds(1500))), "1h30m0s [    1.5s]");
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
// not fit its values throws and so stops the build.
//
// The other half — that a pattern which does not fit really does stop it
// — used to say here that it was checked by tests/txt/format_rejects.cpp.
// That file was never written, and could not have been: the CMake that
// builds this module globs tests/txt/*.cpp, so a file there that must not
// compile would stop the module from building at all. The rejections were
// checked by hand instead, which is to say they were not checked by
// anything that runs.
//
// They are checked here now, from inside a translation unit that has to
// compile, and the trick is BUILDS above: a consteval constructor that
// throws yields no constant, a requires-expression asks whether there is
// one without making it an error that there is not, and the answer comes
// back as a bool a static_assert can weigh. The pattern is named where it
// is asked about, so nothing has to be kept in step with anything.
TEST(Format_Tests, APatternIsReadWhereTheProgramIsBuilt) {
    static_assert((txt::format_pattern<int>("{}"), true));
    static_assert((txt::format_pattern<const char*>("{:s}"), true));
    static_assert((txt::format_pattern<double>("{:.3}"), true));
    static_assert((txt::format_pattern<int, string>("{:#x} {:>4}"), true));
    static_assert((txt::format_pattern<>("{{}}"), true));

    // The same patterns, asked rather than asserted, so that the asking
    // itself is known to answer yes when it should
    static_assert(BUILDS(txt::format_pattern<int>("{}")));
    static_assert(BUILDS(txt::format_pattern<double>("{:.3}")));
    static_assert(BUILDS(txt::format_pattern<int, string>("{:#x} {:>4}")));
    // More steps than a pattern keeps: it keeps none and is read where
    // it runs, which is correct and not wrong
    static_assert(BUILDS(txt::format_pattern<int>("{0} {0} {0} {0} {0}")));
    static_assert(BUILDS(txt::format_pattern<int>("{::>4}")));     // a colon to pad a number with
    static_assert(BUILDS(txt::format_pattern<std::vector<int>>("{::>4}")));
    static_assert(BUILDS(txt::format_pattern<std::vector<int>>("{:n:#x}")));
    static_assert(BUILDS(txt::format_pattern<std::vector<std::vector<int>>>("{:::>4}")));
    static_assert(BUILDS(txt::format_pattern<string>("{:?}")));
    static_assert(BUILDS(txt::format_pattern<Colour>("{:#x}")));
    static_assert(BUILDS(txt::format_pattern<pair<int, int>>("{:m}")));

    // And what must not build. Every one of these is a message from the
    // compiler in a real program.
    static_assert(!BUILDS(txt::format_pattern<int>("{")));           // a brace left open
    static_assert(!BUILDS(txt::format_pattern<int>("}")));           // a closing one on its own
    static_assert(!BUILDS(txt::format_pattern<int>("{:")));          // a specification that ends
    static_assert(!BUILDS(txt::format_pattern<int>("{:.}")));        // a point with no number
    static_assert(!BUILDS(txt::format_pattern<int>("{} {}")));       // no value of that number
    static_assert(!BUILDS(txt::format_pattern<int, int>("{2}")));
    static_assert(!BUILDS(txt::format_pattern<int>("{:s}")));        // not written that way
    static_assert(!BUILDS(txt::format_pattern<int>("{:f}")));
    static_assert(!BUILDS(txt::format_pattern<int>("{:.3}")));       // no precision on a number
    static_assert(!BUILDS(txt::format_pattern<string>("{:d}")));
    static_assert(!BUILDS(txt::format_pattern<int>("{:4294967297}")));  // a width that would wrap
    static_assert(!BUILDS(txt::format_pattern<double>("{:.40000}")));   // a precision past 0x7FFF
    // A closing brace is no longer a character to pad with, the field now
    // ending at the first one — the one thing ranges took away
    static_assert(!BUILDS(txt::format_pattern<int>("{:}<6}")));
    // Enumerations take no 'c' and no 's', the second being left free for
    // whoever writes the names
    static_assert(!BUILDS(txt::format_pattern<Colour>("{:s}")));
    static_assert(!BUILDS(txt::format_pattern<Colour>("{:c}")));
    // A range takes no 's' and no precision, and what it holds must take
    // what it is handed
    static_assert(!BUILDS(txt::format_pattern<std::vector<int>>("{:s}")));
    static_assert(!BUILDS(txt::format_pattern<std::vector<int>>("{:.2}")));
    static_assert(!BUILDS(txt::format_pattern<std::vector<int>>("{::s}")));
    static_assert(!BUILDS(txt::format_pattern<std::vector<int>>("{::f}")));
    static_assert(!BUILDS(txt::format_pattern<std::vector<int>>("{:m}")));   // 'm' is a pair's
    static_assert(!BUILDS(txt::format_pattern<tuple<int, int, int>>("{:m}")));  // and of two
    // A number holds nothing, so nothing can be handed to what it holds
    static_assert(!BUILDS(txt::format_pattern<int>("{::>4:>4}")));
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

    // A width is bounded by the sixteen bits a step of a compiled
    // pattern keeps it in, and not by the unsigned it is accumulated in.
    // The bound used to be the accumulator, which refused
    // {:4294967297} — it wrapped to one — and accepted {:4294967295},
    // and then wrote it: four thousand million columns of padding out of
    // sixteen characters of a pattern. The last width there is, and the
    // first there is not:
    static_assert((txt::format_pattern<int>("{:65535}"), true));
    EXPECT_EQ(txt::format("{:65535}", 1).size(), 65535u);
    static_assert(!BUILDS(txt::format_pattern<int>("{:65536}")));
    static_assert(!BUILDS(txt::format_pattern<int>("{:4294967295}")));
    auto at_run = [](const char* text) { return txt::runtime(string(text)); };
    EXPECT_EQ(txt::format(at_run("{:65535}"), 1).value().size(), 65535u);
    EXPECT_FALSE(txt::format(at_run("{:65536}"), 1));
    EXPECT_FALSE(txt::format(at_run("{:4294967295}"), 1));
    EXPECT_FALSE(txt::fits<int>(at_run("{:4294967295}")));
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
    // The widest field a step can hold, which is also the widest there
    // is: past this the pattern is refused rather than read again
    EXPECT_EQ(txt::format("{:65535}", 1).size(), 65535u);
    EXPECT_EQ(txt::format("{:>65535}", 1).view().substr(65534), "1");
    // And a doubled brace, whose two runs do not touch
    EXPECT_EQ(txt::format("{{{}}} {{{}}} {{{}}} {{{}}} {{{}}}", 1, 2, 3, 4, 5),
              "{1} {2} {3} {4} {5}");
}

// Values made of other values: a list in brackets, a table in braces, a
// pair in parentheses, a value that may not be there. The shapes are the
// ones C++23 settled on. What is asked here is that every kind of range
// this library has answers the same — a vector of ours, a slice, an
// array, an immutable one, a container of the standard's — that text
// still goes down the road text goes down although it is a range of
// characters, and that what follows a second colon reaches the elements
// however deep they are.
TEST(Format_Tests, ARangeIsWrittenInBrackets) {
    vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    EXPECT_EQ(txt::format("{}", v), "[1, 2, 3]");
    EXPECT_EQ(txt::format("{}", vector<int>()), "[]");
    EXPECT_EQ(txt::format("{:n}", v), "1, 2, 3");

    // Every other shape of range this library has
    int raw[3] = {1, 2, 3};
    EXPECT_EQ(txt::format("{}", slice<const int>(raw, raw + 3)), "[1, 2, 3]");
    EXPECT_EQ(txt::format("{}", array<int, 3>{1, 2, 3}), "[1, 2, 3]");
    EXPECT_EQ(txt::format("{}", std::array<int, 3>{1, 2, 3}), "[1, 2, 3]");
    EXPECT_EQ(txt::format("{}", std::vector<int>{1, 2, 3}), "[1, 2, 3]");
    EXPECT_EQ(txt::format("{}", immutable::vector<int>({1, 2, 3})), "[1, 2, 3]");
    EXPECT_EQ(txt::format("{}", std::initializer_list<int>{1, 2}), "[1, 2]");

    // A table is written in braces and by key; a set of keys in braces
    // without one. The sorted ones, because a hash decides the order of
    // the others and a test cannot be written against that.
    sorted_map<int, string> m;
    m.insert({1, string("one")});
    m.insert({2, string("two")});
    EXPECT_EQ(txt::format("{}", m), "{1: \"one\", 2: \"two\"}");
    EXPECT_EQ(txt::format("{:n}", m), "1: \"one\", 2: \"two\"");
    sorted_set<int> s;
    s.insert(3);
    s.insert(1);
    EXPECT_EQ(txt::format("{}", s), "{1, 3}");
    EXPECT_EQ(txt::format("{}", std::map<int, int>{{1, 10}, {2, 20}}), "{1: 10, 2: 20}");
    EXPECT_EQ(txt::format("{}", std::set<int>{3, 1}), "{1, 3}");

    // A pair and a tuple in parentheses, and 'm' writes the two of a pair
    // as one element of a table — which is how a map writes its own
    EXPECT_EQ(txt::format("{}", pair<int, int>(1, 2)), "(1, 2)");
    EXPECT_EQ(txt::format("{:m}", pair<int, int>(1, 2)), "1: 2");
    EXPECT_EQ(txt::format("{:n}", pair<int, int>(1, 2)), "1, 2");
    EXPECT_EQ(txt::format("{}", tuple<int, char, double>(1, 'x', 2.5)), "(1, 'x', 2.5)");
    EXPECT_EQ(txt::format("{}", tuple<int>(7)), "(7)");
    // A range of pairs is a list of pairs and not a table
    std::vector<std::pair<int, int>> ps{{1, 2}, {3, 4}};
    EXPECT_EQ(txt::format("{}", ps), "[(1, 2), (3, 4)]");

    // A value that may not be there
    EXPECT_EQ(txt::format("{}", optional<int>(42)), "42");
    EXPECT_EQ(txt::format("{}", optional<int>()), "nullopt");
    EXPECT_EQ(txt::format("{:#x}", optional<int>(255)), "0xff");
    EXPECT_EQ(txt::format("{:#x}", optional<int>()), "nullopt");
    EXPECT_EQ(txt::format("[{:>8}]", optional<int>(42)), "[      42]");
    EXPECT_EQ(txt::format("[{:>8}]", optional<int>()), "[ nullopt]");
    EXPECT_EQ(txt::format("{}", optional<string>(string("here"))), "\"here\"");

    // Text is a range of characters and must not be taken for one
    EXPECT_EQ(txt::format("{}", string("nasz")), "nasz");
    EXPECT_EQ(txt::format("{}", std::string("std")), "std");
    EXPECT_EQ(txt::format("{}", string("abc").as_slice(1, 2)), "bc");
    // A vector of ours and not one of the standard's: a string holds a
    // tracked_ptr, which may not live in unmanaged memory
    vector<string> words;
    words.push_back(string("żółć"));
    words.push_back(string("a"));
    EXPECT_EQ(txt::format("{}", words), "[\"żółć\", \"a\"]");
}

// What follows a second colon belongs to the elements, which is the shape
// C++23 uses and the one people already know. The standard is the oracle
// over the elements themselves: it cannot be asked about a range in C++20,
// but it answers about every element, and the brackets and the commas are
// the whole of what is left.
TEST(Format_Tests, TheSpecificationAfterTheSecondColonReachesTheElements) {
    std::vector<int> v{1, 42, 255};
    EXPECT_EQ(txt::format("{::>5}", v), "[    1,    42,   255]");
    EXPECT_EQ(txt::format("{::#x}", v), "[0x1, 0x2a, 0xff]");
    EXPECT_EQ(txt::format("{::04}", v), "[0001, 0042, 0255]");
    EXPECT_EQ(txt::format("{:n:#06x}", v), "0x0001, 0x002a, 0x00ff");
    EXPECT_EQ(txt::format("{::}", v), "[1, 42, 255]");   // there, and empty

    // The standard as the oracle over the elements
    auto joined = [](const std::vector<int>& xs, const char* one, const char* open,
                     const char* close) {
        std::string out = open;
        for (size_t i = 0; i < xs.size(); ++i) {
            if (i) {
                out += ", ";
            }
            out += std::vformat(one, std::make_format_args(xs[i]));
        }
        return out + close;
    };
    for (const char* elem : {"", "5", "<5", ">5", "^5", "05", "+", "#x",
                             "#b", "#o", "X", "*^7", "#010x"}) {
        std::string ours = std::string("{::") + elem + "}";
        std::string theirs = std::string("{:") + elem + "}";
        auto made = txt::format(txt::runtime(string(ours.c_str())), v);
        ASSERT_TRUE(made.has_value()) << " over " << ours;
        EXPECT_EQ(made.value().view(), joined(v, theirs.c_str(), "[", "]")) << " over " << ours;
    }

    // The field of a range is the whole of it, measured in columns like
    // any other
    EXPECT_EQ(txt::format("[{:>12}]", std::vector<int>{1, 2}), "[      [1, 2]]");
    EXPECT_EQ(txt::format("[{:*^12}]", std::vector<int>{1, 2}), "[***[1, 2]***]");
    EXPECT_EQ(txt::format("[{:<12}]", std::vector<int>{1, 2}), "[[1, 2]      ]");
    vector<string> polish;
    polish.push_back(string("żółć"));
    EXPECT_EQ(txt::format("[{:>10}]", polish), "[  [\"żółć\"]]");   // eight columns, not twelve bytes

    // A body bigger than the room this call keeps on the stack, which is
    // where it asks for more and writes itself over: every length either
    // side of it, and one far past every doubling, against the same body
    // with no field around it
    for (size_t n : {30u, 31u, 32u, 33u, 34u, 63u, 64u, 65u, 2000u}) {
        std::vector<int> many(n, 7);
        auto bare = txt::format("{}", many);
        auto in_field = txt::format("{:>1}", many);     // a width too small to pad by
        EXPECT_EQ(in_field, bare) << " over " << n << " elements";
        auto no_width = txt::format("{:*>}", many);      // a fill, and nothing to fill
        EXPECT_EQ(no_width, bare) << " over " << n << " elements";
    }
    {
        // and the padding itself is right on either side of the room
        std::vector<int> few(64, 7);                    // 64 * 3 - 2 = 190 characters
        auto bare = txt::format("{}", few);
        EXPECT_EQ(bare.size(), 192u);
        EXPECT_EQ(txt::format("[{:>200}]", few).size(), 202u);
        EXPECT_EQ(txt::format("[{:>200}]", few).view().substr(1, 8), "        ");
        std::vector<int> more(100, 7);                  // past the 256 the call keeps
        auto wide = txt::format("[{:*>400}]", more);
        EXPECT_EQ(wide.size(), 402u);
        EXPECT_EQ(wide.view().substr(1, 3), "***");
        EXPECT_EQ(wide.view().substr(401), "]");
        EXPECT_NE(wide.view().find("[7, 7"), std::string_view::npos);
    }

    // As deep as the type goes, one colon read at every level
    std::vector<std::vector<int>> deep{{1, 2}, {3}};
    EXPECT_EQ(txt::format("{}", deep), "[[1, 2], [3]]");
    EXPECT_EQ(txt::format("{::n}", deep), "[1, 2, 3]");
    EXPECT_EQ(txt::format("{:::>4}", deep), "[[   1,    2], [   3]]");
    std::vector<std::pair<int, int>> ps{{1, 2}};
    EXPECT_EQ(txt::format("{::m}", ps), "[1: 2]");
    EXPECT_EQ(txt::format("{:::>4}", ps), "[(   1,    2)]");
    std::map<int, std::vector<int>> tree{{1, {2, 3}}};
    EXPECT_EQ(txt::format("{}", tree), "{1: [2, 3]}");

    // And a colon is still a character to pad with, for every value that
    // does not hold other values — which is what it always was
    EXPECT_EQ(txt::format("{::>6}", 42), "::::42");
    EXPECT_EQ(txt::format("{::^7}", "ab"), "::ab:::");
    EXPECT_EQ(txt::format("{::>6}", optional<int>(42)), "::::42");

    // A range down the road that reads its pattern where it runs
    auto at_run = [](const char* text) { return txt::runtime(string(text)); };
    EXPECT_EQ(txt::format(at_run("{::>5}"), v).value(), "[    1,    42,   255]");
    EXPECT_EQ(txt::format(at_run("{:n}"), v).value(), "1, 42, 255");
    EXPECT_FALSE(txt::format(at_run("{:s}"), v));          // a range takes no 's'
    EXPECT_FALSE(txt::format(at_run("{:.2}"), v));         // and no precision
    EXPECT_FALSE(txt::format(at_run("{::s}"), v));         // nor does a number, inside one
    // and a third colon over a list of numbers is a colon to pad each
    // of them with, a number holding nothing of its own
    EXPECT_EQ(txt::format(at_run("{:::>4}"), v).value(), "[:::1, ::42, :255]");
    EXPECT_TRUE(txt::fits<std::vector<int>>(at_run("{::#x}")));
    EXPECT_FALSE(txt::fits<std::vector<int>>(at_run("{::f}")));

    // What fits in a buffer of the caller's, and what the whole takes
    char room[64];
    auto whole = slice<char>(room, room + sizeof room);
    EXPECT_EQ(txt::format_to(whole, "{}", v), 12u);
    EXPECT_EQ(std::string(room, 12), "[1, 42, 255]");
}

// {:?} writes text the way a program would rather than the way a reader
// would: in quotes, with what a terminal cannot show as an escape. It is
// what tells a list of two words from one word with a comma in it, and it
// is why the elements of a range are written this way by default.
TEST(Format_Tests, TheDebugFormWritesTextAsAProgramWould) {
    EXPECT_EQ(txt::format("{:?}", "abc"), "\"abc\"");
    EXPECT_EQ(txt::format("{:?}", string("abc")), "\"abc\"");
    EXPECT_EQ(txt::format("{:?}", std::string_view("abc")), "\"abc\"");
    EXPECT_EQ(txt::format("{:?}", string("abc").as_slice(1, 2)), "\"bc\"");
    EXPECT_EQ(txt::format("{:?}", ""), "\"\"");

    // The five escapes with a letter of their own, and the quote that
    // has to be escaped inside each kind of quoting
    EXPECT_EQ(txt::format("{:?}", "a\tb"), "\"a\\tb\"");
    EXPECT_EQ(txt::format("{:?}", "a\nb"), "\"a\\nb\"");
    EXPECT_EQ(txt::format("{:?}", "a\rb"), "\"a\\rb\"");
    EXPECT_EQ(txt::format("{:?}", "a\\b"), "\"a\\\\b\"");
    EXPECT_EQ(txt::format("{:?}", "a\"b"), "\"a\\\"b\"");
    EXPECT_EQ(txt::format("{:?}", "a'b"), "\"a'b\"");    // the other quote stands
    EXPECT_EQ(txt::format("{:?}", '\''), "'\\''");
    EXPECT_EQ(txt::format("{:?}", '"'), "'\"'");
    EXPECT_EQ(txt::format("{:?}", 'x'), "'x'");
    EXPECT_EQ(txt::format("{:?}", '\n'), "'\\n'");
    EXPECT_EQ(txt::format("{:?}", U'ż'), "'ż'");

    // A control with no letter of its own, and the categories the
    // standard names: Cc, Cf, Cs, Co, Cn, Zl, Zp, and every Zs but the
    // space itself
    EXPECT_EQ(txt::format("{:?}", "\x01"), "\"\\u{1}\"");
    EXPECT_EQ(txt::format("{:?}", "\x7F"), "\"\\u{7f}\"");
    EXPECT_EQ(txt::format("{:?}", " "), "\" \"");                 // the space stays
    EXPECT_EQ(txt::format("{:?}", "\u00A0"), "\"\\u{a0}\"");      // Zs: no-break space
    EXPECT_EQ(txt::format("{:?}", "\u2028"), "\"\\u{2028}\"");    // Zl
    EXPECT_EQ(txt::format("{:?}", "\u2029"), "\"\\u{2029}\"");    // Zp
    EXPECT_EQ(txt::format("{:?}", "\u200B"), "\"\\u{200b}\"");    // Cf: zero width space
    EXPECT_EQ(txt::format("{:?}", "\uE000"), "\"\\u{e000}\"");    // Co: private use
    EXPECT_EQ(txt::format("{:?}", "\u0378"), "\"\\u{378}\"");     // Cn: unassigned
    // And what is a letter, a mark or a symbol stands as it is
    EXPECT_EQ(txt::format("{:?}", "żółć"), "\"żółć\"");
    EXPECT_EQ(txt::format("{:?}", "日本"), "\"日本\"");
    EXPECT_EQ(txt::format("{:?}", "á"), "\"á\"");                 // a combining mark

    // A byte that begins no sequence and ends none is not a character,
    // so it goes out as the byte it is rather than as the replacement it
    // would decode to — which would lose which byte it had been
    EXPECT_EQ(txt::format("{:?}", "a\xFF" "b"), "\"a\\x{ff}b\"");
    EXPECT_EQ(txt::format("{:?}", "\xC5"), "\"\\x{c5}\"");        // a lead byte with nothing after
    EXPECT_EQ(txt::format("{:?}", "\uFFFD"), "\"\uFFFD\"");       // and a real one stands

    // A field and a precision are over what comes out, escapes and
    // quotes included, and the field is still measured in columns
    EXPECT_EQ(txt::format("[{:>8?}]", "ab"), "[    \"ab\"]");
    EXPECT_EQ(txt::format("[{:*^10?}]", "ab"), "[***\"ab\"***]");
    EXPECT_EQ(txt::format("[{:>8?}]", "żółć"), "[  \"żółć\"]");   // six columns, ten bytes
    EXPECT_EQ(txt::format("{:.3?}", "abcdef"), "\"ab");           // the quote is cut with it
    EXPECT_EQ(txt::format("{:.4?}", "a\tb"), "\"a\\t");
    // Longer than the room the call keeps for the escaped text
    string long_one(std::string(400, '\t'));
    EXPECT_EQ(txt::format("[{:>810?}]", long_one).size(), 812u);

    // The elements of a range and of a pair take this form by default,
    // which is the change that makes a list of words readable. Naming any
    // type at all takes it back, and a number never had one.
    vector<string> words;
    words.push_back(string("a, b"));
    words.push_back(string("c"));
    EXPECT_EQ(txt::format("{}", words), "[\"a, b\", \"c\"]");
    EXPECT_EQ(txt::format("{::s}", words), "[a, b, c]");          // the ambiguity it fixes
    EXPECT_EQ(txt::format("{::>8}", words), "[  \"a, b\",      \"c\"]");
    EXPECT_EQ(txt::format("{}", std::vector<char>{'a', '\n'}), "['a', '\\n']");
    EXPECT_EQ(txt::format("{}", std::vector<int>{1, 2}), "[1, 2]");
    EXPECT_EQ(txt::format("{}", pair<string, int>(string("k"), 1)), "(\"k\", 1)");
    // And an optional, so that one holding the word and one holding
    // nothing are not the same seven letters
    EXPECT_EQ(txt::format("{}", optional<string>(string("nullopt"))), "\"nullopt\"");
    EXPECT_EQ(txt::format("{}", optional<string>()), "nullopt");
    EXPECT_EQ(txt::format("{:s}", optional<string>(string("nullopt"))), "nullopt");

    // The standard cannot be the oracle here: its own {:?} is C++23 and
    // this is built as C++20, so std::format refuses the pattern where
    // the program is built — the same wall the ranges met. What can be
    // asked instead is the property the form exists for. Two different
    // texts must never come out as the same characters, which is exactly
    // what fails without it; and what comes out must be text — valid
    // UTF-8, with nothing left in it that a terminal would act on.
    const char* every[] = {"", "abc", "a b", "a\tb", "a\nb", "a\rb", "a\\b", "a\"b", "a'b",
                           "\x01", "\x02", "\x7f", " ", "\u017c\u00f3\u0142\u0107",
                           "\u65e5\u672c", "a\u0301", "\u00a0", "\u2028", "\u2029",
                           "\u200b", "\ue000", "\u0378", "\ufffd", "a, b", "a\", \"b",
                           "nullopt", "\\t", "\xFF", "\xC5", "a\xFF" "b"};
    std::vector<std::string> seen;
    for (const char* t : every) {
        auto made = txt::format("{:?}", t);
        std::string out(made.view());
        for (const std::string& before : seen) {
            EXPECT_NE(out, before) << " two texts came out the same: " << out;
        }
        seen.push_back(out);
        EXPECT_TRUE(utf8::valid(made.view())) << " over " << out;
        ASSERT_GE(out.size(), 2u);
        EXPECT_EQ(out.front(), '"');
        EXPECT_EQ(out.back(), '"');
        for (size_t i = 1; i + 1 < out.size(); ++i) {
            EXPECT_FALSE((unsigned char)out[i] < 0x20 || out[i] == 0x7F)
                << " a control survived the escaping of " << out;
        }
        // And the library's own text goes down the road the standard's
        // text goes down
        EXPECT_EQ(txt::format("{:?}", string(t)).view(), out);
    }
}

// An enumeration is written as the number it is, with the whole numeric
// specification over it. Neither an `enum class` nor a plain `enum` is an
// integral type, so before this neither compiled at all. The standard is
// the oracle over the underlying value, which is the same question.
TEST(Format_Tests, AnEnumerationIsWrittenAsTheNumberItIs) {
    EXPECT_EQ(txt::format("{}", Colour::green), "7");
    EXPECT_EQ(txt::format("{}", Colour::blue), "255");
    EXPECT_EQ(txt::format("{:d}", Colour::blue), "255");
    EXPECT_EQ(txt::format("{:x} {:X}", Colour::blue, Colour::blue), "ff FF");
    EXPECT_EQ(txt::format("{:#x}", Colour::blue), "0xff");
    EXPECT_EQ(txt::format("{:#06x}", Colour::blue), "0x00ff");
    EXPECT_EQ(txt::format("{:b} {:#b}", Colour::green, Colour::green), "111 0b111");
    EXPECT_EQ(txt::format("{:o} {:#o}", Colour::green, Colour::green), "7 07");
    EXPECT_EQ(txt::format("[{:>6}]", Colour::blue), "[   255]");
    EXPECT_EQ(txt::format("[{:*^7}]", Colour::blue), "[**255**]");
    EXPECT_EQ(txt::format("{:05}", Colour::blue), "00255");

    // A plain enum, which is no more integral than the other kind
    EXPECT_EQ(txt::format("{} {} {}", monday, friday, sunday), "0 4 6");

    // The underlying type is what says whether there is a sign
    EXPECT_EQ(txt::format("{}", Flags::all), "255");
    EXPECT_EQ(txt::format("{}", Offset::back), "-42");
    EXPECT_EQ(txt::format("{:+} {:+}", Offset::back, Offset::forward), "-42 +42");
    EXPECT_EQ(txt::format("{:05}", Offset::back), "-0042");
    // and what the hexadecimal form is of: -42 as a signed int is the
    // magnitude with a sign in front of it, not a two's complement
    EXPECT_EQ(txt::format("{:x}", Offset::back), "-2a");

    // byte is an enumeration and comes along for free, which is
    // what one wants of it
    EXPECT_EQ(txt::format("{:02x}", byte{0x0A}), "0a");
    EXPECT_EQ(txt::format("{}", byte{255}), "255");

    // The names are the caller's business: a format_value beside the
    // enumeration wins over the formatter that writes the number
    EXPECT_EQ(txt::format("{}", Suit::hearts), "hearts");
    EXPECT_EQ(txt::format("[{:>8}]", Suit::spades), "[  spades]");

    // The standard as the oracle over the underlying value, which is the
    // same question asked of a type it does know
    auto same = [](string ours, const std::string& theirs, const char* pat, long long v) {
        EXPECT_EQ(ours.view(), theirs) << " pattern " << pat << " of " << v;
    };
#define SAME(pat, e) same(txt::format(pat, e), \
                          std::format(pat, static_cast<std::underlying_type_t<decltype(e)>>(e)), \
                          pat, (long long)static_cast<std::underlying_type_t<decltype(e)>>(e))
    for (auto e : {Colour::red, Colour::green, Colour::blue}) {
        SAME("{}", e); SAME("{:d}", e); SAME("{:b}", e); SAME("{:B}", e); SAME("{:o}", e);
        SAME("{:x}", e); SAME("{:X}", e);
        SAME("{:#x}", e); SAME("{:#b}", e); SAME("{:#o}", e);
        SAME("{:8}", e); SAME("{:<8}", e); SAME("{:>8}", e); SAME("{:^8}", e); SAME("{:08}", e);
        SAME("{:+}", e); SAME("{: }", e); SAME("{:*^10}", e); SAME("{:#010x}", e);
    }
    for (auto e : {Offset::back, Offset::forward}) {
        SAME("{}", e); SAME("{:d}", e); SAME("{:x}", e); SAME("{:#x}", e);
        SAME("{:+}", e); SAME("{:08}", e); SAME("{:>10}", e);
    }
    for (auto e : {monday, friday, sunday}) {
        SAME("{}", e); SAME("{:#x}", e); SAME("{:>6}", e);
    }
#undef SAME
}

// A pattern the compiler never saw: the one road where a pattern that
// does not fit its values is not a build error but an answer of nullopt.
// What is checked is that it writes exactly what the compiled road writes
// for the same pattern, that every way of not fitting comes back empty,
// and that nothing is handed out half written.
TEST(Format_Tests, APatternReadWhereTheProgramRuns) {
    auto at_run = [](const char* text) { return txt::runtime(string(text)); };

    EXPECT_EQ(txt::format(at_run("{} left"), 42).value(), "42 left");
    EXPECT_EQ(txt::format(at_run("{:>8.3f} {:#x}"), 1.5, 255).value(), "   1.500 0xff");
    EXPECT_EQ(txt::format(at_run("{{literal}} {}"), true).value(), "{literal} true");
    EXPECT_EQ(txt::format(at_run("nothing at all")).value(), "nothing at all");
    // The order of the values is what a translation changes, so the
    // numbered fields matter more here than anywhere
    EXPECT_EQ(txt::format(at_run("{1} {0}"), "a", "b").value(), "b a");
    EXPECT_EQ(txt::format(at_run("{0} {0} {0}"), 7).value(), "7 7 7");
    // And the field of a text is still measured in columns
    EXPECT_EQ(txt::format(at_run("[{:>10}]"), "żółć").value(), "[      żółć]");

    // The same pattern down both roads writes the same text
    for (int v : {0, 7, -1, 1000000}) {
        EXPECT_EQ(txt::format(at_run("{:>12}"), v).value(), txt::format("{:>12}", v));
        EXPECT_EQ(txt::format(at_run("{:#x}"), v).value(), txt::format("{:#x}", v));
        EXPECT_EQ(txt::format(at_run("a{}b{}c{}d{}e{}f"), v, v, v, v, v).value(),
                  txt::format("a{}b{}c{}d{}e{}f", v, v, v, v, v));
    }

    // Every way of not fitting, which on the other road is the compiler's
    // error and here is an empty answer
    EXPECT_FALSE(txt::format(at_run("{} {}"), 1));            // no value of that number
    EXPECT_FALSE(txt::format(at_run("{2}"), 1, 2));
    EXPECT_FALSE(txt::format(at_run("{"), 1));                // a brace left open
    EXPECT_FALSE(txt::format(at_run("}"), 1));                // one on its own
    EXPECT_FALSE(txt::format(at_run("{:"), 1));               // a specification that does not end
    EXPECT_FALSE(txt::format(at_run("{:.}"), 1));             // a point with no number
    EXPECT_FALSE(txt::format(at_run("{:d}"), "a name"));      // that value is not written so
    EXPECT_FALSE(txt::format(at_run("{:.3}"), 1));            // a whole number has no precision
    EXPECT_FALSE(txt::format(at_run("{:s}"), 1));
    EXPECT_FALSE(txt::format(at_run("{:4294967297}"), 1));    // a width that would wrap
    EXPECT_FALSE(txt::format(at_run("{:.40000}"), 1.0));      // a precision past what a step holds

    // Into a buffer of the caller's, with the same answer about the size
    char room[64];
    auto whole = slice<char>(room, room + sizeof room);
    EXPECT_EQ(txt::format_to(whole, at_run("{} left"), 42).value(), 7u);
    EXPECT_EQ(std::string(room, 7), "42 left");
    EXPECT_FALSE(txt::format_to(whole, at_run("{} {}"), 1));

    // Longer than the room the call keeps on the stack, where the pattern
    // is walked a second time
    string long_one(std::string(400, 'x'));
    EXPECT_EQ(txt::format(at_run("[{}]"), long_one).value().size(), 402u);
    EXPECT_EQ(txt::format(at_run("[{}]"), long_one).value().view().substr(0, 2), "[x");

    // The question asked of a catalogue as it is loaded, with no values
    // in hand
    EXPECT_TRUE(txt::fits<int>(at_run("{} left")));
    EXPECT_TRUE((txt::fits<int, string>(at_run("{1}: {0:#x}"))));
    EXPECT_FALSE(txt::fits<int>(at_run("{} {}")));
    EXPECT_FALSE(txt::fits<int>(at_run("{:s}")));
    EXPECT_FALSE((txt::fits<int, string>(at_run("{1:d}"))));

    // What a caller actually writes: the translation when it fits, the
    // pattern the program was built with when it does not
    auto entry = at_run("pozostało: {}");
    auto broken = at_run("{} {} {}");
    EXPECT_EQ(txt::format(entry, 3).value_or(txt::format("{} left", 3)), "pozostało: 3");
    EXPECT_EQ(txt::format(broken, 3).value_or(txt::format("{} left", 3)), "3 left");
}

TEST(Format_Tests, ASinkWhoseRoomWasLentHandsBackOnlyWhatIsThere) {
    // A growing_sink counts the whole text whether or not it fitted —
    // that is the contract render_to and format_to keep — and it says
    // "this room cannot grow" with a capacity of size_t(-1), so that a
    // walk has one shape of sink and not two. Those two together made
    // text() and view() read size() characters out of a room of n: for
    // a lent sink, whatever stood after the caller's buffer. Nobody
    // called them that way; this is here so that nobody can.
    char room[16];
    std::memset(room, '#', sizeof room);
    {
        txt::growing_sink lent(txt::growing_sink::lent, room, sizeof room);
        lent.out().put("0123456789abcdefghijklmnop", 26);
        EXPECT_EQ(lent.size(), 26u);                    // the whole of it
        EXPECT_EQ(lent.view().size(), sizeof room);     // and what is there
        EXPECT_EQ(lent.view(), "0123456789abcdef");
        EXPECT_EQ(lent.text().size(), sizeof room);
        // the room cannot be added to, and says so
        EXPECT_EQ(lent.capacity(), size_t(-1));
        EXPECT_EQ(lent.take_room(26, 0), size_t(-1));
        EXPECT_EQ(lent.view().size(), sizeof room);
    }
    // A sink that can grow has the two numbers equal at every moment,
    // which is why nothing ever noticed
    {
        char first[8];
        txt::growing_sink grows(first, sizeof first);
        grows.out().put("abcdefghij", 10);
        EXPECT_EQ(grows.size(), 10u);
        EXPECT_GT(grows.size(), grows.capacity());      // it ran off the end
        grows.take_room(grows.size(), 0);
        grows.out().put("abcdefghij", 10);              // written over, into room for it
        EXPECT_EQ(grows.size(), 10u);
        EXPECT_EQ(grows.view(), "abcdefghij");
        EXPECT_EQ(grows.text(), "abcdefghij");
    }
}

TEST(Format_Tests, AnArgumentNumberBiggerThanAnyCountIsRefusedAndNotWrapped) {
    auto at_run = [](const char* text) { return txt::runtime(string(text)); };

    // The number is read into a size_t, and a number that has gone round
    // it comes out small: 2^64 is nought and 2^64 + 1 is one, so a
    // pattern nobody could mean answered as a pattern that means
    // something else — "{18446744073709551617}" over two values wrote the
    // second of them. It is stopped before the multiplication now.
    EXPECT_FALSE(txt::format(at_run("{18446744073709551616}"), 42));
    EXPECT_FALSE(txt::format(at_run("{18446744073709551617}"), 10, 20));
    EXPECT_FALSE(txt::format(at_run("{99999999999999999999999}"), 42));
    // one digit either side of where a size_t stops
    EXPECT_FALSE(txt::format(at_run("{18446744073709551615}"), 42));
    EXPECT_FALSE(txt::format(at_run("{1844674407370955161}"), 42));
    // and a number that fits and is simply too big for the call
    EXPECT_FALSE(txt::format(at_run("{4294967296}"), 42));

    // The question asked of a catalogue as it is loaded says the same
    EXPECT_FALSE(txt::fits<int>(at_run("{18446744073709551616}")));
    EXPECT_FALSE((txt::fits<int, int>(at_run("{18446744073709551617}"))));

    // Whatever else is around it, a field is still counted from where it
    // stands, so the numbers that do fit are untouched
    EXPECT_EQ(txt::format(at_run("{1}{0}"), "a", "b").value(), "ba");
    EXPECT_EQ(txt::format(at_run("{00}"), "a").value(), "a");
    EXPECT_EQ(txt::format(at_run("{000000000000000000000000}"), "a").value(), "a");

    // The same walk stands under the pattern the compiler reads, where
    // the answer is an error of the build rather than an empty one
    static_assert(!BUILDS(txt::format_pattern<int>("{18446744073709551616}")));
    static_assert(!BUILDS(txt::format_pattern<int, int>("{18446744073709551617}")));
    static_assert(BUILDS(txt::format_pattern<int>("{000}")));
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

// A thing made of other things in a field is written before it is padded,
// the width of it not being known until then, and it used to be written
// into room of the field's own: 256 bytes on the stack, and a body that
// did not fit was written again into room sized for it. Which is right
// for one level and wrong for a nest whose every level is bigger than
// that — all of them are, once the innermost is — because each level
// that is written again writes every level under it again as well, and
// the innermost is written two to the power of the depth times. Eight
// levels round three hundred bytes asked for it 256 times; sixteen levels
// took 443 ms through a template.
//
// The levels share one run of memory now, and a level that has to grow
// it grows it for all of them, so the nest is walked once: twice at most
// the first time a thread asks for so much, and once afterwards.
TEST(Format_Tests, ANestWhoseEveryLevelIsLargeIsWrittenOnce) {
    using V = std::vector<std::vector<std::vector<std::vector<
              std::vector<std::vector<std::vector<std::vector<Tally>>>>>>>>;
    V nest(1);
    nest[0].resize(1);
    nest[0][0].resize(1);
    nest[0][0][0].resize(1);
    nest[0][0][0][0].resize(1);
    nest[0][0][0][0][0].resize(1);
    nest[0][0][0][0][0][0].resize(1);
    nest[0][0][0][0][0][0][0].resize(1);
    std::string want = std::string(8, '[') + TallyText + std::string(8, ']');

    // format_to and not format, whose pattern is written twice whenever
    // what it makes passes its own room, which is a different matter and
    // measured on its own
    std::string room(4096, '#');
    auto into = slice<char>(room.data(), room.size());
    tallied = 0;
    size_t n = txt::format_to(into, "{:>2:>2:>2:>2:>2:>2:>2:>2:>2}", nest);
    EXPECT_LE(tallied, 2);
    ASSERT_EQ(n, want.size());
    EXPECT_EQ(room.substr(0, n), want);

    tallied = 0;
    n = txt::format_to(into, "{:>2:>2:>2:>2:>2:>2:>2:>2:>2}", nest);
    EXPECT_EQ(tallied, 1);
    EXPECT_EQ(room.substr(0, n), want);

    // and a field wider than the whole pads the whole, in columns: the
    // text is Polish, so its bytes are more than its columns
    size_t cols = 16 + txt::detail::columns_of_text(TallyText);
    tallied = 0;
    n = txt::format_to(into, "{:*^2000:>2:>2:>2:>2:>2:>2:>2:>2}", nest);
    EXPECT_EQ(tallied, 1);
    ASSERT_EQ(n, want.size() + 2000 - cols);
    EXPECT_EQ(room.substr(0, (2000 - cols) / 2), std::string((2000 - cols) / 2, '*'));
    EXPECT_EQ(room.substr((2000 - cols) / 2, want.size()), want);
}

// A level that ran off the end of the shared room and was then moved,
// because a field opened inside it needed the room to grow. Its count
// was right and its bytes were short — the ", " after an element that
// filled the room exactly had gone nowhere — and the room being larger
// afterwards, nothing asked again: the list came out with the separator
// missing and two bytes of whatever stood at the end. It has to be a
// thread of its own, which starts with no room at all, so that the
// sizes below are the sizes the room really has: 256 for the list, 512
// once the first element opens its field, and that element padded to
// fill it to the last byte.
TEST(Format_Tests, AFieldThatRanOffBeforeTheRoomGrewIsWrittenAgain) {
    auto list_of = [](const std::vector<int>& v) {
        std::string s = "[";
        for (size_t i = 0; i < v.size(); ++i) {
            s += i ? ", " : "";
            s += std::to_string(v[i]);
        }
        return s + "]";
    };
    auto right = [](const std::string& s, size_t w) {
        return s.size() < w ? std::string(w - s.size(), ' ') + s : s;
    };
    auto want = [&](const std::vector<std::vector<int>>& g, size_t w) {
        std::string s = "[";
        for (size_t i = 0; i < g.size(); ++i) {
            s += i ? ", " : "";
            s += right(list_of(g[i]), w);
        }
        return s + "]";
    };

    std::string first, second;
    size_t mismatched = 0;
    std::thread([&] {
        std::vector<std::vector<int>> g{{1}, std::vector<int>(150, 1234)};
        std::string room(8192, '#');
        size_t n = txt::format_to(slice<char>(room.data(), room.size()), "{:>1:>511}", g);
        first = room.substr(0, n);
        second = want(g, 511);
        // and every width round it, over a room that has grown by now
        for (size_t w = 0; w <= 1100; ++w) {
            std::vector<std::vector<int>> h{{1}, {2, 3}, std::vector<int>(w % 170, 1234), {5}};
            std::string spec = "{:>1:>" + std::to_string(w) + "}";
            auto got = txt::format_to(slice<char>(room.data(), room.size()),
                                      txt::runtime(string(spec.c_str())), h);
            if (!got || room.substr(0, *got) != want(h, w)) {
                ++mismatched;
            }
        }
    }).join();
    EXPECT_EQ(first, second);
    EXPECT_EQ(mismatched, 0u);
}
