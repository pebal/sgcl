//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// time::date: the carrying constructor, the fields, the ISO week, the
// calendar arithmetic, ISO 8601's text; against Go (tools/time_oracle.go)
// and against the calendar and std::format of <chrono>.
#include "sgcl/time/time.h"
#include "tests/time/date_cases.h"
#include "tests/types.h"

#include <chrono>
#include <climits>
#include <format>
#include <random>
#include <sstream>
#include <string>
#include <type_traits>

namespace {
    using time::date;
    using time::iso_week;
    using time::weekday;
    namespace chr = std::chrono;

    constexpr int64_t MinDays = -12687428;   // -32767-01-01
    constexpr int64_t MaxDays = 11248737;    // 32767-12-31

    // The ISO week the long way, by the definition: the week's Thursday
    // decides the year, and the week is the count of Thursdays of that
    // year up to it; nullopt when the Thursday is outside the calendar
    // of <chrono>
    optional<iso_week> iso_week_by_thursday(chr::sys_days d) {
        auto thursday = d - (chr::weekday(d).iso_encoding() - 1) * chr::days(1) + chr::days(3);
        chr::year_month_day t(thursday);
        if (!t.year().ok()) {
            return nullopt;
        }
        auto first = chr::sys_days(t.year() / 1 / 1);
        return iso_week{static_cast<int>(t.year()), static_cast<int>((thursday - first).count() / 7 + 1)};
    }

    date at(int64_t days) {
        return date(chr::year_month_day(chr::sys_days(chr::days(days))));
    }

    std::string week_text(date d) {
        auto w = d.iso_week();
        return std::format("{}{:04}-W{:02}-{}", w.year < 0 ? "-" : "", w.year < 0 ? -w.year : w.year, w.week, static_cast<int>(d.weekday()));
    }
}

TEST(Date_Tests, CarriedAndReadAsGoDoes) {
    for (auto& c : oracle::GoDates) {
        date d(c.year, c.month, c.day);
        EXPECT_EQ(d.year(), c.y) << c.year << " " << c.month << " " << c.day;
        EXPECT_EQ(int(d.month()), c.m) << c.year << " " << c.month << " " << c.day;
        EXPECT_EQ(d.day(), c.d) << c.year << " " << c.month << " " << c.day;
        EXPECT_EQ(static_cast<int>(d.weekday()), c.weekday) << d.to_string();
        EXPECT_EQ(d.year_day(), c.year_day) << d.to_string();
        EXPECT_EQ(d.iso_week(), (iso_week{c.iso_year, c.iso_week})) << d.to_string();
        EXPECT_EQ(date::is_valid(c.year, c.month, c.day), c.y == c.year && c.m == c.month && c.d == c.day);
    }
}

TEST(Date_Tests, ArithmeticAsGoDoes) {
    for (auto& c : oracle::GoAddDays) {
        EXPECT_EQ(date(c.y, c.m, c.d).add_days(c.n), date(c.y2, c.m2, c.d2)) << c.y << "-" << c.m << "-" << c.d << " " << c.n;
    }
    for (auto& c : oracle::GoAddMonths) {
        EXPECT_EQ(date(c.y, c.m, c.d).add_months(c.n), date(c.y2, c.m2, c.d2)) << c.y << "-" << c.m << "-" << c.d << " " << c.n;
    }
    for (auto& c : oracle::GoAddYears) {
        EXPECT_EQ(date(c.y, c.m, c.d).add_years(c.n), date(c.y2, c.m2, c.d2)) << c.y << "-" << c.m << "-" << c.d << " " << c.n;
    }
    for (auto& c : oracle::GoDaysBetween) {
        EXPECT_EQ(date(c.y, c.m, c.d).days_until(date(c.y2, c.m2, c.d2)), c.days);
    }
    EXPECT_GT(std::size(oracle::GoAddDays) + std::size(oracle::GoAddMonths) + std::size(oracle::GoAddYears), 4000u);
}

