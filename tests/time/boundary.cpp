//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of the module's types (DESIGN 408): the default values,
// the ends of the ranges and the arithmetic that crosses them, the leap
// second and the leap day, the edges of a zone's gap and overlap, the
// fields of the texts at their limits, the TZif and TZ strings at theirs.
#include "sgcl/time/time.h"
#include "tests/types.h"

#include <chrono>
#include <climits>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sgcl::async;

namespace {
    using namespace std::chrono_literals;
    using time::date;
    using time::datetime;
    using time::zone;
    namespace chr = std::chrono;

    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    zone load(const char* name) {
        auto z = zone::load(name);
        EXPECT_TRUE(z) << name;
        return z ? *z : zone::utc();
    }

    const date First(-32767, 1, 1);
    const date Last(32767, 12, 31);

    datetime lo() {
        return datetime::from_unix_nano(INT64_MIN, zone::utc());
    }

    datetime hi() {
        return datetime::from_unix_nano(INT64_MAX, zone::utc());
    }

    // A TZif file of version 2 (the times of 64 bits), as review.cpp's
    std::string tzif(const std::vector<int64_t>& at, const std::vector<uint8_t>& type_of,
                     const std::vector<std::tuple<int32_t, uint8_t, uint8_t>>& types, const std::string& chars, const std::string& footer) {
        std::string b;
        auto u32 = [&](uint32_t v) {
            b += char(v >> 24);
            b += char(v >> 16);
            b += char(v >> 8);
            b += char(v);
        };
        auto header = [&](size_t times) {
            b += "TZif2";
            b += std::string(15, '\0');
            u32(0);
            u32(0);
            u32(0);
            u32(uint32_t(times));
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
        header(at.size());
        block(4);
        header(at.size());
        block(8);
        b += '\n' + footer + '\n';
        return b;
    }

    slice<const byte> bytes_of(const std::string& s) {
        return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
    }

    // The exact truncation and rounding, in 128 bits: the remainder of the
    // instant counted from Go's zero time, saturated at the ends
    int64_t truncated(int64_t ns, int64_t step) {
        const __int128 zero_to_epoch = (__int128)62135596800 * 1000000000;
        __int128 from_zero = (__int128)ns + zero_to_epoch;
        __int128 r = from_zero % step;
        __int128 down = (__int128)ns - r;
        return down < INT64_MIN ? INT64_MIN : int64_t(down);
    }

    int64_t rounded(int64_t ns, int64_t step) {
        const __int128 zero_to_epoch = (__int128)62135596800 * 1000000000;
        __int128 from_zero = (__int128)ns + zero_to_epoch;
        __int128 r = from_zero % step;
        __int128 v = r + r < step ? (__int128)ns - r : (__int128)ns + (step - r);
        return v < INT64_MIN ? INT64_MIN : v > INT64_MAX ? INT64_MAX : int64_t(v);
    }
}

//----------------------------------------------------------------------------
// date
//----------------------------------------------------------------------------

TEST(DateBoundary_Tests, TheDefaultIsTheEpoch) {
    date d;
    EXPECT_EQ(d, date(1970, 1, 1));
    EXPECT_EQ(d.year(), 1970);
    EXPECT_EQ(d.month(), time::month::january);
    EXPECT_EQ(d.day(), 1);
    EXPECT_EQ(d.weekday(), time::weekday::thursday);
    EXPECT_EQ(d.year_day(), 1);
    EXPECT_EQ(d.iso_week(), (time::iso_week{1970, 1}));
    EXPECT_EQ(d.days_in_month(), 31);
    EXPECT_FALSE(d.is_leap_year());
    EXPECT_EQ(text(d.to_string()), "1970-01-01");
    EXPECT_EQ(chr::sys_days(d).time_since_epoch().count(), 0);
    time::iso_week w;
    EXPECT_EQ(w, (time::iso_week{0, 0}));
}

// d - n is add_days(-n): with n of INT_MIN the negation does not fit an
// int, and the date moves forward to the end of the calendar
TEST(DateBoundary_Tests, SubtractingTheSmallestInt) {
    date d(2026, 9, 24);
    EXPECT_EQ(d - INT_MIN, Last);
    EXPECT_EQ(First - INT_MIN, Last);
    EXPECT_EQ(Last - INT_MAX, First);
    EXPECT_EQ(d - INT_MAX, First);
    date e = d;
    e -= INT_MIN;
    EXPECT_EQ(e, Last);
    e = d;
    e -= INT_MAX;
    EXPECT_EQ(e, First);
    e = d;
    e += INT_MIN;
    EXPECT_EQ(e, First);
    EXPECT_EQ(INT_MAX + d, Last);
    EXPECT_EQ(Last - First, 23936165);
    EXPECT_EQ(First - Last, -23936165);
    EXPECT_EQ(d - d, 0);
}

TEST(DateBoundary_Tests, TheCalendarOfChronoAtItsEnds) {
    EXPECT_EQ(date(chr::sys_days(chr::days(INT_MAX))), Last);
    EXPECT_EQ(date(chr::sys_days(chr::days(INT_MIN))), First);
    EXPECT_EQ(date(chr::year(2026) / 2 / 30), date(2026, 3, 2));     // a day past the month carries
    EXPECT_EQ(date(chr::year(2026) / 13 / 1), date(2027, 1, 1));
    EXPECT_EQ(date(chr::year(2026) / 3 / 0), date(2026, 2, 28));
    EXPECT_EQ(date(chr::year::max() / 12 / 31), Last);
    EXPECT_EQ(date(chr::year::min() / 1 / 1), First);
    auto f = chr::year_month_day(First);
    EXPECT_TRUE(f.ok());
    EXPECT_EQ(f, chr::year::min() / 1 / 1);
    EXPECT_EQ(chr::year_month_day(Last), chr::year::max() / 12 / 31);
    EXPECT_EQ(chr::sys_days(Last), chr::sys_days(chr::year::max() / 12 / 31));
}

TEST(DateBoundary_Tests, IsValidAtTheLimits) {
    EXPECT_TRUE(date::is_valid(-32767, 1, 1));
    EXPECT_TRUE(date::is_valid(32767, 12, 31));
    EXPECT_FALSE(date::is_valid(-32768, 12, 31));
    EXPECT_FALSE(date::is_valid(32768, 1, 1));
    EXPECT_FALSE(date::is_valid(INT_MIN, 1, 1));
    EXPECT_FALSE(date::is_valid(INT_MAX, 1, 1));
    EXPECT_FALSE(date::is_valid(2026, INT_MIN, 1));
    EXPECT_FALSE(date::is_valid(2026, INT_MAX, 1));
    EXPECT_FALSE(date::is_valid(2026, 1, INT_MIN));
    EXPECT_FALSE(date::is_valid(2026, 1, INT_MAX));
    EXPECT_FALSE(date::is_valid(2026, 0, 1));
    EXPECT_FALSE(date::is_valid(2026, 13, 1));
    EXPECT_FALSE(date::is_valid(2026, 1, 0));
    EXPECT_FALSE(date::is_valid(2026, 1, 32));
    EXPECT_TRUE(date::is_valid(2024, 2, 29));
    EXPECT_TRUE(date::is_valid(2000, 2, 29));
    EXPECT_TRUE(date::is_valid(0, 2, 29));
    EXPECT_FALSE(date::is_valid(2023, 2, 29));
    EXPECT_FALSE(date::is_valid(1900, 2, 29));
    EXPECT_FALSE(date::is_valid(-32767, 2, 29));   // -32767 is not divisible by 4
    EXPECT_TRUE(date::is_valid(-32764, 2, 29));
    EXPECT_FALSE(date::is_valid(2026, time::month(0), 1));
    EXPECT_FALSE(date::is_valid(2026, time::month(13), 1));
}

TEST(DateBoundary_Tests, LeapDaysInTheArithmetic) {
    date leap(2024, 2, 29);
    EXPECT_EQ(leap.add_years(1), date(2025, 2, 28));
    EXPECT_EQ(leap.add_years(4), date(2028, 2, 29));
    EXPECT_EQ(leap.add_years(-100), date(1924, 2, 29));
    EXPECT_EQ(leap.add_years(-124), date(1900, 2, 28));   // 1900 has no 29th
    EXPECT_EQ(leap.add_months(12), date(2025, 2, 28));
    EXPECT_EQ(date(2024, 1, 31).add_months(1), leap);
    EXPECT_EQ(date(2024, 3, 31).add_months(-1), leap);
    EXPECT_EQ(leap.add_days(1), date(2024, 3, 1));
    EXPECT_EQ(date(2024, 3, 1).add_days(-1), leap);
    EXPECT_EQ(leap.year_day(), 60);
    EXPECT_EQ(date(2024, 12, 31).year_day(), 366);
    EXPECT_EQ(leap.days_in_month(), 29);
    // The last leap day of the calendar and the year after it
    EXPECT_EQ(date(32764, 2, 29).add_years(3), date(32767, 2, 28));
    EXPECT_EQ(date(32764, 2, 29).add_years(4), Last);
}

TEST(DateBoundary_Tests, ParseEveryFieldAtItsLimits) {
    auto ok = [](const char* s, date expected) {
        auto d = date::parse(s);
        ASSERT_TRUE(d) << s << ": " << (d ? "" : text(d.error().message()));
        EXPECT_EQ(*d, expected) << s;
    };
    auto bad = [](const char* s, size_t offset) {
        auto d = date::parse(s);
        ASSERT_FALSE(d) << s;
        EXPECT_EQ(d.error().offset(), offset) << s << ": " << text(d.error().message());
    };
    ok("-32767-01-01", First);
    ok("32767-12-31", Last);
    ok("+32767-12-31", Last);
    ok("0000-01-01", date(0, 1, 1));
    ok("+0000-01-01", date(0, 1, 1));
    ok("00000-01-01", date(0, 1, 1));
    ok("2024-02-29", date(2024, 2, 29));
    ok("2000-02-29", date(2000, 2, 29));
    ok("2026-01-01", date(2026, 1, 1));
    ok("2026-12-31", date(2026, 12, 31));
    ok("2024-366", date(2024, 12, 31));
    ok("2024366", date(2024, 12, 31));
    ok("2026-001", date(2026, 1, 1));
    ok("2026-W53-7", date(2027, 1, 3));    // 2026 starts on a Thursday: 53 weeks
    ok("2026-W01-1", date(2025, 12, 29));
    ok("2026W537", date(2027, 1, 3));
    bad("", 0);
    bad("+", 1);
    bad("-", 1);
    bad("-32768-12-31", 1);
    bad("+32768-01-01", 1);
    bad("99999-01-01", 0);
    bad("-0000-01-01", 0);
    bad("2026-00-01", 5);
    bad("2026-13-01", 5);
    bad("2026-01-00", 8);
    bad("2026-01-32", 8);
    bad("2023-02-29", 8);
    bad("1900-02-29", 8);
    bad("2026-000", 5);
    bad("2026-366", 5);
    bad("2024-367", 5);
    bad("2026-W00-1", 6);
    bad("2025-W53-1", 6);
    bad("2026-W54-1", 6);
    bad("2026-W01-0", 9);
    bad("2026-W01-8", 9);
    bad("2026-09-2", 8);
    bad("2026-09-240", 10);
    bad("2026-09", 7);
    bad("2026-0924", 7);
    bad("202609-24", 0);
    bad("2026-09-24 ", 10);
    bad(" 2026-09-24", 0);
    bad("2026-09-24T00:00", 10);
}

// The week date at the two ends of the calendar, read by ISO 8601 and by
// a pattern of std::format's specifiers: the same date or the same
// refusal, never a day the text does not name
TEST(DateBoundary_Tests, AWeekDateAtTheEndsOfTheCalendar) {
    for (int year : {-32767, -32766, 32766, 32767}) {
        for (int week = 1; week <= 53; ++week) {
            for (int day = 1; day <= 7; ++day) {
                std::string s = std::format("{}{:04}-W{:02}-{}", year < 0 ? "-" : "", year < 0 ? -year : year, week, day);
                auto iso = date::parse(sgcl::string(s));
                auto pattern = date::parse(sgcl::string(s), "%5G-W%V-%u");
                ASSERT_EQ(bool(iso), bool(pattern)) << s;
                if (iso) {
                    ASSERT_EQ(*iso, *pattern) << s;
                    ASSERT_EQ(text(iso->format("%G-W%V-%u")), s) << s;
                }
            }
        }
    }
    // The weeks counted from the first Sunday and the first Monday of the
    // year: a date found is the one the fields name
    for (int year : {-32767, 32767}) {
        for (const char* p : {"%5Y %U %u", "%5Y %W %u"}) {
            for (int week = 0; week <= 53; ++week) {
                for (int day = 1; day <= 7; ++day) {
                    std::string s = std::format("{} {:02} {}", year, week, day);
                    auto d = date::parse(sgcl::string(s), p);
                    if (d) {
                        ASSERT_EQ(text(d->format(p + 3)), s.substr(s.find(' '))) << s << " " << p;   // the week and the day
                        ASSERT_EQ(d->year(), year) << s << " " << p;
                    }
                }
            }
        }
    }
}

TEST(DateBoundary_Tests, APatternAtItsLimits) {
    auto date_parse = [](const char* s, const char* p) -> std::string {
        auto d = date::parse(s, p);
        return d ? text(d->to_string()) : "error at " + std::to_string(d.error().offset());
    };
    EXPECT_EQ(date_parse("-32767", "%5Y"), "error at 0");   // a year alone names no date
    EXPECT_EQ(date_parse("-32767", "%Y"), "error at 5");    // four digits by default
    EXPECT_EQ(date_parse("-32767-01-01", "%5Y-%m-%d"), "-32767-01-01");
    EXPECT_EQ(date_parse("32767-12-31", "%5Y-%m-%d"), "32767-12-31");
    EXPECT_EQ(date_parse("+32767-12-31", "%5Y-%m-%d"), "32767-12-31");
    EXPECT_EQ(date_parse("32767-12-31", "%Y-%m-%d").substr(0, 5), "error");   // four digits by default
    EXPECT_EQ(date_parse("32768-01-01", "%5Y-%m-%d").substr(0, 5), "error");
    EXPECT_EQ(date_parse("-32768-01-01", "%5Y-%m-%d").substr(0, 5), "error");
    EXPECT_EQ(date_parse("32767 365", "%5Y %j"), "32767-12-31");
    EXPECT_EQ(date_parse("32767 366", "%5Y %j").substr(0, 5), "error");
    EXPECT_EQ(date_parse("2024 366", "%Y %j"), "2024-12-31");
    EXPECT_EQ(date_parse("2024 367", "%Y %j").substr(0, 5), "error");
    EXPECT_EQ(date_parse("2024 000", "%Y %j").substr(0, 5), "error");
    EXPECT_EQ(date_parse("29.02.2024", "%d.%m.%Y"), "2024-02-29");
    EXPECT_EQ(date_parse("29.02.2023", "%d.%m.%Y").substr(0, 5), "error");
    // The two digits of a year: 69 to 99 in the 1900s, 00 to 68 in the 2000s
    EXPECT_EQ(date_parse("01.01.68", "%d.%m.%y"), "2068-01-01");
    EXPECT_EQ(date_parse("01.01.69", "%d.%m.%y"), "1969-01-01");
    EXPECT_EQ(date_parse("01.01.00", "%d.%m.%y"), "2000-01-01");
    EXPECT_EQ(date_parse("01.01.99", "%d.%m.%y"), "1999-01-01");
    EXPECT_EQ(date_parse("-99 01 01 01", "%C %y %m %d"), "-9899-01-01");
    EXPECT_EQ(date_parse("99 99 12 31", "%C %y %m %d"), "9999-12-31");
    EXPECT_EQ(date_parse("100 01 01 01", "%C %y %m %d").substr(0, 5), "error");
    // Empty text, an empty pattern, a pattern cut short
    EXPECT_EQ(date_parse("", "").substr(0, 5), "error");
    EXPECT_EQ(date_parse("", "%Y-%m-%d").substr(0, 5), "error");
    EXPECT_EQ(date_parse("2026", "%").substr(0, 5), "error");
    EXPECT_EQ(date_parse("2026", "%Y%E").substr(0, 5), "error");
    EXPECT_EQ(date_parse("2026-01-01", "%F%").substr(0, 5), "error");
    EXPECT_EQ(date_parse("2026-01-01", "%F %H").substr(0, 5), "error");   // a time of day in the pattern of a date
    EXPECT_EQ(date_parse("2026-01-01", "%F %z").substr(0, 5), "error");
    EXPECT_EQ(date_parse("2026-01-01", "%F %Q").substr(0, 5), "error");   // not a specifier
    EXPECT_EQ(date_parse("2026-01-01", "%1000F"), "2026-01-01");
}

TEST(DateBoundary_Tests, AtWithTheFieldsAtTheirLimits) {
    zone utc;
    EXPECT_EQ(date(2026, 1, 1).at(INT_MAX, INT_MAX, INT_MAX, utc), hi());
    EXPECT_EQ(date(2026, 1, 1).at(INT_MIN, INT_MIN, INT_MIN, utc), lo());
    EXPECT_EQ(Last.at(0, 0, utc), hi());
    EXPECT_EQ(First.at(23, 59, 59, utc), lo());
    EXPECT_EQ(Last.at(0, 0, utc, time::earlier), hi());
    EXPECT_EQ(First.at(0, 0, utc, time::later), lo());
    EXPECT_EQ(Last.start_of_day(load("Europe/Warsaw")), hi().in(load("Europe/Warsaw")));
    EXPECT_EQ(Last.try_at(0, 0, 0, utc), hi());
    EXPECT_EQ(First.try_at(0, 0, 0, utc), lo());
    EXPECT_EQ(date(2026, 9, 24).at(0, 0, -1, utc), date(2026, 9, 23).at(23, 59, 59, utc));
    EXPECT_EQ(date(2026, 9, 24).at(0, 0, 86400, utc), date(2026, 9, 25).at(0, 0, utc));
    EXPECT_EQ(date(2026, 9, 24).at(0, 1440, utc), date(2026, 9, 25).at(0, 0, utc));
}

// The edges of a gap and of an overlap, to the second: the last time
// before a change and the first after it are there once
TEST(DateBoundary_Tests, TheEdgesOfAGapAndAnOverlap) {
    zone w = load("Europe/Warsaw");
    date spring(2026, 3, 29);   // 02:00 CET becomes 03:00 CEST
    EXPECT_TRUE(spring.try_at(1, 59, 59, w));
    EXPECT_FALSE(spring.try_at(2, 0, 0, w));
    EXPECT_FALSE(spring.try_at(2, 59, 59, w));
    EXPECT_TRUE(spring.try_at(3, 0, 0, w));
    EXPECT_EQ(*spring.try_at(3, 0, 0, w) - *spring.try_at(1, 59, 59, w), 1s);
    EXPECT_EQ(spring.at(2, 0, 0, w, time::earlier), *spring.try_at(3, 0, 0, w));
    EXPECT_EQ(spring.at(2, 59, 59, w, time::earlier), *spring.try_at(3, 0, 0, w));
    EXPECT_EQ(text(spring.at(2, 59, 59, w).to_string()), "2026-03-29T03:59:59+02:00");
    date autumn(2026, 10, 25);  // 03:00 CEST becomes 02:00 CET
    EXPECT_TRUE(autumn.try_at(1, 59, 59, w));
    EXPECT_FALSE(autumn.try_at(2, 0, 0, w));
    EXPECT_FALSE(autumn.try_at(2, 59, 59, w));
    EXPECT_TRUE(autumn.try_at(3, 0, 0, w));
    EXPECT_EQ(autumn.at(2, 0, 0, w, time::later) - autumn.at(2, 0, 0, w, time::earlier), 1h);
    EXPECT_EQ(autumn.at(2, 59, 59, w, time::later) - autumn.at(2, 59, 59, w), 1h);
    EXPECT_EQ(*autumn.try_at(3, 0, 0, w) - autumn.at(2, 59, 59, w, time::later), 1s);
    EXPECT_EQ(autumn.at(2, 0, 0, w) - *autumn.try_at(1, 59, 59, w), 1s);
    // The transitions at the edges, strictly before and after
    datetime change = *w.next_transition(*autumn.try_at(1, 59, 59, w));
    EXPECT_EQ(change, autumn.at(2, 0, 0, w, time::later));
    EXPECT_EQ(w.previous_transition(change + 1ns), change);
    EXPECT_NE(w.previous_transition(change), change);
    EXPECT_EQ(w.next_transition(change - 1ns), change);
    EXPECT_NE(w.next_transition(change), change);
    EXPECT_EQ(change.offset(), 1h);
    EXPECT_EQ((change - 1ns).offset(), 2h);
    EXPECT_TRUE((change - 1ns).is_dst());
    EXPECT_FALSE(change.is_dst());
}

TEST(DateBoundary_Tests, FormatAtTheEndsAsStdFormat) {
    const char* pattern = "%Y|%C|%y|%g|%V|%j|%U|%W|%u|%w|%m|%d|%e|%F|%D|%a|%A|%b|%B";
    for (date d : {First, First.add_days(1), First.add_days(6), Last, Last.add_days(-1), Last.add_days(-6), date(0, 1, 1), date(-1, 12, 31)}) {
        auto ymd = chr::year_month_day(d);
        std::string expected = std::vformat(std::string("{:") + pattern + "}", std::make_format_args(ymd));
        EXPECT_EQ(text(d.format(pattern)), expected) << expected;
    }
    // %G as %Y, four digits after the sign ([time.format]: "left-padded with 0
    // to four digits"); libc++ writes "-001" for -1, counting the sign
    EXPECT_EQ(text(date(0, 1, 1).format("%G %Y")), "-0001 0000");
    EXPECT_EQ(text(date(-1, 12, 31).format("%G %Y")), "-0001 -0001");
    EXPECT_EQ(text(First.format("%G %Y")), "-32768 -32767");
    EXPECT_EQ(text(Last.format("%G %Y")), "32767 32767");
    EXPECT_EQ(text(date().format("")), "");
    EXPECT_EQ(text(date().format("%")), "%");
    EXPECT_EQ(text(date().format("%E")), "%E");
    EXPECT_EQ(text(date().format("%H %Z %Q")), "%H %Z %Q");
    EXPECT_EQ(text(date().format("%%")), "%");
    // A text longer than the first room on the stack
    std::string many;
    std::string expected;
    for (int i = 0; i < 300; ++i) {
        many += "%F";
        expected += "1970-01-01";
    }
    EXPECT_EQ(text(date().format(sgcl::string(many))), expected);
}

TEST(DateBoundary_Tests, NamesOutOfTheirTables) {
    EXPECT_EQ(text(time::to_string(time::month(0))), "%!Month(0)");
    EXPECT_EQ(text(time::to_string(time::month(13))), "%!Month(13)");
    EXPECT_EQ(text(time::to_string(time::month(255))), "%!Month(255)");
    EXPECT_EQ(text(time::to_string(time::weekday(0))), "%!Weekday(0)");
    EXPECT_EQ(text(time::to_string(time::weekday(8))), "%!Weekday(8)");
    EXPECT_EQ(text(time::to_string(time::month::december)), "December");
    EXPECT_EQ(text(time::to_string(time::weekday::sunday)), "Sunday");
    EXPECT_EQ(text(txt::format("{}", time::month(13))), "%!Month(13)");
    EXPECT_EQ(text(txt::format("{:%B}", time::month(13))), "%!Month(13)");
    EXPECT_EQ(text(txt::format("{}", time::weekday(0))), "%!Weekday(0)");
}

//----------------------------------------------------------------------------
// datetime
//----------------------------------------------------------------------------

TEST(DatetimeBoundary_Tests, TheDefaultIsTheEpochInUtc) {
    datetime t;
    EXPECT_EQ(t.unix(), 0);
    EXPECT_EQ(t.unix_milli(), 0);
    EXPECT_EQ(t.unix_micro(), 0);
    EXPECT_EQ(t.unix_nano(), 0);
    EXPECT_EQ(t.zone(), zone::utc());
    EXPECT_EQ(text(t.zone().name()), "UTC");
    EXPECT_EQ(t.offset(), duration());
    EXPECT_EQ(text(t.abbreviation()), "UTC");
    EXPECT_FALSE(t.is_dst());
    EXPECT_EQ(t.date(), date());
    EXPECT_EQ(t.hour(), 0);
    EXPECT_EQ(t.minute(), 0);
    EXPECT_EQ(t.second(), 0);
    EXPECT_EQ(t.nanosecond(), 0);
    EXPECT_EQ(t.weekday(), time::weekday::thursday);
    EXPECT_EQ(t.year_day(), 1);
    EXPECT_EQ(text(t.to_string()), "1970-01-01T00:00:00Z");
    EXPECT_EQ(t.to_sys().time_since_epoch().count(), 0);
    EXPECT_EQ(t, datetime::from_unix(0, zone::utc()));
    EXPECT_EQ(t.start_of_day(), t);
    EXPECT_EQ(t.truncate(24h), t);
    EXPECT_EQ(t.round(24h), t);
}

// t - d is t + (-d): the negation of duration::min() is duration::max(),
// a nanosecond short, so the subtraction is its own
TEST(DatetimeBoundary_Tests, SubtractingTheSmallestDuration) {
    EXPECT_EQ((lo() - duration::min()).unix_nano(), 0);
    EXPECT_EQ(datetime() - duration::min(), hi());
    EXPECT_EQ((hi() + duration::min()).unix_nano(), -1);
    EXPECT_EQ((lo() + duration::max()).unix_nano(), -1);
    EXPECT_EQ((hi() - duration::max()).unix_nano(), 0);
    EXPECT_EQ(lo() - duration::max(), lo());
    datetime t = lo();
    t -= duration::min();
    EXPECT_EQ(t.unix_nano(), 0);
    t = datetime::from_unix_nano(-5, zone::utc());
    t -= duration::min();
    EXPECT_EQ(t.unix_nano(), INT64_MAX - 4);
    t = datetime::from_unix_nano(5, zone::utc());
    t -= duration::min();
    EXPECT_EQ(t, hi());
    t = datetime::from_unix_nano(5, zone::utc());
    t -= duration::max();
    EXPECT_EQ(t.unix_nano(), INT64_MIN + 6);
    EXPECT_EQ(hi() - hi(), duration());
    EXPECT_EQ(lo() - datetime(), duration(chr::nanoseconds(INT64_MIN)));
    EXPECT_EQ(datetime() - lo(), duration::max());
}

// The steps of truncate and round up to duration::max(): the remainder
// from Go's zero time is two remainders added, which pass 2^63 when the
// step is more than half of it
TEST(DatetimeBoundary_Tests, TruncateAndRoundByTheLongestSteps) {
    std::vector<int64_t> instants = {INT64_MIN, INT64_MIN + 1, -5000000000000000000, -1, 0, 1, 2430000000000000000,
                                     5000000000000000000, 9000000000000000000, INT64_MAX - 1, INT64_MAX};
    std::vector<int64_t> steps = {1, 2, 3, 1000000000, 3600000000000, 86400000000000, INT64_MAX / 2, INT64_MAX / 2 + 1,
                                  4611686018427387904 + 12345, 7000000000000000000, INT64_MAX - 1, INT64_MAX};
    for (int64_t ns : instants) {
        for (int64_t step : steps) {
            datetime t = datetime::from_unix_nano(ns, zone::utc());
            ASSERT_EQ(t.truncate(chr::nanoseconds(step)).unix_nano(), truncated(ns, step)) << ns << " " << step;
            ASSERT_EQ(t.round(chr::nanoseconds(step)).unix_nano(), rounded(ns, step)) << ns << " " << step;
        }
    }
    EXPECT_EQ(hi().truncate(duration::max()).zone(), zone::utc());
    datetime w = hi().in(load("Europe/Warsaw"));
    EXPECT_EQ(w.truncate(duration::max()).zone(), w.zone());
    EXPECT_EQ(datetime().truncate(duration::min()), datetime());
    EXPECT_EQ(datetime().round(-1ns), datetime());
}

// The milliseconds and microseconds of every count near both ends of 64
// bits, near the ends of the range, round zero and across the counts:
// the instant is the count times the unit, the end of the range beyond
// it. The part below a second is worked modulo 2^64, where the product
// of the least counts wraps and the difference must still be exact
TEST(DatetimeBoundary_Tests, MilliAndMicroOfEveryCountNearTheEnds) {
    auto expected = [](int64_t count, int64_t unit) {
        __int128 v = (__int128)count * unit;
        return v < INT64_MIN ? INT64_MIN : v > INT64_MAX ? INT64_MAX : int64_t(v);
    };
    auto check = [&](int64_t count) {
        ASSERT_EQ(datetime::from_unix_milli(count, zone::utc()).unix_nano(), expected(count, 1000000)) << count;
        ASSERT_EQ(datetime::from_unix_micro(count, zone::utc()).unix_nano(), expected(count, 1000)) << count;
    };
    for (int64_t centre : {INT64_MIN, INT64_MAX, int64_t(0), INT64_MIN / 1000000, INT64_MAX / 1000000,
                           INT64_MIN / 1000, INT64_MAX / 1000}) {
        for (int64_t d = -2500000; d <= 2500000; ++d) {
            if ((d < 0 && centre < INT64_MIN - d) || (d > 0 && centre > INT64_MAX - d)) {
                continue;
            }
            check(centre + d);
        }
    }
    uint64_t x = 0x9E3779B97F4A7C15;
    for (int i = 0; i < 100000; ++i) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        check(int64_t(x));
        check(int64_t(x) >> (i % 64));
    }
}

