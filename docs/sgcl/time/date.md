[sgcl](../README.md) › [time](README.md)

# sgcl::time::date

```cpp
#include "sgcl/time/date.h"   // or "sgcl/time.h"

namespace sgcl::time {
    class date;
}
```

`sgcl::time::date` is a date with no time of day and no time zone: a birthday, a holiday, the day an invoice is
due, the day a log was rotated. Go has no type for it (a `time.Time` at midnight in UTC stands in for one there,
and every question asked of it has to name a zone); it is what Java's `LocalDate` and C++'s `year_month_day` are.
The calendar is the Gregorian one, extended backwards before 1582 as ISO 8601 extends it, over the years -32767 to
32767 of `<chrono>`, whose calendar computes the fields. A date is the number of days from 1970-01-01 in 32 bits,
four bytes, so that a comparison, a difference and a step of days are one instruction each, and a field is asked
of the calendar of the standard, not of a second one written here.

A date built from numbers never fails. A day or a month outside its range carries into the next, as Go's
`time.Date` carries: `date(2026, 2, 30)` is 2026-03-02, `date(2026, 13, 1)` is 2027-01-01, `date(2026, 3, 0)` is
the last day of February, and `date(2026, 1, 267)` is the 267th day of 2026, which lets a program count in days or
months without a loop. [is_valid](date/is_valid.md) tells a date that needs no carrying, for numbers that came
from a person; [parse](date/parse.md) refuses what does not exist.