TEST(Date_Tests, TheCalendarOfChrono) {
    // Every day of four centuries around now, and a day in every 7919
    // over the whole range, the first and the last two years whole: the
    // fields, the day of the year, the week by its definition, the text
    // against std::format's %F and back
    size_t checked = 0;
    auto check = [&](int64_t days) {
        chr::sys_days sd{chr::days(days)};
        chr::year_month_day ymd(sd);
        date d = ymd;
        ASSERT_EQ(d.days_until(date()), -days);
        ASSERT_EQ(static_cast<chr::year_month_day>(d), ymd);
        ASSERT_EQ(d.year(), static_cast<int>(ymd.year()));
        ASSERT_EQ(d.is_leap_year(), ymd.year().is_leap());
        ASSERT_EQ(d.days_in_month(), static_cast<int>(static_cast<unsigned>((ymd.year() / ymd.month() / chr::last).day())));
        ASSERT_EQ(d.year_day(), (sd - chr::sys_days(ymd.year() / 1 / 1)).count() + 1);
        ASSERT_EQ(static_cast<unsigned>(d.weekday()), chr::weekday(sd).iso_encoding());
        if (auto w = iso_week_by_thursday(sd)) {
            ASSERT_EQ(d.iso_week(), *w) << d.to_string();
        }
        ++checked;
    };
    for (int64_t days = date(1800, 1, 1).days_until(date()) * -1; days <= -date(2200, 12, 31).days_until(date()); ++days) {
        check(days);
    }
    for (int64_t days = MinDays; days <= MaxDays; days += 7919) {
        check(days);
    }
    for (int64_t k = 0; k < 731; ++k) {
        check(MinDays + k);
        check(MaxDays - k);
    }
    EXPECT_GT(checked, 150000u);

    for (int64_t days = MinDays; days <= MaxDays; days += 997) {
        date d = at(days);
        auto text = d.to_string();
        ASSERT_EQ(std::string_view(text), std::format("{:%F}", static_cast<chr::year_month_day>(d)));
        ASSERT_EQ(date::parse(text), d) << text;
        if (d.iso_week().year >= -32767) {   // the first two days' week is -32768's, outside the calendar
            ASSERT_EQ(date::parse(string(week_text(d))), d) << week_text(d);
        }
        date ordinal = date(d.year(), 1, d.year_day());
        ASSERT_EQ(ordinal, d);
    }
}

TEST(Date_Tests, TheEndsOfTheRange) {
    date first(-32767, 1, 1);
    date last(32767, 12, 31);
    EXPECT_EQ(first.to_string(), "-32767-01-01");
    EXPECT_EQ(last.to_string(), "32767-12-31");
    EXPECT_EQ(first.days_until(date()), 12687428);
    EXPECT_EQ(date().days_until(last), 11248737);
    // Beyond them, the end: a result saturates, as a duration does
    EXPECT_EQ(date(32767, 12, 32), last);
    EXPECT_EQ(date(-32767, 1, 0), first);
    EXPECT_EQ(date(40000, 1, 1), last);
    EXPECT_EQ(date(INT_MIN, INT_MIN, INT_MIN), first);
    EXPECT_EQ(date(INT_MAX, INT_MAX, INT_MAX), last);
    EXPECT_EQ(last.add_days(1), last);
    EXPECT_EQ(first.add_days(-1), first);
    EXPECT_EQ(last.add_days(INT_MAX), last);
    EXPECT_EQ(first.add_days(INT_MIN), first);
    EXPECT_EQ(last.add_months(1), last);
    EXPECT_EQ(first.add_months(-1), first);
    EXPECT_EQ(date(2026, 1, 1).add_years(INT_MAX), last);
    EXPECT_EQ(date(2026, 1, 1).add_years(INT_MIN), first);
    EXPECT_EQ(date(2026, 1, 1).add_months(INT_MIN), first);
    EXPECT_EQ(first.days_until(last), 23936165);
    // A carry that comes back into the range lands exactly, from a year
    // far outside it
    EXPECT_EQ(date(-32768, 1, 367), first);   // -32768 is a leap year
    EXPECT_EQ(date(-32768, 12, 32), first);
    EXPECT_EQ(date(32768, 1, 0), last);
    EXPECT_EQ(date(2026, 1 - 12 * 100000, 1).add_years(0), first);
    EXPECT_EQ(date(0, 12 * 2026 + 9, 24), date(2026, 9, 24));
    EXPECT_EQ(date(2026, 1, 267), date(2026, 9, 24));
    EXPECT_EQ(date(-50000, 12 * 52026 + 1, 1), date(2026, 1, 1));   // the year carried from far below the range
    // The ISO weeks at the ends
    EXPECT_EQ(first.weekday(), weekday::saturday);
    EXPECT_EQ(first.iso_week(), (iso_week{-32768, 53}));   // the week of the year before the first, which starts on a Thursday
    EXPECT_FALSE(date::parse(string(week_text(first))));   // and so has no week date here
    EXPECT_EQ(date::parse("-32767-W01-1"), date(-32767, 1, 3));
    EXPECT_EQ(date::parse(string(week_text(last))), last);
}

