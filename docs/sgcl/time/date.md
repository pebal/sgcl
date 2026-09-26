# sgcl::time::date, sgcl::time::month, sgcl::time::weekday, sgcl::time::iso_week

```cpp
#include "sgcl/time/date.h"   // or "sgcl/time/time.h"

namespace sgcl::time {
    enum class month : uint8_t { january = 1, february, march, april, may, june, july, august, september, october, november, december };
    enum class weekday : uint8_t { monday = 1, tuesday, wednesday, thursday, friday, saturday, sunday };
    struct iso_week { int year; int week; };      // the year the week belongs to, 1 to 53
    class date;                                   // a date of the Gregorian calendar, -32767 to 32767
}
```

A date with no time of day and no time zone: a birthday, a holiday, the day an invoice is due, the day a log was rotated. What Go has no type for (a `time.Time` at midnight in UTC stands in for one there, and every question asked of it has to name a zone), and what Java's `LocalDate` and C++'s `year_month_day` are. The calendar is the Gregorian one, extended backwards before 1582 as ISO 8601 extends it, over the years -32767 to 32767 of `<chrono>`, whose calendar computes the fields: a date is the number of days from 1970-01-01 in 32 bits, four bytes, so that a comparison, a difference and a step of days are one instruction each, and a field is asked of the calendar of the standard, not of a second one written here.

A date built from numbers never fails. A day or a month outside its range carries into the next, as Go's `time.Date` carries: `date(2026, 2, 30)` is 2026-03-02, `date(2026, 13, 1)` is 2027-01-01, `date(2026, 3, 0)` is the last day of February, and `date(2026, 1, 267)` is the 267th day of 2026 — which lets a program count in days or months without a loop. `date::is_valid(y, m, d)` tells a date that needs no carrying, for numbers that came from a person; `date::parse` refuses what does not exist. A result past either end of the calendar, by carrying or by arithmetic, is the end: the arithmetic saturates, as [`duration`](../core/duration.md)'s does.

`add_months` keeps the day of the month and cuts it to the month's last where the month is shorter: 2026-01-31 plus one month is 2026-02-28 and plus two is 2026-03-31. That is what "a month from now" means to a person and to Java, .NET and PostgreSQL; Go's `AddDate` carries into March instead (2026-03-03). `add_years` is twelve months: 2024-02-29 plus a year is 2025-02-28. The two are not associative (plus one month and plus one month again is 03-28, plus two months at once is 03-31), and nothing that cuts to the month's end can be.

The day of the week is numbered as ISO 8601 numbers it, Monday 1 to Sunday 7, not C's and Go's Sunday 0. The ISO week starts on Monday, and week 1 is the one with the year's first Thursday, so the few days around New Year may belong to a week of the year next to theirs: `date(2024, 12, 30).iso_week()` is week 1 of 2025, and 2026 has 53 weeks.

The text is ISO 8601's. `to_string()` writes the extended calendar date as `std::format`'s `%F` does (`"2026-09-24"`, `"-0044-03-15"`, `"10000-01-01"`), and `parse` reads the three forms of a date, each extended or basic: the calendar date (`"2026-09-24"`, `"20260924"`), the week date (`"2026-W39-4"`, `"2026W394"`) and the ordinal date (`"2026-267"`, `"2026267"`), with a year of four digits, or five in the extended forms, and a sign or none (`"-0044-03-15"`, `"+10000-01-01"`). The date must exist and be the whole text: `"2026-02-29"` and `"2026-09-24T10:00"` are refused, each with a sentence and the byte at which it went wrong. Other shapes are read and written by a pattern of `%` as `std::format` and `std::chrono::parse` have them for `<chrono>` (`date::parse("24.09.2026", "%d.%m.%Y")`, `d.format("%A, %d %B %Y")`; the [text](layout.md) page says what each specifier does), and `txt::format("{:%d.%m}", d)` writes one in a field.

A date becomes an instant at a time of the clock in a zone: `d.at(9, 30, zone)`, the [datetime](datetime.md) page says how a time of the clock that a change of the clock skipped or showed twice is read; `d.start_of_day(zone)` is the first instant of the date there.

## Rules

- A date is a plain value, trivially copyable, `constexpr` but for its text: it lives anywhere.
- `year()` is the year of the calendar, where 1 BC is year 0 and 2 BC is -1, as in ISO 8601 and `<chrono>`; `date(0, 1, 1).is_leap_year()` is true.
- `iso_week().year` may be the year before or after `year()`; for the first two days of the calendar it is -32768, which is outside it, so those two have no week date that `parse` reads.
- `days_until(other)` is `other` minus this, in days: negative for a date in the past.
- A date is made implicitly from a `std::chrono::year_month_day` and from a `std::chrono::sys_days` (which is what a date holds: days from 1970-01-01), so code written with the standard's calendar passes one on as it is and a date compares with either (`d == 2026y/9/24`); back to them only explicitly (`year_month_day(d)`, `static_cast<sys_days>(d)`), so that such a comparison has one way to go. A `year_month_day` that is not `ok()` because its day is beyond its month is carried like the numbers; a `sys_days` beyond the calendar is its end.
- `month()` is a `time::month`, as Go's `time.Month`: a name for the number, `int(d.month())` the number 1 to 12; a date is made with either (`date(2026, 9, 25)`, `date(2026, time::month::september, 25)`).
- `to_string(month)` and `to_string(weekday)` are the English names as Go writes them (`"September"`, `"Monday"`); `operator<<` writes a date's `to_string()`, a month's and a weekday's name.
- `parse` refuses `-0000`, which ISO 8601 forbids; year zero is `0000`.