TEST(DatetimeBoundary_Tests, TheConstructorsAtTheEnds) {
    EXPECT_EQ(datetime::from_unix(INT64_MAX, zone::utc()), hi());
    EXPECT_EQ(datetime::from_unix(INT64_MIN, zone::utc()), lo());
    EXPECT_EQ(datetime::from_unix_milli(INT64_MAX, zone::utc()), hi());
    EXPECT_EQ(datetime::from_unix_milli(INT64_MIN, zone::utc()), lo());
    EXPECT_EQ(datetime::from_unix_micro(INT64_MAX, zone::utc()), hi());
    EXPECT_EQ(datetime::from_unix_micro(INT64_MIN, zone::utc()), lo());
    EXPECT_EQ(datetime::from_unix_micro(-9223372036854775, zone::utc()).unix_nano(), -9223372036854775000);
    EXPECT_EQ(datetime::from_unix_micro(-9223372036854776, zone::utc()), lo());
    EXPECT_EQ(datetime::from_unix(9223372036, zone::utc()).unix(), 9223372036);
    EXPECT_EQ(datetime::from_unix(9223372037, zone::utc()), hi());
    EXPECT_EQ(datetime::from_unix(-9223372036, zone::utc()).unix(), -9223372036);
    EXPECT_EQ(datetime::from_unix(-9223372037, zone::utc()), lo());
    EXPECT_EQ(datetime(chr::sys_time<chr::nanoseconds>(chr::nanoseconds(INT64_MIN)), zone::utc()), lo());
    EXPECT_EQ(datetime(chr::sys_time<chr::nanoseconds>(chr::nanoseconds(INT64_MAX)), zone::utc()), hi());
    // The readings, rounded down
    EXPECT_EQ(lo().unix(), -9223372037);
    EXPECT_EQ(lo().unix_milli(), -9223372036855);
    EXPECT_EQ(lo().unix_micro(), -9223372036854776);
    EXPECT_EQ(hi().unix(), 9223372036);
    EXPECT_EQ(hi().unix_milli(), 9223372036854);
    EXPECT_EQ(hi().unix_micro(), 9223372036854775);
    EXPECT_EQ(hi().nanosecond(), 854775807);
    EXPECT_EQ(datetime::from_unix_nano(-1, zone::utc()).unix(), -1);
    EXPECT_EQ(datetime::from_unix_nano(-1, zone::utc()).nanosecond(), 999999999);
    EXPECT_EQ(datetime::from_unix_nano(-1, zone::utc()).second(), 59);
}