TEST(Date_Tests, LeapYearsAndYearZero) {
    EXPECT_TRUE(date(2000, 1, 1).is_leap_year());
    EXPECT_FALSE(date(1900, 1, 1).is_leap_year());
    EXPECT_FALSE(date(2100, 1, 1).is_leap_year());
    EXPECT_TRUE(date(2024, 1, 1).is_leap_year());
    EXPECT_TRUE(date(0, 1, 1).is_leap_year());       // 1 BC, divisible by 400
    EXPECT_TRUE(date(-4, 1, 1).is_leap_year());
    EXPECT_FALSE(date(-100, 1, 1).is_leap_year());
    EXPECT_EQ(date(1900, 2, 29), date(1900, 3, 1));
    EXPECT_EQ(date(2000, 2, 29).day(), 29);
    EXPECT_EQ(date(0, 1, 1).to_string(), "0000-01-01");
    EXPECT_EQ(date(-1, 12, 31).add_days(1), date(0, 1, 1));
    EXPECT_EQ(date(-1, 1, 1).to_string(), "-0001-01-01");
    EXPECT_EQ(date(10000, 1, 1).to_string(), "10000-01-01");
    EXPECT_EQ(date(2000, 2, 1).days_in_month(), 29);
    EXPECT_EQ(date(1900, 2, 1).days_in_month(), 28);
    EXPECT_EQ(date(2026, 12, 31).year_day(), 365);
    EXPECT_EQ(date(2024, 12, 31).year_day(), 366);
}

TEST(Date_Tests, MonthsAreCutToTheMonthsEnd) {
    // Where Go carries (2026-01-31 plus a month is 2026-03-03 there)
    EXPECT_EQ(date(2026, 1, 31).add_months(1), date(2026, 2, 28));
    EXPECT_EQ(date(2024, 1, 31).add_months(1), date(2024, 2, 29));
    EXPECT_EQ(date(2026, 3, 31).add_months(-1), date(2026, 2, 28));
    EXPECT_EQ(date(2026, 3, 31).add_months(1), date(2026, 4, 30));
    EXPECT_EQ(date(2026, 1, 31).add_months(1).add_months(1), date(2026, 3, 28));   // not associative, as nowhere
    EXPECT_EQ(date(2026, 1, 31).add_months(2), date(2026, 3, 31));
    EXPECT_EQ(date(2026, 12, 31).add_months(2), date(2027, 2, 28));
    EXPECT_EQ(date(2026, 1, 15).add_months(-13), date(2024, 12, 15));
    EXPECT_EQ(date(2024, 2, 29).add_years(1), date(2025, 2, 28));
    EXPECT_EQ(date(2024, 2, 29).add_years(4), date(2028, 2, 29));
    EXPECT_EQ(date(2024, 2, 29).add_years(-4), date(2020, 2, 29));
    EXPECT_EQ(date(2000, 2, 29).add_years(100), date(2100, 2, 28));
    EXPECT_EQ(date(1, 1, 31).add_months(-1), date(0, 12, 31));
    EXPECT_EQ(date(0, 3, 31).add_months(-13), date(-1, 2, 28));
}

TEST(Date_Tests, IsValid) {
    EXPECT_TRUE(date::is_valid(2026, 9, 24));
    EXPECT_TRUE(date::is_valid(2024, 2, 29));
    EXPECT_FALSE(date::is_valid(2026, 2, 29));
    EXPECT_FALSE(date::is_valid(2026, 2, 30));
    EXPECT_FALSE(date::is_valid(2026, 13, 1));
    EXPECT_FALSE(date::is_valid(2026, 0, 1));
    EXPECT_FALSE(date::is_valid(2026, 1, 0));
    EXPECT_FALSE(date::is_valid(2026, 4, 31));
    EXPECT_FALSE(date::is_valid(2026, 1, -1));
    EXPECT_TRUE(date::is_valid(-32767, 1, 1));
    EXPECT_TRUE(date::is_valid(32767, 12, 31));
    EXPECT_FALSE(date::is_valid(-32768, 12, 31));
    EXPECT_FALSE(date::is_valid(32768, 1, 1));
    EXPECT_FALSE(date::is_valid(INT_MIN, 1, 1));
}

