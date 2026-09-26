//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the review of the module found (2026-09-25), one test each, so that
// none of it comes back: the first second of the range, a year_month_day
// that is no date, durations of no time of day, the rule of a TZif footer
// from the file's last transition, offsets past RFC 9636's range, names
// that are another name's, the keys of the registry, fields read twice.
#include "sgcl/time/time.h"
#include "tests/types.h"

#include <chrono>
#include <cmath>
#include <string>
#include <vector>

namespace {
    using namespace std::chrono_literals;
    using time::date;
    using time::datetime;
    using time::zone;
    namespace chr = std::chrono;

    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // A TZif file of version 2: transitions, types (offset, dst,
    // designation index), the designations, the footer
    std::string tzif(const std::vector<int64_t>& at, const std::vector<uint8_t>& type_of,
                     const std::vector<std::tuple<int32_t, uint8_t, uint8_t>>& types, const std::string& chars, const std::string& footer) {
        std::string b;
        auto u32 = [&](uint32_t v) {
            b += char(v >> 24);
            b += char(v >> 16);
            b += char(v >> 8);
            b += char(v);
        };
        auto header = [&] {
            b += "TZif2";
            b += std::string(15, '\0');
            u32(0);
            u32(0);
            u32(0);
            u32(uint32_t(at.size()));
            u32(uint32_t(types.size()));
            u32(uint32_t(chars.size()));
        };
        auto block = [&](int width) {
            for (int64_t t : at) {
                if (width == 8) {
                    u32(uint32_t(uint64_t(t) >> 32));
                }
                u32(uint32_t(t));
            }
            for (uint8_t i : type_of) {
                b += char(i);
            }
            for (auto& [o, d, i] : types) {
                u32(uint32_t(o));
                b += char(d);
                b += char(i);
            }
            b += chars;
        };
        header();
        block(4);
        header();
        block(8);
        b += '\n' + footer + '\n';
        return b;
    }

    slice<const byte> bytes_of(const std::string& s) {
        return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
    }
}

TEST(Review_Tests, TheFirstSecondOfTheRange) {
    auto lo = datetime::from_unix_nano(INT64_MIN, zone::utc());
    EXPECT_EQ(lo.nanosecond(), 145224192);
    EXPECT_EQ(text(lo.to_string()), "1677-09-21T00:12:43.145224192Z");
    EXPECT_EQ(text(lo.format("%T")), "00:12:43.145224192");
    EXPECT_EQ(lo.truncate(1h), lo);                                       // the step below is before the range
    EXPECT_EQ(lo.round(1h), lo);
    EXPECT_EQ(datetime::from_unix(INT64_MIN / 2, zone::utc()), lo);
    EXPECT_EQ(date(1600, 1, 1).at(0, 0, zone::utc()), lo);
    auto parsed = datetime::parse("1677-09-21 00:12:43.145224192 +0000", "%F %T %z");
    ASSERT_TRUE(parsed);
    EXPECT_EQ(*parsed, lo);
    EXPECT_EQ(text(parsed->format(time::rfc3339_nano)), "1677-09-21T00:12:43.145224192Z");
    EXPECT_EQ(lo.add_days(0), lo);
    // Exact where it fits, saturated only where it does not
    EXPECT_EQ(datetime::from_unix_milli(-9223372036854, zone::utc()).unix_nano(), -9223372036854000000);
    EXPECT_EQ(datetime::from_unix_milli(-9223372036855, zone::utc()), lo);
}

TEST(Review_Tests, AYearMonthDayThatIsNoDate) {
    chr::year_month_day thirteen{chr::year(2020), chr::month(13), chr::day(1)};
    EXPECT_EQ(text(txt::format("{}", thirteen)), "2020-13-01 is not a valid date");
    EXPECT_THROW((void)txt::format("{:%b}", thirteen), invalid_argument);
    EXPECT_THROW((void)txt::format("{:%a %j}", chr::year_month_day{chr::year(2020), chr::month(2), chr::day(30)}), invalid_argument);
    chr::year_month_day day200{chr::year(2020), chr::month(1), chr::day(200)};
    EXPECT_EQ(text(txt::format("{}", day200)), "2020-01-200 is not a valid date");
}

TEST(Review_Tests, DurationsWithNoTimeOfDay) {
    EXPECT_EQ(text(txt::format("{}", chr::duration<double>(INFINITY))), "infs");
    EXPECT_EQ(text(txt::format("{}", chr::duration<double>(-INFINITY))), "-infs");
    EXPECT_EQ(text(txt::format("{:%T}", chr::duration<double>(NAN))), "%T");
    EXPECT_EQ(text(txt::format("{:%T|%Q}", chr::duration<double>(1e300))), "%T|1e+300");
    EXPECT_EQ(text(txt::format("{}", chr::nanoseconds::min())), "-9223372036854775808ns");
    EXPECT_EQ(text(txt::format("{:%Q%q}", chr::nanoseconds::min())), "-9223372036854775808ns");
    EXPECT_EQ(text(txt::format("{}", chr::nanoseconds::max())), "9223372036854775807ns");
    EXPECT_EQ(text(txt::format("{}", chr::hh_mm_ss<chr::seconds>(100h))), "100:00:00");
    // A fraction rounded up to a whole second carries into it
    EXPECT_EQ(text(txt::format("{:%T}", chr::duration<double, std::milli>(999.9996))), "00:00:01.000");
}