TEST(DatetimeBoundary_Tests, TheCalendarsArithmeticAtTheEnds) {
    datetime t = date(2026, 9, 24).at(12, 0, zone::utc());
    EXPECT_EQ(t.add_days(INT_MAX), hi());
    EXPECT_EQ(t.add_days(INT_MIN), lo());
    EXPECT_EQ(t.add_months(INT_MAX), hi());
    EXPECT_EQ(t.add_months(INT_MIN), lo());
    EXPECT_EQ(t.add_years(INT_MAX), hi());
    EXPECT_EQ(t.add_years(INT_MIN), lo());
    EXPECT_EQ(hi().add_days(1), hi());
    EXPECT_EQ(lo().add_days(-1), lo());
    EXPECT_EQ(lo().add_days(0), lo());
    EXPECT_EQ(hi().add_days(0), hi());
    // A leap day a year on, at the time of the clock
    datetime leap = date(2024, 2, 29).at(12, 30, load("Europe/Warsaw"));
    EXPECT_EQ(text(leap.add_years(1).to_string()), "2025-02-28T12:30:00+01:00");
    EXPECT_EQ(text(leap.add_years(4).to_string()), "2028-02-29T12:30:00+01:00");
    EXPECT_EQ(text(leap.add_months(1).to_string()), "2024-03-29T12:30:00+01:00");
    EXPECT_EQ(text(leap.add_days(1).to_string()), "2024-03-01T12:30:00+01:00");
}