## Members

```cpp
constexpr date() noexcept;                                  // 1970-01-01
constexpr date(int year, int month, int day) noexcept;     // carried, saturated at the ends
constexpr date(int year, time::month month, int day) noexcept;
constexpr date(std::chrono::year_month_day ymd) noexcept;  // implicit, carried
constexpr date(std::chrono::sys_days days) noexcept;       // implicit, saturated
constexpr explicit operator std::chrono::year_month_day() const noexcept;
constexpr explicit operator std::chrono::sys_days() const noexcept;
static constexpr bool is_valid(int year, int month, int day) noexcept;
static expected<date, error> parse(const string& text);    // ISO 8601: calendar, week and ordinal dates
static expected<date, error> parse(const string& text, const string& pattern);   // "%d.%m.%Y" (layout.md)

constexpr int year() const noexcept;
constexpr time::month month() const noexcept;              // january to december; int(...) 1 to 12
constexpr int day() const noexcept;                        // 1 to 31
constexpr time::weekday weekday() const noexcept;          // Monday 1 to Sunday 7
constexpr int year_day() const noexcept;                   // 1 to 366
constexpr time::iso_week iso_week() const noexcept;
constexpr int days_in_month() const noexcept;              // 28 to 31
constexpr bool is_leap_year() const noexcept;

constexpr date add_days(int n) const noexcept;
constexpr date add_months(int n) const noexcept;           // the day cut to the month's end
constexpr date add_years(int n) const noexcept;            // twelve months
constexpr int days_until(date other) const noexcept;       // other - this
date operator+(date d, int n), operator+(int n, date d), operator-(date d, int n);   // add_days
int operator-(date a, date b);                             // the days from b to a
date& operator+=(int n), operator-=(int n);

datetime at(int hour, int minute, const zone& z) const;           // and with seconds, and with earlier or later (datetime.md)
optional<datetime> try_at(int hour, int minute, int second, const zone& z) const;
datetime start_of_day(const zone& z) const;

string format(const string& pattern) const;                // "%A, %d %B %Y"
string to_string() const;                                  // "2026-09-24", as %F
// == and <=>; operator<< writes to_string()

string to_string(weekday d);                               // a free function: "Monday" ... "Sunday"
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include "sgcl/time/time.h"
#include <iostream>

using namespace sgcl;

int main() {
    time::date d(2026, 9, 24);
    auto [year, week] = d.iso_week();
    std::cout << d.to_string() << " is day " << d.year_day() << ", weekday " << static_cast<int>(d.weekday())
              << ", week " << week << " of " << year << "\n";          // 2026-09-24 is day 267, weekday 4, week 39 of 2026

    std::cout << time::date(2026, 2, 30).to_string() << " "            // carried: 2026-03-02
              << time::date::is_valid(2026, 2, 30) << "\n";            // 0

    auto invoice = time::date(2026, 1, 31);
    for (int i : range(1, 4)) {
        std::cout << invoice.add_months(i).to_string() << " ";          // the month's end when it is shorter
    }
    std::cout << "\n";
    std::cout << time::date(2024, 2, 29).add_years(1).to_string() << " "
              << time::date(2024, 12, 30).iso_week().year << "\n";      // 2025-02-28 2025

    auto christmas = time::date(2026, 12, 25);
    std::cout << d.days_until(christmas) << " days to " << christmas.to_string() << "\n";

    for (auto text : {"2026-W39-4", "2026-267", "20260924", "2026-02-29"}) {
        auto parsed = time::date::parse(text);
        if (parsed) {
            std::cout << text << " -> " << parsed->to_string() << "\n";
        } else {
            std::cout << text << ": " << parsed.error().message() << " (byte " << parsed.error().offset() << ")\n";
        }
    }
    return 0;
}
```

Output:

```text
2026-09-24 is day 267, weekday 4, week 39 of 2026
2026-03-02 0
2026-02-28 2026-03-31 2026-04-30 
2025-02-28 2025
92 days to 2026-12-25
2026-W39-4 -> 2026-09-24
2026-267 -> 2026-09-24
20260924 -> 2026-09-24
2026-02-29: a day that the month has expected (byte 8)
```

## See also

- [time](README.md): the module, its errors and the stopwatch
- [datetime](datetime.md): a date at a time of the clock in a zone
- [text](layout.md): patterns of `%`, read and written
- [duration](../core/duration.md): a span of time; a day of the calendar is not one (23, 24 or 25 hours where the clock changes), which is why a date steps by `add_days`