The text is ISO 8601's: [to_string](date/to_string.md) writes the extended calendar date as `std::format`'s `%F`
does (`"2026-09-24"`), and [parse](date/parse.md) reads the three forms of a date, the calendar date, the week date
and the ordinal date, each extended or basic. A date the program itself writes is constructed from the same text,
`time::date d("2026-09-24")`, and a wrong one throws `parse`'s error; a text from outside is parsed. Other shapes
are read and written by a [pattern](README.md#patterns) of `%` as `std::format` and `std::chrono::parse` have them
for `<chrono>`, and [txt::format](README.md#formatting-with-txt) writes a date in a field. A date becomes an
instant at a time of the clock in a zone by [at](date/at.md), and [start_of_day](date/start_of_day.md) is the first
instant of the date there.

## Rules

- A date is a plain value of four bytes, trivially copyable, `constexpr` but for its text and its instants in a
  zone: it lives anywhere.
- A result past either end of the calendar, by carrying or by arithmetic, is the end: the arithmetic saturates,
  as [duration](../core/duration.md)'s does.
- `year()` is the year of the calendar, where 1 BC is year 0 and 2 BC is -1, as in ISO 8601 and `<chrono>`.
- `month()` is a [month](month.md), as Go's `time.Month`: a name for the number, `int(d.month())` the number 1 to
  12; a date is made with either (`date(2026, 9, 25)`, `date(2026, time::month::september, 25)`). `weekday()` is
  a [weekday](weekday.md), numbered as ISO 8601 numbers it, Monday 1 to Sunday 7.
- A date is made implicitly from a `std::chrono::year_month_day` and from a `std::chrono::sys_days` (which is what
  a date holds: days from 1970-01-01), so code written with the standard's calendar passes one on as it is and a
  date compares with either (`d == 2026y / 9 / 24`); back to them only explicitly, so that such a comparison has
  one way to go.
- A day of the calendar is not a [duration](../core/duration.md): it is 23, 24 or 25 hours where the clock
  changes, which is why a date steps by `add_days`, and an instant by a datetime's arithmetic.

### From code written for Go

| With Go | With sgcl::time |
|---|---|
| a `time.Time` at midnight in UTC | `date`: Go has no date without a time of day |
| `time.Date(2026, 2, 30, 0, 0, 0, 0, time.UTC)`, the day carried into March | `date(2026, 2, 30)`, carried the same way: 2026-03-02 |
| `t.AddDate(0, 1, 0)` from January 31: March 3, carried | `d.add_months(1)`: 2026-02-28, the month's last day |
| `t.AddDate(1, 0, 0)` from 2024-02-29: 2025-03-01 | `d.add_years(1)`: 2025-02-28 |
| `t.Weekday()`, Sunday 0 to Saturday 6 | `d.weekday()`, Monday 1 to Sunday 7 |
| `t.YearDay()`, `t.ISOWeek()` | `d.year_day()`, `d.iso_week()` (an [iso_week](iso_week.md) of `year` and `week`) |
| `t.Format("02.01.2006")`, `time.Parse("02.01.2006", text)` | `d.format("%d.%m.%Y")`, `date::parse(text, "%d.%m.%Y")`: a pattern of `%` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](date/date.md) | constructs a date: 1970-01-01, from numbers carried, from the calendar of `<chrono>`, from a literal text |
| [operator std::chrono::year_month_day, operator std::chrono::sys_days](date/operator_conv.md) | converts to the calendar of `<chrono>`, explicitly |

#### Fields

| Function | Description |
|---|---|
| [year](date/year.md) | the year, -32767 to 32767 |
| [month](date/month.md) | the month, January to December |
| [day](date/day.md) | the day of the month, 1 to 31 |
| [weekday](date/weekday.md) | the day of the week, Monday 1 to Sunday 7 |
| [year_day](date/year_day.md) | the day of the year, 1 to 366 |
| [iso_week](date/iso_week.md) | the week of ISO 8601 and the year it belongs to |
| [days_in_month](date/days_in_month.md) | the days of the date's month, 28 to 31 |
| [is_leap_year](date/is_leap_year.md) | checks whether the date's year has a 29th of February |
| [is_valid](date/is_valid.md) | checks whether a year, a month and a day exist as written (static) |

#### Arithmetic

| Function | Description |
|---|---|
| [add_days](date/add_days.md) | the date n days later |
| [add_months](date/add_months.md) | the same day n months later, cut to the month's last |
| [add_years](date/add_years.md) | the same day n years later, cut to the month's last |
| [days_until](date/days_until.md) | the days from this date to another |
| [operator+=, operator-=](date/operator_arith.md) | moves the date by days |

#### In a zone

| Function | Description |
|---|---|
| [at](date/at.md) | the instant of a time of the clock on this date in a zone |
| [try_at](date/try_at.md) | the same, or nothing when the zone's clock skipped the time or showed it twice |
| [start_of_day](date/start_of_day.md) | the first instant of the date in a zone |

#### Text

| Function | Description |
|---|---|
| [parse](date/parse.md) | reads ISO 8601's date, or a date in a pattern of `%` (static) |
| [format](date/format.md) | writes the date in a pattern of `%` |
| [to_string](date/to_string.md) | ISO 8601's extended calendar date, `"2026-09-24"` |

## Non-member functions

| Function | Description |
|---|---|
| [operator+, operator-](date/operator_arith.md) | a date moved by days, the days between two dates |
| [operator==, operator\<=\>](date/operator_cmp.md) | compare two dates |
| [operator\<\<](date/to_string.md) | writes `to_string()` to a stream |

## Complexity

Every member is constant but the text: `parse` and `format` are linear in the length of the text and of the
pattern, and `at`, `try_at` and `start_of_day` search the zone's changes of the clock.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::date d(2026, 9, 24);
    auto [year, week] = d.iso_week();
    println("{} is a {}, day {}, week {} of {}", d, d.weekday(), d.year_day(), week, year);

    println("{} {}", time::date(2026, 2, 30), time::date::is_valid(2026, 2, 30));
    println("{} {}", time::date(2026, 1, 31).add_months(1), time::date(2024, 2, 29).add_years(1));

    time::date christmas("2026-12-25");
    println("{} days to {}", d.days_until(christmas), christmas.format("%A, %d %B"));
    println(d.at(9, 30, time::zone("Europe/Warsaw")));
}
```

Output:

```text
2026-09-24 is a Thursday, day 267, week 39 of 2026
2026-03-02 false
2026-02-28 2025-02-28
92 days to Friday, 25 December
2026-09-24T09:30:00+02:00
```

## See also

- [datetime](datetime.md): an instant and the zone it is seen in, a date at a time of the clock
- [zone](zone.md): the zones a date is placed in by `at`
- [month](month.md), [weekday](weekday.md), [iso_week](iso_week.md): what a date answers with
- [Patterns](README.md#patterns): the specifiers of `%`, written and read
- [duration](../core/duration.md): a span of time
- [time](README.md)