TEST(Date_Tests, ParseTheFormsOfISO8601) {
    date d(2026, 9, 24);
    EXPECT_EQ(date::parse("2026-09-24"), d);
    EXPECT_EQ(date::parse("20260924"), d);
    EXPECT_EQ(date::parse("2026-W39-4"), d);
    EXPECT_EQ(date::parse("2026W394"), d);
    EXPECT_EQ(date::parse("2026-267"), d);
    EXPECT_EQ(date::parse("2026267"), d);
    EXPECT_EQ(date::parse("+2026-09-24"), d);
    EXPECT_EQ(date::parse("+20260924"), d);
    EXPECT_EQ(date::parse("02026-09-24"), d);
    EXPECT_EQ(date::parse("2024-W01-1"), date(2024, 1, 1));
    EXPECT_EQ(date::parse("2025-W01-1"), date(2024, 12, 30));   // week 1 of 2025 starts in 2024
    EXPECT_EQ(date::parse("2026-W53-7"), date(2027, 1, 3));     // 2026 has 53 weeks
    EXPECT_EQ(date::parse("2020-W53-5"), date(2021, 1, 1));
    EXPECT_EQ(date::parse("2024-366"), date(2024, 12, 31));
    EXPECT_EQ(date::parse("2024-060"), date(2024, 2, 29));
    EXPECT_EQ(date::parse("-0044-03-15"), date(-44, 3, 15));
    EXPECT_EQ(date::parse("-00440315"), date(-44, 3, 15));
    EXPECT_EQ(date::parse("0000-02-29"), date(0, 2, 29));
    EXPECT_EQ(date::parse("+10000-01-01"), date(10000, 1, 1));
    EXPECT_EQ(date::parse("10000-01-01"), date(10000, 1, 1));
    EXPECT_EQ(date::parse("-32767-01-01"), date(-32767, 1, 1));
    EXPECT_EQ(date::parse("32767-12-31"), date(32767, 12, 31));
}

TEST(Date_Tests, ParseRefusesWithAnOffset) {
    struct Case {
        const char* text;
        size_t offset;
    };
    Case cases[] = {
        {"", 0},
        {"-", 1},
        {"26-09-24", 0},             // a year of two digits
        {"202-09-24", 0},
        {"202609-24", 0},
        {"2026-9-24", 5},            // a month of one digit
        {"2026-09-4", 8},
        {"2026-13-01", 5},
        {"2026-00-01", 5},
        {"2026-02-29", 8},           // not a leap year
        {"2026-04-31", 8},
        {"2026-09-24T10:00", 10},    // a time after it: not a date
        {"2026-09-24 ", 10},
        {" 2026-09-24", 0},
        {"2026-09", 7},              // the day missing
        {"2026-0924", 7},            // extended and basic mixed
        {"202609-24", 0},
        {"2026-W54-1", 6},
        {"2025-W53-1", 6},           // 2025 has 52 weeks
        {"2026-W00-1", 6},
        {"2026-W39-8", 9},
        {"2026-W39-0", 9},
        {"2026-W39", 8},             // a week without its day
        {"2026-W394", 8},
        {"2026W39-4", 7},
        {"2026-000", 5},
        {"2026-366", 5},             // not a leap year
        {"2024-367", 5},
        {"32768-01-01", 0},          // beyond the calendar
        {"-32768-01-01", 1},
        {"99999-01-01", 0},
        {"2026/09/24", 0},
        {"2026-09-2x", 8},
        {"x", 0},
        {"+-2026-09-24", 1},
        {"-0000-01-01", 0},          // ISO 8601 forbids the minus on year zero
        {"-00000101", 0},
        {"2026--09-24", 5},
        {"2026-09-24-", 10},
        {"20260924x", 8},
        {"2026-W39-4x", 10},
        {"2026-267x", 8},
    };
    for (auto& c : cases) {
        auto d = date::parse(c.text);
        ASSERT_FALSE(d.has_value()) << c.text << " read as " << d->to_string();
        EXPECT_EQ(d.error().offset(), c.offset) << c.text << ": " << d.error().message();
        EXPECT_FALSE(d.error().message().empty());
    }
    EXPECT_EQ(date::parse("2026-02-29").error().message(), "a day that the month has expected");
    EXPECT_EQ(date::parse("2026-13-01").error().message(), "a month from 01 to 12 expected");
    EXPECT_EQ(date::parse("2026-09-24T10:00").error().message(), "the end of the date expected");
}