TEST(Review_Tests, TheFootersRuleFromTheFilesLastTransition) {
    const std::string rule = "CET-1CEST,M3.5.0,M10.5.0/3";
    // A: one transition that changes nothing, at 1950: before it the file's
    // type, CET all summer
    auto a = zone::from_tzif(bytes_of(tzif({-631152000}, {0}, {{3600, 0, 0}, {7200, 1, 4}}, std::string("CET\0CEST\0", 9), rule)), "A");
    ASSERT_TRUE(a);
    EXPECT_EQ(text(date(1920, 7, 1).at(12, 0, *a).abbreviation()), "CET");
    EXPECT_EQ(text(date(1960, 7, 1).at(12, 0, *a).abbreviation()), "CEST");
    // B: CET from 1990, again CET at 2000: the rule from 2000, not from 1990
    auto b = zone::from_tzif(bytes_of(tzif({631152000, 946684800}, {0, 0}, {{3600, 0, 0}, {7200, 1, 4}}, std::string("CET\0CEST\0", 9), rule)), "B");
    ASSERT_TRUE(b);
    EXPECT_EQ(text(date(1995, 7, 1).at(12, 0, *b).abbreviation()), "CET");
    EXPECT_EQ(text(date(2005, 7, 1).at(12, 0, *b).abbreviation()), "CEST");
    // C: the file says CEST at its last transition in winter; the rule wins
    auto c = zone::from_tzif(bytes_of(tzif({946684800}, {1}, {{3600, 0, 0}, {7200, 1, 4}}, std::string("CET\0CEST\0", 9), rule)), "C");
    ASSERT_TRUE(c);
    EXPECT_EQ(text(date(2000, 1, 2).at(12, 0, *c).abbreviation()), "CET");
    EXPECT_EQ(text(date(1999, 12, 31).at(12, 0, *c).abbreviation()), "CET");
}

TEST(Review_Tests, OffsetsAndManyChanges) {
    // Past RFC 9636's range: refused
    auto far = zone::from_tzif(bytes_of(tzif({0}, {1}, {{0, 0, 0}, {200 * 3600, 0, 4}}, std::string("UTC\0XXX\0", 8), "")), "F");
    ASSERT_FALSE(far);
    // A change every minute for two days: a time of the clock is still read
    // near its instant, never as 1970
    std::vector<int64_t> at;
    std::vector<uint8_t> types;
    int64_t start = 1790000000;
    for (int k = 0; k < 2 * 24 * 60; ++k) {
        at.push_back(start + k * 60);
        types.push_back(uint8_t(k % 2));
    }
    auto busy = zone::from_tzif(bytes_of(tzif(at, types, {{0, 0, 0}, {1800, 1, 4}}, std::string("AAA\0BBB\0", 8), "")), "Busy");
    ASSERT_TRUE(busy);
    auto t = datetime::from_unix(start + 86400, *busy);
    for (auto r : {t.date().at(t.hour(), t.minute(), *busy), t.date().at(t.hour(), t.minute(), *busy, time::earlier), t.date().at(t.hour(), t.minute(), *busy, time::later)}) {
        EXPECT_LT(std::abs(r.unix() - t.unix()), 3600) << r;
    }
}

TEST(Review_Tests, OneNameForOneZone) {
    for (const char* name : {"Europe//Warsaw", "Europe/./Warsaw", "Europe/Warsaw/", "right/UTC", "Europe/.Warsaw"}) {
        EXPECT_FALSE(zone::load(name)) << name;
    }
    EXPECT_TRUE(zone::load("Etc/GMT+5"));
    EXPECT_TRUE(zone::load("America/Argentina/Buenos_Aires"));
    // The two kinds of made zone have keys of their own
    std::string file = tzif({}, {}, {{3600, 0, 0}}, std::string("CET\0", 4), "CET-1");
    ASSERT_TRUE(zone::from_tzif(bytes_of(file), "N"));
    std::string clash = "N";
    clash += '\0';
    clash += file;
    EXPECT_FALSE(zone::from_posix(sgcl::string(clash)));
}

TEST(Review_Tests, AFieldReadTwiceMustAgree) {
    EXPECT_FALSE(datetime::parse("2026-01-05 02", "%Y-%m-%d %m"));
    EXPECT_TRUE(datetime::parse("2026-01-05 01", "%Y-%m-%d %m"));
    EXPECT_FALSE(datetime::parse("2026 2025-01-05", "%Y %Y-%m-%d"));
    EXPECT_FALSE(datetime::parse("2026-01-05 25", "%Y-%m-%d %y"));
    EXPECT_TRUE(datetime::parse("2026-01-05 26", "%Y-%m-%d %y"));
    EXPECT_FALSE(datetime::parse("2026-01-05 19", "%Y-%m-%d %C"));
    EXPECT_FALSE(datetime::parse("2026-01-05 Feb", "%Y-%m-%d %b"));
    EXPECT_FALSE(datetime::parse("Mon 2 2026-01-06", "%a %u %F"));
    EXPECT_TRUE(datetime::parse("Tue 2 2026-01-06", "%a %u %F"));
    EXPECT_FALSE(datetime::parse("2026-01-05 13 AM", "%F %H %p"));
    EXPECT_TRUE(datetime::parse("2026-01-05 13 PM", "%F %H %p"));
    EXPECT_FALSE(datetime::parse("2026-01-05 +0100 +0200", "%F %z %z"));
}