TEST(DatetimeBoundary_Tests, TheZoneAtTheEnds) {
    zone w = load("Europe/Warsaw");
    EXPECT_FALSE(w.next_transition(hi()));
    EXPECT_FALSE(w.previous_transition(lo()));
    EXPECT_FALSE(w.next_transition(hi() - 24h));   // the next change is past 2262-04-11
    EXPECT_TRUE(w.previous_transition(hi()));
    EXPECT_TRUE(w.next_transition(lo()));
    EXPECT_EQ(text(w.abbreviation_at(lo())), "LMT");
    EXPECT_EQ(w.offset_at(lo()), 1h + 24min);
    EXPECT_FALSE(w.is_dst_at(lo()));
    EXPECT_EQ(text(lo().in(w).to_string()), "1677-09-21T01:36:43.145224192+01:24");   // the seconds of LMT's offset are not written
    EXPECT_EQ(text(lo().in(w).format("%z %Ez")), "+0124 +01:24");
    EXPECT_EQ(text(hi().in(w).to_string()), "2262-04-12T01:47:16.854775807+02:00");
    EXPECT_EQ(hi().in(w).date(), date(2262, 4, 12));
    EXPECT_FALSE(zone::utc().next_transition(lo()));
    EXPECT_FALSE(zone::utc().previous_transition(hi()));
}