TEST(Date_Tests, ChronoBothWays) {
    chr::year_month_day ymd = chr::year(2026) / chr::September / 24;
    date d = ymd;   // implicit, as a year_month_day passes on
    EXPECT_EQ(d, date(2026, 9, 24));
    chr::year_month_day back(d);   // back only explicitly: one way for a comparison to go
    EXPECT_EQ(back, ymd);
    EXPECT_TRUE(d == ymd && ymd == d && !(d < ymd) && d <= ymd);   // the chrono value converted to a date
    chr::sys_days sd = ymd;
    date from_days = sd;           // what a date is: days from 1970-01-01
    EXPECT_EQ(from_days, d);
    EXPECT_EQ(static_cast<chr::sys_days>(d), sd);
    EXPECT_TRUE(d == sd && sd == d && d < sd + chr::days(1));
    EXPECT_EQ(date(chr::floor<chr::days>(chr::system_clock::time_point())), date());
    EXPECT_EQ(date(chr::sys_days(chr::days(INT32_MIN))), date(-32767, 1, 1));   // saturated
    EXPECT_EQ(date(chr::sys_days(chr::days(INT32_MAX))), date(32767, 12, 31));
    static_assert(!std::is_convertible_v<date, chr::year_month_day> && !std::is_convertible_v<date, chr::sys_days>);
    static_assert(std::is_convertible_v<chr::year_month_day, date> && std::is_convertible_v<chr::sys_days, date>);
    date carried = chr::year(2026) / chr::February / 30;   // not ok() to chrono: carried here
    EXPECT_EQ(carried, date(2026, 3, 2));
    EXPECT_EQ(std::format("{}", static_cast<chr::year_month_day>(d)), "2026-09-24");

    static_assert(sizeof(date) == 4);
    static_assert(std::is_trivially_copyable_v<date>);
    static_assert(date(2026, 2, 30) == date(2026, 3, 2));
    static_assert(date(2026, 9, 24).weekday() == weekday::thursday);
    static_assert(date(2024, 12, 30).iso_week() == iso_week{2025, 1});
    static_assert(date(2026, 1, 31).add_months(1) == date(2026, 2, 28));
    static_assert(date().year() == 1970 && date().month() == time::month::january && date().day() == 1);
    static_assert(date(2026, 1, 1) < date(2026, 1, 2));
    static_assert(date::is_valid(2024, 2, 29) && !date::is_valid(2023, 2, 29));
}

TEST(Date_Tests, TheWeekdayIsNumberedAsISO) {
    EXPECT_EQ(date(2026, 9, 21).weekday(), weekday::monday);
    EXPECT_EQ(static_cast<int>(weekday::monday), 1);
    EXPECT_EQ(static_cast<int>(date(2026, 9, 27).weekday()), 7);   // Sunday: 7, not 0
    EXPECT_EQ(date(1970, 1, 1).weekday(), weekday::thursday);
}

TEST(Date_Tests, WrittenAsText) {
    EXPECT_EQ(time::to_string(weekday::monday), "Monday");
    EXPECT_EQ(time::to_string(weekday::sunday), "Sunday");
    EXPECT_EQ(to_string(date(2026, 9, 24).weekday()), "Thursday");   // found by ADL
    std::ostringstream os;
    os << date(2026, 9, 24) << " " << date(2026, 9, 27).weekday() << " " << date(-44, 3, 15);
    EXPECT_EQ(os.str(), "2026-09-24 Sunday -0044-03-15");
}

