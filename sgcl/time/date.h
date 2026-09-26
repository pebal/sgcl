//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/string.h"
#include "error.h"

#include <algorithm>
#include <chrono>
#include <compare>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace sgcl::time {
    class datetime;
    class layout;
    class zone;

    namespace detail {
        struct layout_writer;
    }

    // How a time of the clock that a change of the clock makes happen
    // twice (the hour repeated in autumn) is read: the earlier of the two
    // instants, or the later; datetime.h says what each does with a time
    // the change skipped
    inline constexpr struct earlier_t {
        explicit earlier_t() = default;
    } earlier{};

    inline constexpr struct later_t {
        explicit later_t() = default;
    } later{};

    // A month of the year, January 1 to December 12, as Go's time.Month:
    // a name for the number, int(m) for the number itself
    enum class month : uint8_t {
        january = 1,
        february,
        march,
        april,
        may,
        june,
        july,
        august,
        september,
        october,
        november,
        december,
    };

    // The month's English name, as Go's Month.String() writes it: "January"
    inline string to_string(month m) {
        static constexpr const char* Names[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
        unsigned i = static_cast<unsigned>(m) - 1;
        return i < 12 ? string(Names[i]) : string("%!Month(" + std::to_string(static_cast<unsigned>(m)) + ")");
    }

    template<class CharT, class Traits>
    std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, month m) {
        return os << to_string(m);
    }

    // A day of the week, numbered as ISO 8601 numbers it: Monday 1 to
    // Sunday 7 (not C's and Go's Sunday 0)
    enum class weekday : uint8_t {
        monday = 1,
        tuesday,
        wednesday,
        thursday,
        friday,
        saturday,
        sunday,
    };

    // The day's English name, as Go's Weekday.String() writes it: "Monday"
    inline string to_string(weekday d) {
        static constexpr const char* Names[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
        unsigned i = static_cast<unsigned>(d) - 1;
        return i < 7 ? string(Names[i]) : string("%!Weekday(" + std::to_string(static_cast<unsigned>(d)) + ")");
    }

    template<class CharT, class Traits>
    std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, weekday d) {
        return os << to_string(d);
    }

    // A week of ISO 8601: the weeks start on Monday, and week 1 is the
    // one with the year's first Thursday, so the few days around New
    // Year may belong to a week of the year next to theirs: 2024-12-30
    // is in week 1 of 2025
    struct iso_week {
        int year = 0;   // the year the week belongs to
        int week = 0;   // 1 to 52 or 53

        friend constexpr bool operator==(const iso_week&, const iso_week&) noexcept = default;
    };

    namespace detail {
        // Floor division and the remainder that goes with it, for the
        // arithmetic of years and months that crosses zero
        constexpr int64_t floor_div(int64_t a, int64_t b) noexcept {
            int64_t q = a / b;
            return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
        }

        // The days from 1970-01-01 to the first of a month of any year:
        // the year brought into [0, 400) by whole cycles of 146097 days
        // (the Gregorian calendar repeats every 400 years), the calendar
        // of <chrono> asked there
        constexpr int64_t days_from_civil(int64_t year, unsigned month) noexcept {
            int64_t cycles = floor_div(year, 400);
            auto first = std::chrono::year(static_cast<int>(year - cycles * 400)) / std::chrono::month(month) / std::chrono::day(1);
            return std::chrono::sys_days(first).time_since_epoch().count() + cycles * 146097;
        }

        // The year, month and day of a count of days since 1970-01-01, by
        // the Euclidean affine functions of Neri and Schneider ("Euclidean
        // affine functions and their application to calendar algorithms",
        // 2021): the days shifted by whole 400-year cycles so that every
        // date of the type's range is positive, the calendar counted from
        // March (the leap day last), then a century, a year and a month
        // each by one multiplication and a shift; no division by anything
        // but a power of two or a constant the compiler turns into one.
        // Every day of the range is checked against <chrono> in the tests.
        struct Civil {
            int32_t year;
            uint32_t month;   // 1 to 12
            uint32_t day;     // 1 to 31
        };

        constexpr Civil civil_from_days(int32_t days) noexcept {
            constexpr uint32_t shift = 82;                          // 400-year cycles added: year -32 800 at the least
            constexpr uint32_t offset = 719468 + 146097 * shift;    // 1970-01-01 is day 719468 of the March calendar from year 0
            const uint32_t n = uint32_t(int64_t(days) + offset);
            const uint32_t n1 = 4 * n + 3;
            const uint32_t century = n1 / 146097;
            const uint32_t day_of_century = n1 % 146097 / 4;
            const uint32_t n2 = 4 * day_of_century + 3;
            const uint64_t p2 = uint64_t(2939745) * n2;
            const uint32_t year_of_century = uint32_t(p2 >> 32);
            const uint32_t day_of_year = uint32_t(p2 & 0xFFFFFFFFu) / 2939745 / 4;
            const uint32_t n3 = 2141 * day_of_year + 197913;
            const uint32_t month = n3 >> 16;
            const uint32_t day = (n3 & 0xFFFFu) / 2141;
            const uint32_t january_or_later = day_of_year >= 306;
            return Civil{
                int32_t(100 * century + year_of_century) - int32_t(400 * shift) + int32_t(january_or_later),
                january_or_later ? month - 12 : month,
                day + 1
            };
        }

        // The weeks of an ISO year: 53 when it starts on a Thursday, or
        // on a Wednesday in a leap year, 52 otherwise; p(y) is the day of
        // the week of the year's last day (0 for Sunday)
        constexpr int iso_weeks_in(int64_t year) noexcept {
            auto p = [](int64_t y) {
                int64_t v = y + floor_div(y, 4) - floor_div(y, 100) + floor_div(y, 400);
                return v - floor_div(v, 7) * 7;
            };
            return (p(year) == 4 || p(year - 1) == 3) ? 53 : 52;
        }
    }

    // A date of the Gregorian calendar (proleptic: the same rules before
    // 1582), with no time of day and no time zone: a birthday, a holiday,
    // the day a payment is due. Years -32767 to 32767, as the calendar of
    // <chrono>, which the fields are computed with; the value is the
    // number of days from 1970-01-01 in 32 bits, so a comparison and a
    // difference are one instruction.
    //
    // Nothing here fails. A day or a month out of its range carries into
    // the next, as Go's time.Date does: date(2026, 2, 30) is 2026-03-02,
    // date(2026, 13, 1) is 2027-01-01, date(2026, 3, 0) the last of
    // February; is_valid tells a date that needs no carrying, for data
    // from a person. A result beyond either end of the range is the end.
    class date {
    public:
        // 1970-01-01
        constexpr date() noexcept = default;

        // Carried as above; the time of day, where there is one, is a
        // datetime's (at() below)
        constexpr date(int year, int month, int day) noexcept
        : _days(_clamped(_first_of(int64_t(year), int64_t(month)) + int64_t(day) - 1)) {
        }

        constexpr date(int year, time::month month, int day) noexcept
        : date(year, int(month), day) {
        }

        // From the calendar of <chrono> implicitly, so that code written
        // with year_month_day or sys_days (which is what a date is: days
        // from 1970-01-01) passes one on as it is, and a date compares with
        // either; back to them only explicitly, so that a comparison of a
        // date with one of them has a single way to go. A day beyond its
        // month is carried as above, a point beyond the calendar saturated
        constexpr date(std::chrono::year_month_day ymd) noexcept
        : date(static_cast<int>(ymd.year()), static_cast<int>(static_cast<unsigned>(ymd.month())), static_cast<int>(static_cast<unsigned>(ymd.day()))) {
        }

        constexpr date(std::chrono::sys_days days) noexcept
        : _days(_clamped(int64_t(days.time_since_epoch().count()))) {
        }

        constexpr explicit operator std::chrono::year_month_day() const noexcept {
            return _fields();
        }

        constexpr explicit operator std::chrono::sys_days() const noexcept {
            return _sys();
        }

        // Whether the date exists as written: a year in -32767..32767, a
        // month 1..12, a day within the month
        static constexpr bool is_valid(int year, int month, int day) noexcept {
            if (year < MinYear || year > MaxYear || month < 1 || month > 12 || day < 1) {
                return false;
            }
            auto last = std::chrono::year(year) / std::chrono::month(static_cast<unsigned>(month)) / std::chrono::last;
            return static_cast<unsigned>(day) <= static_cast<unsigned>(last.day());
        }

        static constexpr bool is_valid(int year, time::month month, int day) noexcept {
            return is_valid(year, int(month), day);
        }

        // ISO 8601, the three forms of a date, each extended or basic:
        // the calendar date "2026-09-24" or "20260924", the week date
        // "2026-W39-4" or "2026W394", the ordinal date "2026-267" or
        // "2026267". A year of four digits, or of five with the extended
        // forms, with a sign or none ("-0044-03-15", "+10000-01-01"). The
        // date must exist (not "2026-02-30") and be the whole text.
        static expected<date, error> parse(const string& text);

        // A date in a pattern of std::format's specifiers for <chrono>,
        // read as std::chrono::parse reads one (layout.h): "%d.%m.%Y",
        // "%B %e, %Y", "%G-W%V-%u". The date must exist and the pattern
        // take the whole text; a specifier of a time of day or of a zone
        // is refused, a date having neither
        static expected<date, error> parse(const string& text, const string& pattern);

        constexpr int year() const noexcept {
            return detail::civil_from_days(_days).year;
        }

        // January to December (int(d.month()) is 1 to 12)
        constexpr time::month month() const noexcept {
            return time::month(detail::civil_from_days(_days).month);
        }

        // 1 to 31
        constexpr int day() const noexcept {
            return int(detail::civil_from_days(_days).day);
        }

        constexpr time::weekday weekday() const noexcept {
            return static_cast<time::weekday>(std::chrono::weekday(_sys()).iso_encoding());
        }

        // 1 to 366
        constexpr int year_day() const noexcept {
            return static_cast<int>(_days - detail::days_from_civil(year(), 1)) + 1;
        }

        constexpr time::iso_week iso_week() const noexcept {
            int y = year();
            int week = (year_day() - static_cast<int>(weekday()) + 10) / 7;
            if (week < 1) {
                return {y - 1, detail::iso_weeks_in(y - 1)};
            }
            if (week > detail::iso_weeks_in(y)) {
                return {y + 1, 1};
            }
            return {y, week};
        }

        // 28 to 31
        constexpr int days_in_month() const noexcept {
            auto f = _fields();
            return static_cast<int>(static_cast<unsigned>((f.year() / f.month() / std::chrono::last).day()));
        }

        constexpr bool is_leap_year() const noexcept {
            return _fields().year().is_leap();
        }

        // n days later (earlier for a negative n)
        constexpr date add_days(int n) const noexcept {
            return _of(_clamped(int64_t(_days) + n));
        }

        // The same day n months later, or the month's last day when it is
        // shorter: 2026-01-31 plus one month is 2026-02-28, as "a month
        // from now" is read by Java, .NET, PostgreSQL and a person (Go
        // carries into March instead)
        constexpr date add_months(int n) const noexcept {
            return _months_later(int64_t(n));
        }

        // The same, by years: 2024-02-29 plus one year is 2025-02-28
        constexpr date add_years(int n) const noexcept {
            return _months_later(int64_t(n) * 12);
        }

        // other minus this, in days
        constexpr int days_until(date other) const noexcept {
            return other._days - _days;
        }

        // This date at that time of the clock in a zone. A time the zone's
        // clock skipped (the hour lost to a change in spring) is moved on
        // by the length of the skip — 02:30 on the night Warsaw goes from
        // 02:00 to 03:00 is 03:30 — and a time it showed twice (the hour
        // repeated in autumn) is the first of the two: the rule Java,
        // JavaScript's Temporal and iCalendar (RFC 5545) call compatible.
        // Hours, minutes and seconds out of their ranges carry, as the
        // date's own fields do: at(24, 0, z) is midnight of the next day
        datetime at(int hour, int minute, const zone& z) const;
        datetime at(int hour, int minute, int second, const zone& z) const;

        // The same, the choice made otherwise: earlier takes the first of
        // a time shown twice and, for a time skipped, the instant of the
        // change (03:00 in the example above: the first time of the clock
        // after the skip); later takes the second of a time shown twice
        // and moves a skipped one on as above
        datetime at(int hour, int minute, const zone& z, earlier_t) const;
        datetime at(int hour, int minute, int second, const zone& z, earlier_t) const;
        datetime at(int hour, int minute, const zone& z, later_t) const;
        datetime at(int hour, int minute, int second, const zone& z, later_t) const;

        // The same when the time is there exactly once, nothing when it
        // was skipped or shown twice
        optional<datetime> try_at(int hour, int minute, int second, const zone& z) const;

        // The first instant of this date in a zone: midnight, or where
        // midnight was skipped (America/Santiago, Asia/Beirut in some
        // years) the change that skipped it
        datetime start_of_day(const zone& z) const;

        // The date written by a pattern of std::format's specifiers for
        // <chrono> (layout.h): "%A, %d %B %Y" is "Thursday, 24 September
        // 2026"; a specifier of a time of day or of a zone is written as
        // it stands
        string format(const string& pattern) const;

        // ISO 8601's extended calendar date, as std::format's %F writes
        // it: "2026-09-24", "-0044-03-15", "10000-01-01"
        string to_string() const;

        // Written as to_string() writes it
        template<class CharT, class Traits>
        friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, date d) {
            return os << d.to_string();
        }

        // The day arithmetic in operators, as datetime has it: d + n and
        // d - n are add_days, a - b the days from b to a (b.days_until(a))
        friend constexpr date operator+(date d, int n) noexcept {
            return d.add_days(n);
        }

        friend constexpr date operator+(int n, date d) noexcept {
            return d.add_days(n);
        }

        friend constexpr date operator-(date d, int n) noexcept {
            return d.add_days(-n);
        }

        friend constexpr int operator-(date a, date b) noexcept {
            return b.days_until(a);
        }

        constexpr date& operator+=(int n) noexcept {
            return *this = add_days(n);
        }

        constexpr date& operator-=(int n) noexcept {
            return *this = add_days(-n);
        }

        friend constexpr bool operator==(const date&, const date&) noexcept = default;
        friend constexpr std::strong_ordering operator<=>(const date&, const date&) noexcept = default;

    private:
        static constexpr int MinYear = -32767;
        static constexpr int MaxYear = 32767;
        static constexpr int64_t MinDays = -12687428;   // -32767-01-01
        static constexpr int64_t MaxDays = 11248737;    // 32767-12-31

        static constexpr date _of(int32_t days) noexcept {
            date d;
            d._days = days;
            return d;
        }

        static constexpr int32_t _clamped(int64_t days) noexcept {
            return static_cast<int32_t>(std::clamp(days, MinDays, MaxDays));
        }

        // The days to the first of a month, the month carried into the
        // year: exact for every year an int and the carry make (some
        // 800 billion days at most, well inside 64 bits), so that a day
        // carried back into the range lands where it should
        static constexpr int64_t _first_of(int64_t year, int64_t month) noexcept {
            int64_t m = month - 1;
            int64_t y = year + detail::floor_div(m, 12);
            m -= detail::floor_div(m, 12) * 12;
            return detail::days_from_civil(y, static_cast<unsigned>(m + 1));
        }

        constexpr date _months_later(int64_t n) const noexcept {
            auto f = _fields();
            int64_t total = int64_t(static_cast<int>(f.year())) * 12 + (static_cast<unsigned>(f.month()) - 1) + n;
            int64_t y = detail::floor_div(total, 12);
            unsigned m = static_cast<unsigned>(total - y * 12) + 1;
            if (y < MinYear) {
                return _of(MinDays);
            }
            if (y > MaxYear) {
                return _of(MaxDays);
            }
            auto last = std::chrono::year(static_cast<int>(y)) / std::chrono::month(m) / std::chrono::last;
            unsigned d = std::min(static_cast<unsigned>(f.day()), static_cast<unsigned>(last.day()));
            return _of(static_cast<int32_t>(detail::days_from_civil(y, m) + d - 1));
        }

        // The length of the run of digits at i
        static size_t _digits_at(const char* s, size_t n, size_t i) noexcept {
            size_t j = i;
            while (j < n && s[j] >= '0' && s[j] <= '9') {
                ++j;
            }
            return j - i;
        }

        constexpr std::chrono::sys_days _sys() const noexcept {
            return std::chrono::sys_days(std::chrono::days(_days));
        }

        constexpr std::chrono::year_month_day _fields() const noexcept {
            auto c = detail::civil_from_days(_days);
            return std::chrono::year_month_day(std::chrono::year(c.year), std::chrono::month(c.month), std::chrono::day(c.day));
        }

        int32_t _days = 0;
    };

    inline string date::to_string() const {
        auto f = _fields();
        int y = static_cast<int>(f.year());
        unsigned m = static_cast<unsigned>(f.month());
        unsigned d = static_cast<unsigned>(f.day());
        char text[16];   // "-32767-12-31", 12 bytes
        char* out = text;
        if (y < 0) {
            *out++ = '-';
            y = -y;
        }
        char digits[8];
        int n = 0;
        do {
            digits[n++] = static_cast<char>('0' + y % 10);
            y /= 10;
        } while (y != 0);
        for (int pad = n; pad < 4; ++pad) {
            *out++ = '0';
        }
        while (n != 0) {
            *out++ = digits[--n];
        }
        *out++ = '-';
        *out++ = static_cast<char>('0' + m / 10);
        *out++ = static_cast<char>('0' + m % 10);
        *out++ = '-';
        *out++ = static_cast<char>('0' + d / 10);
        *out++ = static_cast<char>('0' + d % 10);
        return string(text, static_cast<size_t>(out - text));
    }

    inline expected<date, error> date::parse(const string& text) {
        const char* s = text.data();
        size_t n = text.size();
        size_t i = 0;
        auto is_digit = [&](size_t at) { return at < n && s[at] >= '0' && s[at] <= '9'; };
        auto fail = [](const char* message, size_t at) { return expected<date, error>(unexpect, error(message, at)); };
        // The number of `count` digits at `at`, or -1 when they are not all digits
        auto number = [&](size_t at, size_t count) {
            int v = 0;
            for (size_t k = 0; k < count; ++k) {
                if (!is_digit(at + k)) {
                    return -1;
                }
                v = v * 10 + (s[at + k] - '0');
            }
            return v;
        };

        // The year: a sign or none, then the digits up to the first
        // character that is not one
        bool negative = false;
        if (i < n && (s[i] == '+' || s[i] == '-')) {
            negative = s[i] == '-';
            ++i;
        }
        size_t year_at = i;
        size_t run = year_at;
        while (is_digit(run)) {
            ++run;
        }
        size_t digits = run - year_at;
        bool extended = run < n && s[run] == '-';
        bool basic_week = run == year_at + 4 && run < n && s[run] == 'W';
        size_t year_digits = 4;
        if (extended) {
            if (digits != 4 && digits != 5) {
                return fail("a year of four or five digits expected", year_at);
            }
            year_digits = digits;
        } else if (!basic_week && digits != 8 && digits != 7) {
            return fail(digits < 4 ? "a year of four digits expected" : "a date expected: 2026-09-24, 2026-W39-4 or 2026-267", year_at);
        }
        int year = number(year_at, year_digits);
        if (negative && year == 0) {
            return fail("a year of zero has no minus sign", year_at - 1);   // ISO 8601 forbids -0000
        }
        if (negative) {
            year = -year;
        }
        if (year < MinYear || year > MaxYear) {
            return fail("a year from -32767 to 32767 expected", year_at);
        }
        i = year_at + year_digits;
        if (extended) {
            ++i;   // the hyphen
        }

        int64_t days = 0;
        if (i < n && s[i] == 'W') {
            // The week date: the Monday of week 1 is the Monday on or
            // before the 4th of January, which is always in week 1
            size_t week_at = ++i;
            int week = number(week_at, 2);
            if (week < 1 || week > detail::iso_weeks_in(year)) {
                return fail("a week from 01 to 52, or 53 in a long year, expected", week_at);
            }
            i += 2;
            if (extended) {
                if (i >= n || s[i] != '-') {
                    return fail("a hyphen and a day of the week expected", i);
                }
                ++i;
            }
            size_t weekday_at = i;
            int wd = number(weekday_at, 1);
            if (wd < 1 || wd > 7) {
                return fail("a day of the week from 1 to 7 expected", weekday_at);
            }
            i += 1;
            int64_t jan4 = detail::days_from_civil(year, 1) + 3;
            int64_t jan4_weekday = static_cast<int64_t>(std::chrono::weekday(std::chrono::sys_days(std::chrono::days(jan4))).iso_encoding());
            days = jan4 - (jan4_weekday - 1) + int64_t(week - 1) * 7 + (wd - 1);
            if (days < MinDays || days > MaxDays) {
                return fail("a date from -32767-01-01 to 32767-12-31 expected", year_at);
            }
        } else if (_digits_at(s, n, i) == 3) {
            // The ordinal date: the day of the year, three digits where a
            // calendar date has two and two
            size_t ordinal_at = i;
            int ordinal = number(ordinal_at, 3);
            int length = std::chrono::year(year).is_leap() ? 366 : 365;
            if (ordinal < 1 || ordinal > length) {
                return fail(length == 366 ? "a day of the year from 001 to 366 expected" : "a day of the year from 001 to 365 expected", ordinal_at);
            }
            i += 3;
            days = detail::days_from_civil(year, 1) + ordinal - 1;
        } else {
            // The calendar date
            size_t month_at = i;
            int month = number(month_at, 2);
            if (month < 1 || month > 12) {
                return fail("a month from 01 to 12 expected", month_at);
            }
            i += 2;
            if (extended) {
                if (i >= n || s[i] != '-') {
                    return fail("a hyphen and a day of the month expected", i);
                }
                ++i;
            }
            size_t day_at = i;
            int day = number(day_at, 2);
            if (day < 1 || !is_valid(year, month, day)) {
                return fail("a day that the month has expected", day_at);
            }
            i += 2;
            days = detail::days_from_civil(year, static_cast<unsigned>(month)) + day - 1;
        }
        if (i != n) {
            return fail("the end of the date expected", i);
        }
        return _of(static_cast<int32_t>(days));
    }
}