TEST(DatetimeBoundary_Tests, TheLayoutsAtTheEnds) {
    EXPECT_EQ(text(lo().format(time::rfc3339)), "1677-09-21T00:12:43Z");
    EXPECT_EQ(text(lo().format(time::rfc3339_nano)), "1677-09-21T00:12:43.145224192Z");
    EXPECT_EQ(text(lo().format(time::http)), "Tue, 21 Sep 1677 00:12:43 GMT");
    EXPECT_EQ(text(lo().format(time::email)), "Tue, 21 Sep 1677 00:12:43 +0000");
    EXPECT_EQ(text(lo().format(time::iso8601)), "1677-09-21T00:12:43.145224192Z");
    EXPECT_EQ(text(hi().format(time::rfc3339)), "2262-04-11T23:47:16Z");
    EXPECT_EQ(text(hi().format(time::http)), "Fri, 11 Apr 2262 23:47:16 GMT");
    // The first and the last instant read back from every layout that keeps
    // the nanoseconds
    for (datetime t : {lo(), hi()}) {
        for (auto f : {time::rfc3339_nano, time::iso8601}) {
            auto back = datetime::parse(t.format(f), f);
            ASSERT_TRUE(back);
            EXPECT_EQ(*back, t);
        }
    }
    EXPECT_FALSE(datetime::parse("1677-09-21T00:12:43.145224191Z", time::rfc3339));
    EXPECT_TRUE(datetime::parse("1677-09-21T00:12:43.145224192Z", time::rfc3339));
    EXPECT_TRUE(datetime::parse("2262-04-11T23:47:16.854775807Z", time::rfc3339));
    EXPECT_FALSE(datetime::parse("2262-04-11T23:47:16.854775808Z", time::rfc3339));
    EXPECT_FALSE(datetime::parse("2262-04-12T01:47:16.854775808+02:00", time::rfc3339));
    EXPECT_TRUE(datetime::parse("2262-04-12T01:47:16.854775807+02:00", time::rfc3339));
    EXPECT_FALSE(datetime::parse("0000-01-01T00:00:00Z", time::rfc3339));
    EXPECT_FALSE(datetime::parse("9999-12-31T23:59:59Z", time::rfc3339));
    EXPECT_FALSE(datetime::parse("1677-09-21", time::iso8601));
    EXPECT_TRUE(datetime::parse("1677-09-22", time::iso8601));
    EXPECT_FALSE(datetime::parse("32767-12-31", time::iso8601));
    EXPECT_FALSE(datetime::parse("2262-04-11T24:00", time::iso8601));
    EXPECT_FALSE(datetime::parse("Sun, 06 Nov 1600 08:49:37 GMT", time::http));
    EXPECT_FALSE(datetime::parse("Sun, 06 Nov 1600 08:49:37 +0000", time::email));
}

// A second of 60 is a leap second only as the last second of a day in UTC
TEST(DatetimeBoundary_Tests, TheLeapSecond) {
    auto next_day = date(2017, 1, 1).at(0, 0, zone::utc());
    EXPECT_EQ(datetime::parse("2016-12-31T23:59:60Z", time::rfc3339), next_day);
    EXPECT_EQ(datetime::parse("2016-12-31t23:59:60z", time::rfc3339), next_day);
    EXPECT_EQ(datetime::parse("2017-01-01T00:59:60+01:00", time::rfc3339), next_day);
    EXPECT_EQ(datetime::parse("2016-12-31T18:59:60-05:00", time::rfc3339), next_day);
    EXPECT_EQ(datetime::parse("2016-12-31T23:59:60.5Z", time::rfc3339), next_day + 500ms);
    EXPECT_FALSE(datetime::parse("2016-12-31T23:59:60+01:00", time::rfc3339));
    EXPECT_FALSE(datetime::parse("2016-12-31T23:58:60Z", time::rfc3339));
    EXPECT_FALSE(datetime::parse("2016-12-31T23:59:61Z", time::rfc3339));
    EXPECT_FALSE(datetime::parse("2016-12-31T24:00:00Z", time::rfc3339));
    EXPECT_FALSE(datetime::parse("2016-12-31T23:60:00Z", time::rfc3339));
    EXPECT_EQ(datetime::parse("2016-12-31T23:59:60Z", time::iso8601), next_day);
    EXPECT_FALSE(datetime::parse("2016-12-31T23:59:60+01", time::iso8601));
    EXPECT_EQ(datetime::parse("Sat, 31 Dec 2016 23:59:60 GMT", time::http), next_day);
    EXPECT_EQ(datetime::parse("Sat, 31 Dec 2016 23:59:60 +0000", time::email), next_day);
    EXPECT_FALSE(datetime::parse("Sat, 31 Dec 2016 23:59:60 +0100", time::email));
    EXPECT_EQ(datetime::parse("2016-12-31 23:59:60", "%F %T"), next_day);
    EXPECT_EQ(datetime::parse("2016-12-31 23:59:60 +0000", "%F %T %z"), next_day);
    EXPECT_EQ(datetime::parse("2016-12-31 23:59:60 UTC", "%F %T %Z", load("Europe/Warsaw")), next_day);
    EXPECT_FALSE(datetime::parse("2016-12-31 23:59:60", "%F %T", load("Europe/Warsaw")));
    EXPECT_FALSE(datetime::parse("2016-12-31 23:59:61", "%F %T"));
    // At the end of the range: the instant after the leap second is past it
    EXPECT_FALSE(datetime::parse("2262-04-11T23:59:60Z", time::rfc3339));
}