TEST(Date_Tests, TheErrorType) {
    static_assert(!std::is_convertible_v<const char*, time::error> && !std::is_convertible_v<string, time::error>);
    time::error e("a month from 01 to 12 expected", 5);
    EXPECT_EQ(e.message(), "a month from 01 to 12 expected");
    EXPECT_EQ(e.offset(), 5u);
    time::error f("no offset");
    EXPECT_EQ(f.offset(), 0u);
}

TEST(Date_Tests, RandomTextIsReadOrRefusedWithAnOffset) {
    // Texts drawn from the characters of the three forms: each is a date
    // that reads back from its own text, or a refusal with a sentence
    // and an offset inside the text
    std::mt19937 rng(20260924);
    const char alphabet[] = "0123456789012345678901234567890123456789--W+x";
    size_t accepted = 0;
    for (int k = 0; k < 200000; ++k) {
        std::string text;
        size_t length = rng() % 13;
        for (size_t i = 0; i < length; ++i) {
            text += alphabet[rng() % (sizeof(alphabet) - 1)];
        }
        auto d = date::parse(string(text));
        if (d) {
            ++accepted;
            ASSERT_EQ(date::parse(d->to_string()), *d) << text;
            ASSERT_TRUE(date::is_valid(d->year(), d->month(), d->day())) << text;
        } else {
            ASSERT_LE(d.error().offset(), text.size()) << text;
            ASSERT_FALSE(d.error().message().empty()) << text;
        }
    }
    EXPECT_GT(accepted, 100u);
}

// The calendar of the fields, Neri and Schneider's, against <chrono> for
// every day of the type's range
TEST(Date_Tests, EveryDayOfTheRangeAsChronoHasIt) {
    auto lo = std::chrono::sys_days(std::chrono::year(-32767) / 1 / 1).time_since_epoch().count();
    auto hi = std::chrono::sys_days(std::chrono::year(32767) / 12 / 31).time_since_epoch().count();
    long wrong = 0;
    for (long d = lo; d <= hi; ++d) {
        std::chrono::year_month_day ref{std::chrono::sys_days(std::chrono::days(d))};
        auto c = sgcl::time::detail::civil_from_days(int32_t(d));
        wrong += c.year != int(ref.year()) || c.month != unsigned(ref.month()) || c.day != unsigned(ref.day());
    }
    EXPECT_EQ(wrong, 0);
}

// A month is a name for its number, as Go's time.Month
TEST(Date_Tests, TheMonthIsANamedNumber) {
    date d(2026, time::month::september, 25);
    EXPECT_EQ(d, date(2026, 9, 25));
    EXPECT_EQ(d.month(), time::month::september);
    EXPECT_EQ(int(d.month()), 9);
    EXPECT_EQ(time::to_string(d.month()), "September");
    EXPECT_EQ(time::to_string(time::month(13)), "%!Month(13)");
    EXPECT_TRUE(date::is_valid(2024, time::month::february, 29));
    EXPECT_FALSE(date::is_valid(2026, time::month::february, 29));
}

// The day arithmetic in operators, as datetime has it
TEST(Date_Tests, DaysInOperators) {
    date d(2026, 2, 27);
    EXPECT_EQ(d + 2, date(2026, 3, 1));
    EXPECT_EQ(2 + d, date(2026, 3, 1));
    EXPECT_EQ(d - 27, date(2026, 1, 31));
    EXPECT_EQ(date(2026, 3, 1) - d, 2);
    EXPECT_EQ(d - date(2026, 3, 1), -2);
    d += 3;
    EXPECT_EQ(d, date(2026, 3, 2));
    d -= 1;
    EXPECT_EQ(d, date(2026, 3, 1));
    static_assert(date(2026, 1, 1) + 364 == date(2026, 12, 31));
}

// A month is formatted by its name, as a weekday is
TEST(Date_Tests, AMonthFormatsByItsName) {
    date d(2026, 9, 26);
    EXPECT_EQ(txt::format("{}", d.month()), "September");
    EXPECT_EQ(txt::format("{:%b}", d.month()), "Sep");
    EXPECT_EQ(txt::format("{:%m}", d.month()), "09");
    EXPECT_EQ(txt::format("{:>11}", d.month()), "  September");
    EXPECT_EQ(txt::format("{}", d.weekday()), "Saturday");
}
