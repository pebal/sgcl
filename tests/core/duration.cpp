//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// duration: nanoseconds in 64 bits with Go's text, against Go's own
// answers (tools/duration_oracle.go), and its place among the types of
// std::chrono: the conversions both ways, the mixed operators, the
// saturation at the ends of the range.
#include "sgcl/core/duration.h"
#include "tests/core/duration_cases.h"
#include "tests/types.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <sstream>
#include <type_traits>

namespace {
    using namespace std::chrono_literals;
    using sgcl::duration;

    constexpr int64_t Max = std::numeric_limits<int64_t>::max();
    constexpr int64_t Min = std::numeric_limits<int64_t>::min();

    duration ns(int64_t n) {
        return std::chrono::nanoseconds(n);
    }
}

TEST(Duration_Tests, ParseAsGoDoes) {
    size_t accepted = 0;
    for (auto& c : oracle::GoParseDuration) {
        auto d = duration::parse(c.text);
        ASSERT_EQ(d.has_value(), c.ok) << '"' << c.text << '"';
        if (c.ok) {
            EXPECT_EQ(d->nanoseconds(), c.ns) << '"' << c.text << '"';
            ++accepted;
        } else {
            EXPECT_LE(d.error().offset(), std::char_traits<char>::length(c.text)) << '"' << c.text << '"';
            EXPECT_FALSE(d.error().message().empty());
        }
    }
    EXPECT_GT(accepted, 1000u);   // the random inputs are not all refusals
}

TEST(Duration_Tests, AFractionIsTakenExactly) {
    // Where Go's float64 lands off the exact value: math/big's answer
    for (auto& c : oracle::ExactParse) {
        auto d = duration::parse(c.text);
        ASSERT_TRUE(d.has_value()) << c.text;
        EXPECT_EQ(d->nanoseconds(), c.ns) << c.text;
    }
    EXPECT_EQ(duration::parse("0.3333333333333333333h")->nanoseconds(), 1199999999999);   // 3600e9 / 3 = 1.2e12 would need the digits to go on
    EXPECT_EQ(duration::parse("0.33333333333333333333333333333333333333333333333333333333333333333334h")->nanoseconds(), 1200000000000);
    EXPECT_EQ(duration::parse("1.9999999999999999999999999999999999999999ns")->nanoseconds(), 1);
}

TEST(Duration_Tests, TextAndUnitsAsGoHasThem) {
    for (auto& c : oracle::GoDurationValues) {
        duration d = ns(c.ns);
        EXPECT_EQ(d.to_string(), c.text) << c.ns;
        EXPECT_EQ(d.seconds(), c.seconds) << c.ns;
        EXPECT_EQ(d.minutes(), c.minutes) << c.ns;
        EXPECT_EQ(d.hours(), c.hours) << c.ns;
        EXPECT_EQ(d.microseconds(), c.microseconds) << c.ns;
        EXPECT_EQ(d.milliseconds(), c.milliseconds) << c.ns;
        EXPECT_EQ(d.abs().nanoseconds(), c.abs) << c.ns;
        EXPECT_EQ(d.nanoseconds(), c.ns);
        auto back = duration::parse(d.to_string());   // the text reads back as the value
        ASSERT_TRUE(back.has_value()) << c.text;
        EXPECT_EQ(*back, d) << c.text;
    }
}

TEST(Duration_Tests, TruncateAndRoundAsGoDoes) {
    for (auto& c : oracle::GoDurationSteps) {
        EXPECT_EQ(ns(c.ns).truncate(ns(c.step)).nanoseconds(), c.truncated) << c.ns << " " << c.step;
        EXPECT_EQ(ns(c.ns).round(ns(c.step)).nanoseconds(), c.rounded) << c.ns << " " << c.step;
    }
}

TEST(Duration_Tests, HalfASecondBelowZero) {
    duration d = -500ms;
    EXPECT_EQ(d.to_string(), "-500ms");
    EXPECT_EQ(d.seconds(), -0.5);
    EXPECT_EQ(d.milliseconds(), -500);
    EXPECT_EQ(d.truncate(1s), duration());           // toward zero
    EXPECT_EQ(d.round(1s), -1s);                     // a half away from zero
    EXPECT_EQ(duration(-1500ms).truncate(1s), -1s);
    EXPECT_EQ(duration(-1500ms).round(1s), -2s);
    EXPECT_EQ(duration(-1499ms).round(1s), -1s);
    EXPECT_EQ(duration(-1ns).microseconds(), 0);     // toward zero, not down
    EXPECT_EQ(duration(-1999us).milliseconds(), -1);
}