TEST(DatetimeBoundary_Tests, TheFieldsOfTheLayoutsAtTheirLimits) {
    using time::rfc3339;
    EXPECT_FALSE(datetime::parse("", rfc3339));
    EXPECT_FALSE(datetime::parse("", time::http));
    EXPECT_FALSE(datetime::parse("", time::email));
    EXPECT_FALSE(datetime::parse("", time::iso8601));
    EXPECT_EQ(datetime::parse("", rfc3339).error().offset(), 0u);
    EXPECT_TRUE(datetime::parse("2026-01-01T00:00:00+23:59", rfc3339));
    EXPECT_TRUE(datetime::parse("2026-01-01T00:00:00-23:59", rfc3339));
    EXPECT_FALSE(datetime::parse("2026-01-01T00:00:00+24:00", rfc3339));
    EXPECT_FALSE(datetime::parse("2026-01-01T00:00:00+23:60", rfc3339));
    EXPECT_EQ(datetime::parse("2026-01-01T00:00:00-00:00", rfc3339)->zone(), zone::utc());
    EXPECT_EQ(datetime::parse("2026-01-01T00:00:00+00:00", rfc3339)->zone(), zone::utc());
    EXPECT_EQ(datetime::parse("2026-01-01T00:00:00+23:59", rfc3339)->zone(), zone::fixed(23h + 59min));
    // A fraction of any length, the nanoseconds kept
    auto long_fraction = datetime::parse("2026-01-01T00:00:00.12345678999999999999999Z", rfc3339);
    ASSERT_TRUE(long_fraction);
    EXPECT_EQ(long_fraction->nanosecond(), 123456789);
    EXPECT_FALSE(datetime::parse("2026-01-01T00:00:00.Z", rfc3339));
    EXPECT_EQ(datetime::parse("2026-01-01T23:59:59.999999999Z", rfc3339)->nanosecond(), 999999999);
    EXPECT_FALSE(datetime::parse("2026-02-29T00:00:00Z", rfc3339));
    EXPECT_TRUE(datetime::parse("2024-02-29T00:00:00Z", rfc3339));
    EXPECT_FALSE(datetime::parse("2026-01-01T00:00:00", rfc3339));
    EXPECT_FALSE(datetime::parse("2026-01-01T00:00:00Z ", rfc3339));
    // HTTP's three forms
    EXPECT_FALSE(datetime::parse("Thu, 32 Sep 2026 10:41:15 GMT", time::http));
    EXPECT_FALSE(datetime::parse("Thu, 00 Sep 2026 10:41:15 GMT", time::http));
    EXPECT_FALSE(datetime::parse("Sun, 29 Feb 2026 10:41:15 GMT", time::http));
    EXPECT_TRUE(datetime::parse("Thu, 29 Feb 2024 10:41:15 GMT", time::http));
    EXPECT_FALSE(datetime::parse("Thu, 24 Sep 2026 24:00:00 GMT", time::http));
    EXPECT_FALSE(datetime::parse("Thu, 24 Sep 2026 10:41:15 UTC", time::http));
    EXPECT_FALSE(datetime::parse("Thu, 24 Sep 2026 10:41:15 GMT ", time::http));
    EXPECT_TRUE(datetime::parse("Thu Sep  4 10:41:15 2026", time::http));
    EXPECT_TRUE(datetime::parse("Thu Sep 04 10:41:15 2026", time::http));
    EXPECT_FALSE(datetime::parse("Thu Sep 31 10:41:15 2026", time::http));
    EXPECT_FALSE(datetime::parse("Thu Sep 4", time::http));
    EXPECT_FALSE(datetime::parse("Thursday, 31-Sep-26 10:41:15 GMT", time::http));
    EXPECT_FALSE(datetime::parse("Thu", time::http));
    EXPECT_FALSE(datetime::parse("Thu,", time::http));
    // E-mail's: obsolete years, comments, zones
    EXPECT_EQ(datetime::parse("1 Jan 49 00:00 +0000", time::email)->year(), 2049);
    EXPECT_EQ(datetime::parse("1 Jan 50 00:00 +0000", time::email)->year(), 1950);
    EXPECT_EQ(datetime::parse("1 Jan 126 00:00 +0000", time::email)->year(), 2026);
    EXPECT_EQ(datetime::parse("1 Jan 199 00:00 +0000", time::email)->year(), 2099);
    EXPECT_FALSE(datetime::parse("1 Jan 999 00:00 +0000", time::email));   // 2899, past the range
    EXPECT_FALSE(datetime::parse("1 Jan 12345 00:00 +0000", time::email));
    EXPECT_FALSE(datetime::parse("1 Jan 2026 00:00 +2400", time::email));
    EXPECT_FALSE(datetime::parse("1 Jan 2026 00:00 +0060", time::email));
    EXPECT_TRUE(datetime::parse("1 Jan 2026 00:00 -2359", time::email));
    EXPECT_FALSE(datetime::parse("1 Jan 2026 00:00 +0000 (a comment", time::email));
    EXPECT_FALSE(datetime::parse("1 Jan 2026 00:00 +0000 (\\", time::email));
    EXPECT_FALSE(datetime::parse("1 Jan 2026 00:00 +0000 (\\)", time::email));
    EXPECT_TRUE(datetime::parse("1 Jan 2026 00:00 +0000 (a (nested) \\) comment)", time::email));
    EXPECT_TRUE(datetime::parse("1 Jan 2026 00:00 Z", time::email));
    EXPECT_FALSE(datetime::parse("1 Jan 2026 00:00 J", time::email));
    EXPECT_FALSE(datetime::parse("1 Jan 2026 00:00 ESTX", time::email));
    EXPECT_FALSE(datetime::parse("0 Jan 2026 00:00 +0000", time::email));
    EXPECT_FALSE(datetime::parse("1 Jan 2026 00:00", time::email));
    // ISO 8601: 24:00, the fractions of hours and minutes, the offsets
    auto midnight = date(2026, 9, 25).at(0, 0, zone::utc());
    EXPECT_EQ(datetime::parse("2026-09-24T24:00", time::iso8601), midnight);
    EXPECT_EQ(datetime::parse("2026-09-24T24", time::iso8601), midnight);
    EXPECT_EQ(datetime::parse("2026-09-24T24:00:00", time::iso8601), midnight);
    EXPECT_EQ(datetime::parse("2026-12-31T24:00", time::iso8601), date(2027, 1, 1).at(0, 0, zone::utc()));
    EXPECT_FALSE(datetime::parse("2026-09-24T24:00:01", time::iso8601));
    EXPECT_FALSE(datetime::parse("2026-09-24T24:00:00.5", time::iso8601));
    EXPECT_FALSE(datetime::parse("2026-09-24T25", time::iso8601));
    EXPECT_EQ(datetime::parse("2026-09-24T12.5", time::iso8601), date(2026, 9, 24).at(12, 30, zone::utc()));
    EXPECT_EQ(datetime::parse("2026-09-24T12:30.5", time::iso8601), date(2026, 9, 24).at(12, 30, 30, zone::utc()));
    EXPECT_EQ(datetime::parse("2026-09-24T23.999999999999", time::iso8601)->unix_nano(),
              date(2026, 9, 24).at(23, 59, 59, zone::utc()).unix_nano() + 999996400);   // nine digits of the hour kept
    EXPECT_EQ(datetime::parse("2026-09-24T12+05", time::iso8601)->zone(), zone::fixed(5h));
    EXPECT_EQ(datetime::parse("2026-09-24T12-0530", time::iso8601)->zone(), zone::fixed(-(5h + 30min)));
    EXPECT_FALSE(datetime::parse("2026-09-24T12+24", time::iso8601));
    EXPECT_FALSE(datetime::parse("2026-09-24T12+05:", time::iso8601));
    EXPECT_FALSE(datetime::parse("2026-09-24T", time::iso8601));
    EXPECT_FALSE(datetime::parse("T12:00", time::iso8601));
    EXPECT_FALSE(datetime::parse("2026-09-24T12:3", time::iso8601));
    EXPECT_FALSE(datetime::parse("2026-09-24T1230:15", time::iso8601));
}

TEST(DatetimeBoundary_Tests, APatternsFieldsAtTheirLimits) {
    auto at = [](int h, int m, int s) { return date(2026, 9, 24).at(h, m, s, zone::utc()); };
    EXPECT_EQ(datetime::parse("2026-09-24 12 AM", "%F %I %p"), at(0, 0, 0));
    EXPECT_EQ(datetime::parse("2026-09-24 12 PM", "%F %I %p"), at(12, 0, 0));
    EXPECT_EQ(datetime::parse("2026-09-24 11 pm", "%F %I %p"), at(23, 0, 0));
    EXPECT_FALSE(datetime::parse("2026-09-24 00 AM", "%F %I %p"));
    EXPECT_FALSE(datetime::parse("2026-09-24 13 PM", "%F %I %p"));
    EXPECT_FALSE(datetime::parse("2026-09-24 11", "%F %I"));   // 12 hours need AM or PM
    EXPECT_EQ(datetime::parse("2026-09-24 23:59:59", "%F %T"), at(23, 59, 59));
    EXPECT_FALSE(datetime::parse("2026-09-24 24:00:00", "%F %T"));
    EXPECT_FALSE(datetime::parse("2026-09-24 23:60:00", "%F %T"));
    EXPECT_EQ(datetime::parse("2026-09-24 23:59:59.999999999", "%F %T")->nanosecond(), 999999999);
    EXPECT_FALSE(datetime::parse("2026-09-24 23:59:59.9999999999", "%F %T"));   // nine digits at most
    EXPECT_EQ(datetime::parse("2026-09-24 00:00:00 +2359", "%F %T %z")->offset(), 23h + 59min);
    EXPECT_EQ(datetime::parse("2026-09-24 00:00:00 -2359", "%F %T %z")->offset(), -(23h + 59min));
    EXPECT_FALSE(datetime::parse("2026-09-24 00:00:00 +2400", "%F %T %z"));
    EXPECT_FALSE(datetime::parse("2026-09-24 00:00:00 +0060", "%F %T %z"));
    EXPECT_EQ(datetime::parse("2026-09-24 00:00:00 +5:30", "%F %T %Ez")->offset(), 5h + 30min);
    EXPECT_EQ(datetime::parse("2026-09-24 00:00:00 -00:00", "%F %T %Ez")->zone(), zone::utc());
    EXPECT_FALSE(datetime::parse("2026-09-24 00:00:00 +", "%F %T %z"));
    EXPECT_FALSE(datetime::parse("2026-09-24 00:00:00 ", "%F %T %Z"));
    EXPECT_FALSE(datetime::parse("2026-09-24 00:00:00 XYZ", "%F %T %Z", load("Europe/Warsaw")));
    // The abbreviation settles a time shown twice, and refuses one the zone
    // does not have then
    zone w = load("Europe/Warsaw");
    EXPECT_EQ(datetime::parse("2026-10-25 02:30 CEST", "%F %R %Z", w)->offset(), 2h);
    EXPECT_EQ(datetime::parse("2026-10-25 02:30 CET", "%F %R %Z", w)->offset(), 1h);
    EXPECT_FALSE(datetime::parse("2026-07-01 12:00 CET", "%F %R %Z", w));
    EXPECT_FALSE(datetime::parse("2026-03-29 02:30 CET", "%F %R %Z", w));   // skipped: neither
    EXPECT_EQ(datetime::parse("2026-03-29 02:30", "%F %R", w)->hour(), 3);   // read as date::at reads it
    // A date is needed, and a range
    EXPECT_FALSE(datetime::parse("12:00", "%R"));
    EXPECT_FALSE(datetime::parse("", ""));
    EXPECT_FALSE(datetime::parse("1600-01-01", "%F"));
    EXPECT_FALSE(datetime::parse("2262-04-12", "%F"));
    EXPECT_TRUE(datetime::parse("2262-04-11", "%F"));
    EXPECT_FALSE(datetime::parse("2262-04-11 23:47:17", "%F %T"));
    EXPECT_TRUE(datetime::parse("2262-04-11 23:47:16.854775807", "%F %T"));
    EXPECT_FALSE(datetime::parse("2262-04-11 23:47:16.854775808", "%F %T"));
    EXPECT_FALSE(datetime::parse("2262-04-12 01:47:16.854775808", "%F %T", w));
    EXPECT_TRUE(datetime::parse("2262-04-12 01:47:16.854775807", "%F %T", w));
}

TEST(DatetimeBoundary_Tests, FormatAtTheEndsAsStdFormat) {
    const char* pattern = "%Y|%C|%y|%G|%g|%V|%j|%U|%W|%u|%w|%H|%I|%M|%S|%p|%R|%T|%r|%c|%D|%x|%X|%F|%z|%Ez|%Z|%a|%b|%e";
    // Not at INT64_MIN itself: libc++'s floor of it to seconds and days
    // overflows (UBSan); the first instant is checked by hand in review.cpp
    for (int64_t ns : {INT64_MIN + 2 * 86400 * int64_t(1000000000), int64_t(-1), int64_t(0), INT64_MAX - 999999999, INT64_MAX}) {
        datetime t = datetime::from_unix_nano(ns, zone::utc());
        auto sys = t.to_sys();
        std::string expected = std::vformat(std::string("{:") + pattern + "}", std::make_format_args(sys));
        EXPECT_EQ(text(t.format(pattern)), expected) << ns;
    }
    EXPECT_EQ(text(datetime().format("")), "");
    EXPECT_EQ(text(datetime().format("%")), "%");
    EXPECT_EQ(text(datetime().format("%Q%q%j")), "%Q%q001");
    std::string many;
    for (int i = 0; i < 100; ++i) {
        many += "%c ";
    }
    EXPECT_EQ(text(datetime().format(sgcl::string(many))).size(), size_t(100 * 25));
}

//----------------------------------------------------------------------------
// zone
//----------------------------------------------------------------------------

TEST(ZoneBoundary_Tests, TheDefaultIsUtc) {
    zone z;
    EXPECT_EQ(z, zone::utc());
    EXPECT_EQ(z, zone::fixed(0s));
    EXPECT_EQ(text(z.name()), "UTC");
    EXPECT_EQ(z.offset_at(datetime()), duration());
    EXPECT_EQ(text(z.abbreviation_at(datetime())), "UTC");
    EXPECT_FALSE(z.is_dst_at(datetime()));
    EXPECT_FALSE(z.next_transition(datetime()));
    EXPECT_FALSE(z.previous_transition(datetime()));
    EXPECT_EQ(zone::load("UTC"), z);
    EXPECT_NE(load("Etc/UTC"), z);   // a zone of the database, read from its file
    zone copy = z;
    const zone& alias = copy;
    copy = alias;
    EXPECT_EQ(copy, z);
}

TEST(ZoneBoundary_Tests, AFixedOffsetAtItsLimits) {
    EXPECT_EQ(text(zone::fixed(86399s).name()), "+23:59:59");
    EXPECT_EQ(text(zone::fixed(-86399s).name()), "-23:59:59");
    EXPECT_EQ(text(zone::fixed(86399999ms).name()), "+23:59:59");   // the rest of a second dropped
    EXPECT_EQ(text(zone::fixed(-86399999ms).name()), "-23:59:59");
    EXPECT_THROW(zone::fixed(86400s), invalid_argument);
    EXPECT_THROW(zone::fixed(-86400s), invalid_argument);
    EXPECT_THROW(zone::fixed(duration::max()), invalid_argument);
    EXPECT_THROW(zone::fixed(duration::min()), invalid_argument);
    EXPECT_EQ(zone::fixed(1ns), zone::utc());
    EXPECT_EQ(zone::fixed(-999999999ns), zone::utc());
    EXPECT_EQ(zone::fixed(-1s), zone::fixed(-1s));
    EXPECT_EQ(text(zone::fixed(-1s).name()), "-00:00:01");
    EXPECT_EQ(text(zone::fixed(24h - 15min).name()), "+23:45");
    EXPECT_EQ(text(zone::fixed(-(24h - 15min)).name()), "-23:45");
    EXPECT_EQ(zone::fixed(86399s).offset_at(datetime()), 86399s);
    EXPECT_EQ(text(datetime().in(zone::fixed(86399s)).to_string()), "1970-01-01T23:59:59+23:59");
    EXPECT_EQ(text(datetime().in(zone::fixed(-86399s)).to_string()), "1969-12-31T00:00:01-23:59");
    EXPECT_EQ(text(lo().in(zone::fixed(-86399s)).to_string()), "1677-09-20T00:12:44.145224192-23:59");
    EXPECT_EQ(text(hi().in(zone::fixed(86399s)).to_string()), "2262-04-12T23:47:15.854775807+23:59");
    EXPECT_EQ(date(2026, 1, 1).at(0, 0, zone::fixed(86399s)).unix(), date(2025, 12, 31).at(0, 0, 1, zone::utc()).unix());
}

TEST(ZoneBoundary_Tests, LoadAtItsLimits) {
    EXPECT_FALSE(zone::load(""));
    EXPECT_FALSE(zone::load(sgcl::string(std::string(256, 'A'))));
    auto long_name = zone::load(sgcl::string(std::string(255, 'A')));
    ASSERT_FALSE(long_name);
    EXPECT_NE(text(long_name.error().message()).find("unknown"), std::string::npos);   // a name, not there
    EXPECT_FALSE(zone::load("Europe"));                                                // a directory
    EXPECT_FALSE(zone::load("Europe/Warsaw/x"));                                       // under a file
    EXPECT_FALSE(zone::load(sgcl::string(std::string("Europe/Warsaw\0", 14))));
    EXPECT_FALSE(zone::load("/etc/localtime"));
    EXPECT_FALSE(zone::load(".."));
    EXPECT_FALSE(zone::load("a/../Europe/Warsaw"));
    EXPECT_EQ(load("Europe/Warsaw"), load("Europe/Warsaw"));
    EXPECT_THROW(zone("Europe"), bad_expected_access<time::error>);
}