TEST(Duration_Tests, WhereARefusalPoints) {
    struct Case {
        const char* text;
        size_t offset;
    };
    Case cases[] = {
        {"", 0},                          // nothing
        {"-", 1},                         // a sign alone: the number expected after it
        {".s", 0},                        // a point with no digit on either side
        {"1h.m", 2},
        {"1d", 1},                        // the unit
        {"1h30", 4},                      // the unit missing at the end
        {"1s1", 3},
        {"1 s", 1},
        {"9223372036854775808ns", 0},     // the number too large
        {"1h9223372036854775807ns", 2},   // the sum too large, at the number that tipped it
        {"1.5.5s", 3},
        {"0.0", 3},
    };
    for (auto& c : cases) {
        auto d = duration::parse(c.text);
        ASSERT_FALSE(d.has_value()) << c.text;
        EXPECT_EQ(d.error().offset(), c.offset) << c.text;
    }
    EXPECT_EQ(duration::parse("1d").error().message(), "an unknown unit: ns, us, ms, s, m or h expected");
    EXPECT_EQ(duration::parse("1").error().message(), "a unit expected: ns, us, ms, s, m or h");
    EXPECT_EQ(duration::parse("").error().message(), "a number expected");
    EXPECT_EQ(duration::parse("3000000h").error().message(), "out of range: a duration is at most about 292 years");
}

TEST(Duration_Tests, BothMicroSigns) {
    EXPECT_EQ(duration::parse("1\xC2\xB5s")->nanoseconds(), 1000);   // µ, the micro sign
    EXPECT_EQ(duration::parse("1\xCE\xBCs")->nanoseconds(), 1000);   // μ, the Greek letter
    EXPECT_EQ(duration::parse("1us")->nanoseconds(), 1000);
    EXPECT_EQ(duration(1500ns).to_string(), "1.5\xC2\xB5s");          // written with the micro sign, as Go writes it
}

TEST(Duration_Tests, TheEndsOfTheRange) {
    EXPECT_EQ(duration::parse("-9223372036854775808ns")->nanoseconds(), Min);
    EXPECT_EQ(duration::parse("9223372036854775807ns")->nanoseconds(), Max);
    EXPECT_FALSE(duration::parse("9223372036854775808ns"));
    EXPECT_EQ(ns(Min).to_string(), "-2562047h47m16.854775808s");
    EXPECT_EQ(ns(Max).to_string(), "2562047h47m16.854775807s");
    EXPECT_EQ(ns(Min).abs().nanoseconds(), Max);   // no positive counterpart: the largest
}

TEST(Duration_Tests, FromAndToTheStandardDurations) {
    duration h = 1h;
    duration m = std::chrono::minutes(30);
    duration u = std::chrono::duration<unsigned, std::milli>(1500u);
    EXPECT_EQ(h.nanoseconds(), 3600000000000);
    EXPECT_EQ(m.minutes(), 30.0);
    EXPECT_EQ(u.to_string(), "1.5s");
    std::chrono::nanoseconds n = 90s + h;   // a duration in, a std one out
    EXPECT_EQ(n.count(), 3690000000000);
    EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::nanoseconds(h)).count(), 3600);

    // A floating one only explicitly, truncated toward zero; a NaN is zero
    static_assert(!std::is_convertible_v<std::chrono::duration<double>, duration>);
    static_assert(std::is_constructible_v<duration, std::chrono::duration<double>>);
    static_assert(!std::is_constructible_v<duration, std::chrono::duration<int64_t, std::pico>>);   // finer than a nanosecond: a duration_cast first
    static_assert(std::is_convertible_v<duration, std::chrono::nanoseconds>);
    static_assert(!std::is_convertible_v<int64_t, duration>);   // a bare number has no unit
    EXPECT_EQ(duration(std::chrono::duration<double>(1.5)), 1500ms);
    EXPECT_EQ(duration(std::chrono::duration<double>(-0.5)), -500ms);
    EXPECT_EQ(duration(std::chrono::duration<double, std::nano>(2.9)), 2ns);
    EXPECT_EQ(duration(std::chrono::duration<double, std::nano>(-2.9)), -2ns);
    EXPECT_EQ(duration(std::chrono::duration<double>(std::nan(""))), duration());
    EXPECT_EQ(duration(std::chrono::duration<double>(1e300)).nanoseconds(), Max);
    EXPECT_EQ(duration(std::chrono::duration<double>(-1e300)).nanoseconds(), Min);
    EXPECT_EQ(duration(std::chrono::duration<float>(0.25f)), 250ms);

    // A count too large for nanoseconds saturates, where chrono wraps
    EXPECT_EQ(duration(std::chrono::hours::max()).nanoseconds(), Max);
    EXPECT_EQ(duration(std::chrono::hours::min()).nanoseconds(), Min);
    EXPECT_EQ(duration(std::chrono::duration<uint64_t, std::nano>(UINT64_MAX)).nanoseconds(), Max);
    EXPECT_EQ(duration(std::chrono::duration<int64_t, std::ratio<31556952>>(-1000)).nanoseconds(), Min);   // a thousand years back
    EXPECT_EQ(duration(std::chrono::duration<int64_t, std::ratio<31556952>>(292)).nanoseconds(), int64_t(292) * 31556952 * 1000000000);
}