// TZif files at the limits of RFC 9636: the offsets, the count of types,
// transitions at the ends of 64 bits, no transitions, a footer alone
TEST(ZoneBoundary_Tests, TzifAtItsLimits) {
    std::string utc_chars("UTC\0", 4);
    EXPECT_FALSE(zone::from_tzif(bytes_of(""), "Empty"));
    EXPECT_FALSE(zone::from_tzif(bytes_of("TZif"), "Short"));
    // The offsets: -25 hours exclusive to 26 exclusive
    EXPECT_TRUE(zone::from_tzif(bytes_of(tzif({}, {}, {{-89999, 0, 0}}, utc_chars, "")), "MinOffset"));
    EXPECT_FALSE(zone::from_tzif(bytes_of(tzif({}, {}, {{-90000, 0, 0}}, utc_chars, "")), "PastMinOffset"));
    EXPECT_TRUE(zone::from_tzif(bytes_of(tzif({}, {}, {{93599, 0, 0}}, utc_chars, "")), "MaxOffset"));
    EXPECT_FALSE(zone::from_tzif(bytes_of(tzif({}, {}, {{93600, 0, 0}}, utc_chars, "")), "PastMaxOffset"));
    auto far_east = zone::from_tzif(bytes_of(tzif({}, {}, {{93599, 0, 0}}, utc_chars, "")), "FarEast");
    ASSERT_TRUE(far_east);
    EXPECT_EQ(lo().in(*far_east).date(), date(1677, 9, 22));
    EXPECT_EQ(hi().in(*far_east).date(), date(2262, 4, 13));
    EXPECT_EQ(date(2026, 1, 1).at(0, 0, *far_east).unix(), date(2026, 1, 1).at(0, 0, zone::utc()).unix() - 93599);
    // 256 types, every index of a byte; 257 is refused
    std::vector<std::tuple<int32_t, uint8_t, uint8_t>> types;
    for (int i = 0; i < 256; ++i) {
        types.emplace_back(i * 60, 0, 0);
    }
    std::vector<int64_t> at;
    std::vector<uint8_t> type_of;
    for (int i = 0; i < 256; ++i) {
        at.push_back(1000000000 + int64_t(i) * 3600);
        type_of.push_back(uint8_t(255 - i));
    }
    auto many = zone::from_tzif(bytes_of(tzif(at, type_of, types, utc_chars, "")), "Many");
    ASSERT_TRUE(many);
    EXPECT_EQ(many->offset_at(datetime::from_unix(1000000000, zone::utc())), 255min);
    EXPECT_EQ(many->offset_at(datetime::from_unix(1000000000 + 255 * 3600, zone::utc())), 0min);
    types.emplace_back(0, 0, 0);
    EXPECT_FALSE(zone::from_tzif(bytes_of(tzif({}, {}, types, utc_chars, "")), "TooMany"));
    // Transitions at the two ends of 64 bits: past the range of a datetime,
    // they are no transition of it, and the type between them is in force
    std::string ends = tzif({INT64_MIN, INT64_MAX}, {1, 0}, {{0, 0, 0}, {3600, 0, 4}}, std::string("UTC\0ONE\0", 8), "");
    auto edges = zone::from_tzif(bytes_of(ends), "Ends");
    ASSERT_TRUE(edges);
    EXPECT_EQ(edges->offset_at(lo()), 1h);
    EXPECT_EQ(edges->offset_at(hi()), 1h);
    EXPECT_FALSE(edges->next_transition(datetime()));
    EXPECT_FALSE(edges->previous_transition(datetime()));
    EXPECT_EQ(text(date(2026, 1, 1).at(0, 0, *edges).to_string()), "2026-01-01T00:00:00+01:00");
    // The same with a footer whose rule governs after INT64_MAX: nothing of
    // it reaches the range, and reading it overflows nothing
    std::string ends_rule = tzif({INT64_MIN, INT64_MAX}, {1, 0}, {{0, 0, 0}, {3600, 0, 4}}, std::string("UTC\0ONE\0", 8), "<+14>-14<+15>-15,J1/0,J365/25");
    auto edges_rule = zone::from_tzif(bytes_of(ends_rule), "EndsRule");
    ASSERT_TRUE(edges_rule);
    EXPECT_EQ(edges_rule->offset_at(hi()), 1h);
    std::string first_rule = tzif({INT64_MIN}, {1}, {{0, 0, 0}, {3600, 0, 4}}, std::string("UTC\0ONE\0", 8), "<-12>12<-11>11,M3.5.0/-167,M10.5.0/167");
    auto from_first = zone::from_tzif(bytes_of(first_rule), "FirstRule");
    ASSERT_TRUE(from_first);
    EXPECT_EQ(from_first->offset_at(datetime::from_unix(0, zone::utc())), -12h);
    // No transitions and no footer: the one type for all time
    auto one = zone::from_tzif(bytes_of(tzif({}, {}, {{-3600, 0, 0}}, std::string("XXX\0", 4), "")), "One");
    ASSERT_TRUE(one);
    EXPECT_EQ(one->offset_at(lo()), -1h);
    EXPECT_EQ(text(one->abbreviation_at(hi())), "XXX");
    EXPECT_FALSE(one->next_transition(lo()));
    // The same bytes under another name are another zone, under the same
    // one the same
    EXPECT_EQ(*one, *zone::from_tzif(bytes_of(tzif({}, {}, {{-3600, 0, 0}}, std::string("XXX\0", 4), "")), "One"));
    EXPECT_NE(*one, *zone::from_tzif(bytes_of(tzif({}, {}, {{-3600, 0, 0}}, std::string("XXX\0", 4), "")), "Two"));
    EXPECT_EQ(text(one->name()), "One");
    EXPECT_EQ(text(zone::from_tzif(bytes_of(tzif({}, {}, {{0, 0, 0}}, utc_chars, "")), "")->name()), "");
}

// A TZ string at the limits of POSIX and RFC 9636: offsets of 24 hours,
// rule times of ±167 hours, the first and the last day of each kind
TEST(ZoneBoundary_Tests, PosixAtItsLimits) {
    for (const char* rule : {"UTC0", "ABC24", "ABC-24", "<+0330>-3:30", "<-00>0", "ABC-24DEF-23:59:59", "EST5EDT",
                             "CET-1CEST,J1/0,J365/25", "CET-1CEST,0/-167,365/167", "CET-1CEST,M1.1.0/0,M12.5.6/23:59:59",
                             "<+14>-14<+15>-15,J1/0,J365/25", "<-12>12<-11>11,M3.5.0/-167,M10.5.0/167"}) {
        auto z = zone::from_posix(rule);
        ASSERT_TRUE(z) << rule << ": " << text(z.error().message());
        EXPECT_EQ(text(z->name()), rule);
        // Walked from one end of the range to the other, the transitions go
        // forward and each changes something
        int count = 0;
        optional<datetime> t = z->next_transition(lo());
        optional<datetime> before;
        while (t && count < 2000) {
            if (before) {
                ASSERT_GT(*t, *before) << rule;
            }
            datetime just_before = *t - 1ns;
            EXPECT_TRUE(z->offset_at(*t) != z->offset_at(just_before) || z->is_dst_at(*t) != z->is_dst_at(just_before)
                        || z->abbreviation_at(*t) != z->abbreviation_at(just_before)) << rule << " " << *t;
            EXPECT_EQ(z->previous_transition(*t + 1ns), t) << rule;
            before = t;
            t = z->next_transition(*t);
            ++count;
        }
        EXPECT_LT(count, 2000) << rule;
        EXPECT_FALSE(z->next_transition(hi())) << rule;
    }
    for (const char* rule : {"", "AB0", "ABC", "ABC25", "ABC-25", "ABC24:60", "ABC24:00:60", "<AB>0", "<ABC0", "ABC0DEF,",
                             "ABC0DEF,J0,J1", "ABC0DEF,J1,J366", "ABC0DEF,366,1", "ABC0DEF,M0.1.0,M1.1.0",
                             "ABC0DEF,M13.1.0,M1.1.0", "ABC0DEF,M1.0.0,M1.1.0", "ABC0DEF,M1.6.0,M1.1.0",
                             "ABC0DEF,M1.1.7,M1.1.0", "ABC0DEF,J1/168,J2", "ABC0DEF,J1/-168,J2", "ABC0DEF,J1,J2,", "ABC0 "}) {
        EXPECT_FALSE(zone::from_posix(rule)) << rule;
    }
}

TEST(ZoneBoundary_Tests, TheLocalZoneAndTheDatabase) {
    EXPECT_EQ(zone::local(), zone::local());
    EXPECT_FALSE(zone::local().name().empty());
    auto names = zone::available();
    ASSERT_FALSE(names.empty());
    for (size_t i = 1; i < names.size(); ++i) {
        ASSERT_LT(names[i - 1], names[i]);
    }
}

//----------------------------------------------------------------------------
// error, layout, stopwatch
//----------------------------------------------------------------------------

TEST(ErrorBoundary_Tests, TheFieldsAtTheirLimits) {
    time::error e("");
    EXPECT_EQ(text(e.message()), "");
    EXPECT_EQ(e.offset(), 0u);
    time::error far("x", SIZE_MAX);
    EXPECT_EQ(far.offset(), SIZE_MAX);
    EXPECT_EQ(far, time::error("x", SIZE_MAX));
    EXPECT_NE(far, time::error("x", 0));
    EXPECT_NE(far, time::error("y", SIZE_MAX));
    time::error copy = far;
    const time::error& alias = copy;
    copy = alias;
    EXPECT_EQ(copy, far);
    time::error moved = std::move(copy);
    EXPECT_EQ(moved, far);
}

TEST(LayoutBoundary_Tests, EveryLayoutIsItself) {
    std::vector<time::layout> all = {time::rfc3339, time::rfc3339_nano, time::http, time::email, time::iso8601};
    for (size_t i = 0; i < all.size(); ++i) {
        for (size_t j = 0; j < all.size(); ++j) {
            EXPECT_EQ(all[i] == all[j], i == j);
        }
    }
}

TEST(StopwatchBoundary_Tests, RestartAndMeasure) {
    manual_clock clock;
    clock.install();
    time::stopwatch sw;
    EXPECT_EQ(sw.restart(), duration());
    EXPECT_EQ(sw.restart(), duration());
    clock.advance(1ns);
    EXPECT_EQ(sw.restart(), 1ns);
    EXPECT_EQ(sw.elapsed(), duration());
    EXPECT_EQ(time::stopwatch::measure([] {}), duration());
    EXPECT_EQ(time::stopwatch::measure([&] { clock.advance(5s); return 42; }), 5s);
    EXPECT_THROW(time::stopwatch::measure([] { throw std::runtime_error("x"); }), std::runtime_error);
    static_assert(noexcept(time::stopwatch::measure([]() noexcept {})));
    static_assert(!noexcept(time::stopwatch::measure([] {})));
    time::stopwatch copy = sw;
    clock.advance(2s);
    EXPECT_EQ(copy.elapsed(), sw.elapsed());
}