TEST(Duration_Tests, MixedWithTheStandardTypes) {
    duration d = 2s;
    EXPECT_EQ(d + 500ms, 2500ms);
    EXPECT_EQ(500ms + d, 2500ms);
    EXPECT_EQ(d - 3s, -1s);
    EXPECT_EQ(3s - d, 1s);
    EXPECT_TRUE(d < 3s);
    EXPECT_TRUE(3s > d);
    EXPECT_TRUE(d == 2000ms);
    EXPECT_TRUE(2000ms == d);
    EXPECT_TRUE(d != 1s);
    static_assert(std::is_same_v<decltype(d + 1s), duration>);
    static_assert(std::is_same_v<decltype(1s - d), duration>);

    auto t = std::chrono::steady_clock::now();
    EXPECT_EQ((t + d) - t, 2s);
    EXPECT_EQ((d + t) - t, 2s);
    EXPECT_EQ(t - (t - d), 2s);
    auto u = t;
    u += d;             // time_point's own +=, through the conversion
    u -= d;
    EXPECT_EQ(u, t);
    static_assert(std::is_same_v<decltype(t + d), std::chrono::steady_clock::time_point>);
    auto s = std::chrono::sys_seconds(std::chrono::seconds(10));   // a point of seconds moved by nanoseconds: nanoseconds, as chrono does
    static_assert(std::is_same_v<decltype(s + d), std::chrono::sys_time<std::chrono::nanoseconds>>);
    EXPECT_EQ((s + duration(1500ms)).time_since_epoch().count(), 11500000000);

    std::chrono::nanoseconds acc{};
    acc += d;           // the std duration's own +=
    EXPECT_EQ(acc.count(), 2000000000);
}

TEST(Duration_Tests, ArithmeticSaturates) {
    duration max = ns(Max);
    duration min = ns(Min);
    EXPECT_EQ(max + 1ns, max);
    EXPECT_EQ(min - 1ns, min);
    EXPECT_EQ(min + (-1ns), min);
    EXPECT_EQ(max - (-1ns), max);
    EXPECT_EQ(max + min, -1ns);
    EXPECT_EQ(-min, max);
    EXPECT_EQ(-max, ns(Min + 1));
    EXPECT_EQ(max * 2, max);
    duration one_s = 1s;
    duration one_h = 1h;
    EXPECT_EQ(max * -2, min);
    EXPECT_EQ(min * -1, max);
    EXPECT_EQ(min * 1, min);
    EXPECT_EQ(2 * one_h, 7200s);
    EXPECT_EQ(one_s * uint64_t(UINT64_MAX), max);
    EXPECT_EQ(-one_s * uint64_t(UINT64_MAX), min);
    EXPECT_EQ(one_h * 2562047, ns(int64_t(2562047) * 3600 * 1000000000));
    EXPECT_EQ(one_h * 2562048, max);
    EXPECT_EQ(min / -1, max);
    EXPECT_EQ(min / 1, min);
    EXPECT_EQ(min / uint64_t(UINT64_MAX), duration());
    EXPECT_EQ(7 * one_s / 2, 3500ms);
    EXPECT_EQ(ns(-7) / 2, -3ns);   // toward zero
    EXPECT_EQ(ns(7) / -2, -3ns);
    EXPECT_EQ(min / -1ns, Max);    // how many times: saturated too
    EXPECT_EQ(7 * one_s / 2s, 3);
    EXPECT_EQ(-7 * one_s / 2s, -3);
    EXPECT_EQ(7 * one_s % 2s, 1s);
    EXPECT_EQ(-7 * one_s % 2s, -1s);   // the sign of the dividend
    EXPECT_EQ(min % -1ns, duration());
    static_assert(std::is_same_v<decltype(one_s * 2), duration> && std::is_same_v<decltype(one_s / 2), duration> && std::is_same_v<decltype(one_s / 2s), int64_t>);
    duration d = 1s;
    d += 500ms;
    d -= 250ms;
    d *= 4;
    d /= 5;
    d %= 600ms;
    EXPECT_EQ(d, 400ms);
}

TEST(Duration_Tests, ConstantsAndConstantExpressions) {
    using sgcl::hour;
    using sgcl::microsecond;
    using sgcl::millisecond;
    using sgcl::minute;
    using sgcl::nanosecond;
    using sgcl::second;
    static_assert(nanosecond.nanoseconds() == 1);
    static_assert(microsecond == 1000 * nanosecond);
    static_assert(millisecond == 1000 * microsecond);
    static_assert(second == 1000 * millisecond);
    static_assert(minute == 60 * second);
    static_assert(hour == 60 * minute);
    static_assert(90 * second == 1min + 30s);
    static_assert((hour + 30 * minute).minutes() == 90.0);
    static_assert(duration() == duration::zero());
    static_assert((-90 * second).abs() == 90s);
    static_assert((1500 * millisecond).round(second) == 2 * second);
    static_assert(sizeof(duration) == 8);
    static_assert(std::is_trivially_copyable_v<duration>);
    static_assert(std::is_nothrow_default_constructible_v<duration>);
}

TEST(Duration_Tests, MaxAndMin) {
    static_assert(duration::max().nanoseconds() == Max && duration::min().nanoseconds() == Min);
    static_assert(duration::max() + 1 * sgcl::nanosecond == duration::max());
    EXPECT_EQ(sgcl::hour * 3000000, duration::max());   // 342 years: saturated
    EXPECT_EQ(duration::min().abs(), duration::max());
}

TEST(Duration_Tests, APointMovedSaturates) {
    // Where chrono's own + would overflow the count: the point stops at
    // its type's max() or min()
    using Steady = std::chrono::steady_clock;
    auto now = Steady::now();
    EXPECT_EQ(now + duration::max(), Steady::time_point::max());
    EXPECT_EQ(duration::max() + now, Steady::time_point::max());
    EXPECT_EQ(now - duration::min(), Steady::time_point::max());
    EXPECT_EQ(Steady::time_point::max() + 1 * sgcl::nanosecond, Steady::time_point::max());
    EXPECT_EQ(Steady::time_point::min() - 1 * sgcl::nanosecond, Steady::time_point::min());
    EXPECT_EQ(Steady::time_point::min() + duration::min(), Steady::time_point::min());
    EXPECT_EQ(now + duration(1s) - duration(1s), now);
    EXPECT_EQ((Steady::time_point() - duration::min()).time_since_epoch().count(), Max);   // 2^63 saturated, not 2^63 - 1 by way of -min
    EXPECT_EQ((Steady::time_point(std::chrono::nanoseconds(-5)) - duration::min()).time_since_epoch().count(), Max - 4);
    // A coarser point: its count scaled to nanoseconds saturates too
    auto far = std::chrono::sys_seconds(std::chrono::seconds(Max / 2));
    EXPECT_EQ(far + duration(1ns), std::chrono::sys_time<std::chrono::nanoseconds>::max());
    EXPECT_EQ(std::chrono::sys_seconds(std::chrono::seconds(Min / 2)) - duration(1ns), std::chrono::sys_time<std::chrono::nanoseconds>::min());
    // A finer one: the duration scaled to it
    using Pico = std::chrono::time_point<Steady, std::chrono::duration<int64_t, std::pico>>;
    EXPECT_EQ(Pico() + duration(1ns), Pico(std::chrono::duration<int64_t, std::pico>(1000)));
    EXPECT_EQ(Pico() + duration(10000h), Pico::max());
    // A floating one: chrono's arithmetic
    using Real = std::chrono::time_point<Steady, std::chrono::duration<double>>;
    EXPECT_EQ((Real() + duration(1500ms)).time_since_epoch().count(), 1.5e9);   // in the finer unit, nanoseconds of a double, as chrono makes it
    static_assert(Steady::time_point::max() + duration::max() == Steady::time_point::max());
}

TEST(Duration_Tests, WrittenToAStream) {
    std::ostringstream os;
    os << 90 * sgcl::minute << " " << duration() << " " << duration(-1500us);
    EXPECT_EQ(os.str(), "1h30m0s 0s -1.5ms");
}

TEST(Duration_Tests, RandomTextIsReadOrRefusedWithAnOffset) {
    // Texts drawn from the characters of Go's grammar and a few others,
    // the bytes of both micro signs among them: each is a duration whose
    // own text reads back as it, or a refusal with a sentence and an
    // offset inside the text
    std::mt19937 rng(20260924);
    const char* pieces[] = {"0", "1", "2", "5", "9", "0", "7", "3", ".", "h", "m", "s", "ms", "us", "ns", "\xC2\xB5s", "\xCE\xBCs", "\xC2", "-", "+", "x", " "};
    size_t accepted = 0;
    for (int k = 0; k < 200000; ++k) {
        std::string text;
        size_t length = rng() % 12;
        for (size_t i = 0; i < length; ++i) {
            text += pieces[rng() % std::size(pieces)];
        }
        auto d = duration::parse(sgcl::string(text));
        if (d) {
            ++accepted;
            ASSERT_EQ(duration::parse(d->to_string()), *d) << text;
        } else {
            ASSERT_LE(d.error().offset(), text.size()) << text;
            ASSERT_FALSE(d.error().message().empty()) << text;
        }
    }
    EXPECT_GT(accepted, 1000u);
}
