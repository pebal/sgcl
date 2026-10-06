[sgcl](../../README.md) › [time](../README.md)

# sgcl::time::datetime

```cpp
#include "sgcl/time/datetime.h"   // or "sgcl/time.h"

namespace sgcl::time {
    class datetime;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::time::datetime` is an instant on the time line and the zone it is seen in: Go's `time.Time`. The instant is
the nanoseconds since 1970-01-01T00:00:00Z in 64 bits — the years 1677 to 2262, the range and the unit of
`std::chrono::system_clock` and of [io::file_info::modified](../../io/file_info/README.md), which a datetime is made from as it is —
and the zone is a [zone](../zone/README.md). Sixteen bytes, the zone a `tracked_ptr` to its data, so a datetime lives where a
[string](../../core/string/README.md) does and is taken as `const datetime&`. Go's is 24 bytes, with a monotonic reading inside
that this module keeps in a [stopwatch](../stopwatch/README.md) of its own.

The fields — [year](year.md) to [nanosecond](nanosecond.md), [weekday](weekday.md),
[year_day](year_day.md), [iso_week](iso_week.md), [date](date.md) — are what the zone's
clock shows at that instant. [in](in.md) is the same instant in another zone, [utc](utc.md) and
[local](local.md) in UTC and the local one; [offset](offset.md),
[abbreviation](abbreviation.md) and [is_dst](is_dst.md) are what the zone is then. Two datetimes
are equal when they are the same instant, whatever their zones: the same moment in Warsaw and in UTC is equal, and
they are ordered by the instant. Go's `==` compares the zone too, which is a known trap there (`t.Equal(u)` is what
Go code means); here `a.zone() == b.zone()` asks the other question.

Two kinds of arithmetic. The exact one is `t + d`, `t - d` and `t2 - t1`, a [duration](../../core/duration/README.md) of
nanoseconds: an hour later is 3600 seconds later. The calendar's is [add_days](add_days.md),
[add_months](add_months.md) and [add_years](add_years.md), which keep the time of the clock: a day
later across a change of the clock is 23 or 25 hours later, and a month later from the 31st is the month's last day,
as [date](../date/add_months.md)'s `add_months` cuts it. Both saturate at the ends of the range; Go wraps. A time of the
clock becomes a datetime by a date's [at](../date/at.md) (Go's `time.Date`), and the time now is
[time::now()](../now.md).

## Rules

- A datetime is a value of sixteen bytes, the count and the zone's pointer, not trivially copyable, and is taken as
  `const datetime&`.
- The range is 1677-09-21T00:12:43.145224192Z to 2262-04-11T23:47:16.854775807Z. A date beyond it
  (`date(3000, 1, 1).at(0, 0, z)`), a sum or a difference past it, and `from_unix` of more seconds than it holds
  give its end; a text of an instant beyond it is refused by [parse](parse.md).
- `unix()`, `unix_milli()`, `unix_micro()` and `nanosecond()` round down: half a second before 1970 is
  `unix() == -1` and `nanosecond() == 500000000`, 1969-12-31T23:59:59.5Z.
- A datetime made without a zone (`from_unix(s)`, `datetime(sys_time)`) is in the local zone, as in Go;
  `datetime()` is 1970-01-01T00:00:00Z in UTC.
- `truncate(d)` and `round(d)` count steps from Go's zero time, 0001-01-01T00:00:00Z, as Go does — for a step that
  divides a day the same as counting from midnight UTC, not from the zone's midnight.
- `to_string()` is RFC 3339 with the fraction of a second only where there is one, Go's `RFC3339Nano`; the other
  texts are the [layouts](../layout/README.md) and the [patterns](../README.md#patterns) of [format](format.md).
- Nothing of a datetime throws but its two constructors from text, a literal the program spells, and a stream's
  `operator<<`: a text from outside the program is read by [parse](parse.md), whose error is a value.

### A time of the clock skipped or shown twice

A change of the clock makes some times of it two instants or none. A date's [at](../date/at.md) reads them by the rule
Java, JavaScript's Temporal and iCalendar (RFC 5545) call compatible: a time the clock skipped (the hour lost in
spring) moves on by the length of the skip, 02:30 on the night Warsaw goes from 02:00 to 03:00 being 03:30; a time
it showed twice (the hour repeated in autumn) is the first of the two. The tag `time::earlier`
([earlier_t](../earlier_t.md)) takes the first of a time shown twice and, for a skipped time, the change itself
(03:00, the first time of the clock after the skip); `time::later` takes the second and moves a skipped time on; a
date's [try_at](../date/try_at.md) is nothing for both. Go does not say which it takes ("not guaranteed"), and takes
either by where the time falls against the change read as UTC.

Hours, minutes and seconds out of their ranges carry, as a date's fields do: `d.at(24, 0, z)` is midnight of the next
day, `d.at(0, 0, 86400, z)` too. The calendar's arithmetic ([add_days](add_days.md) and the others) and a
pattern read without an offset ([parse](parse.md)) read a time of the clock by the same rule.

### From code written for Go

| With Go | With sgcl::time |
|---|---|
| `time.Time` | `datetime`: an instant and a zone, 16 bytes (Go's 24 carry a monotonic reading: here a [stopwatch](../stopwatch/README.md)) |
| `time.Now()` | [time::now()](../now.md): in the local zone, as in Go; it follows a test's `manual_clock` |
| `time.Date(y, m, d, h, mi, s, ns, loc)` | `date(y, m, d).at(h, mi, s, zone)`: fields out of range carried as in Go; a skipped or repeated time by the compatible rule, `earlier`, `later` or `try_at` (Go: "not guaranteed") |
| `time.Unix`, `UnixMilli`, `t.Unix()`, `t.UnixNano()` | `from_unix`, `from_unix_milli`, `from_unix_nano`, `unix()`, `unix_milli()`, `unix_nano()` |
| `t.Year()` … `t.Nanosecond()`, `Weekday`, `YearDay`, `ISOWeek`, `Date`, `Clock` | `year()` … `nanosecond()`, `weekday()`, `year_day()`, `iso_week()`, `date()`; the weekday in ISO's numbering, Monday 1 |
| `t.In(loc)`, `t.UTC()`, `t.Local()`, `t.Zone()`, `t.IsDST()` | `in(z)`, `utc()`, `local()`, `abbreviation()` and `offset()`, `is_dst()` |
| `t.Add(d)`, `t.Sub(u)`, `t.AddDate(y, m, d)` | `t + d`, `t - u`, `add_years`, `add_months`, `add_days`: months cut to the month's end (Go carries: 31 January plus a month is 3 March); saturated (Go wraps) |
| `t.Before`, `t.After`, `t.Equal`, `==` | `<`, `>`, `==`: `==` by the instant; Go's `==` compares the zone too |
| `t.Truncate(d)`, `t.Round(d)` | `truncate(d)`, `round(d)`: the same steps, from Go's zero time |
| `t.Format(layout)`, `time.Parse`, `time.ParseInLocation` | `format(pattern)`, `datetime::parse(text, pattern, zone)`: a pattern of `%` (`std::format`'s), not Go's reference time |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](datetime.md) | constructs a datetime: the start of 1970 in UTC, from a `std::chrono` instant, from a literal text |

#### Unix time

| Function | Description |
|---|---|
| [from_unix](from_unix.md) | the datetime of the seconds since 1970 (static) |
| [from_unix_milli](from_unix_milli.md) | the datetime of the milliseconds since 1970 (static) |
| [from_unix_micro](from_unix_micro.md) | the datetime of the microseconds since 1970 (static) |
| [from_unix_nano](from_unix_nano.md) | the datetime of the nanoseconds since 1970 (static) |
| [unix](unix.md) | the seconds since 1970, rounded down |
| [unix_milli](unix_milli.md) | the milliseconds since 1970, rounded down |
| [unix_micro](unix_micro.md) | the microseconds since 1970, rounded down |
| [unix_nano](unix_nano.md) | the nanoseconds since 1970 |
| [to_sys](to_sys.md) | the instant as a `std::chrono::sys_time` |

#### Zone

| Function | Description |
|---|---|
| [zone](zone.md) | the zone the instant is seen in |
| [in](in.md) | the same instant in another zone |
| [utc](utc.md) | the same instant in UTC |
| [local](local.md) | the same instant in the local zone |
| [offset](offset.md) | the zone's offset from UTC at the instant |
| [abbreviation](abbreviation.md) | the zone's abbreviation at the instant |
| [is_dst](is_dst.md) | whether the zone is on daylight saving time at the instant |

#### Fields

| Function | Description |
|---|---|
| [date](date.md) | the date of the zone's clock |
| [year](year.md) | the year |
| [month](month.md) | the month |
| [day](day.md) | the day of the month |
| [hour](hour.md) | the hour, 0 to 23 |
| [minute](minute.md) | the minute, 0 to 59 |
| [second](second.md) | the second, 0 to 59 |
| [nanosecond](nanosecond.md) | the part of the second, 0 to 999999999 |
| [weekday](weekday.md) | the day of the week |
| [year_day](year_day.md) | the day of the year, 1 to 366 |
| [iso_week](iso_week.md) | the week of ISO 8601 and its year |

#### Calendar

| Function | Description |
|---|---|
| [add_days](add_days.md) | the same time of the clock some days on |
| [add_months](add_months.md) | the same time of the clock some months on, cut to the month's end |
| [add_years](add_years.md) | the same time of the clock some years on |
| [start_of_day](start_of_day.md) | the first instant of the datetime's date in its zone |

#### Rounding

| Function | Description |
|---|---|
| [truncate](truncate.md) | down to a whole number of steps |
| [round](round.md) | to the nearest whole number of steps |

#### Text

| Function | Description |
|---|---|
| [to_string](to_string.md) | RFC 3339, with the fraction of a second where there is one |
| [format](format.md) | the text by a layout or by a pattern of `%`; as a locale writes it, in its styles or for a skeleton |
| [format_interval](format_interval.md) | the datetime and another as an interval in a locale's patterns: `24–26 wrz 2026` |
| [parse](parse.md) | reads a text by a layout or by a pattern of `%` (static) |

#### Arithmetic

| Function | Description |
|---|---|
| [operator+=, operator-=](operator_arith.md) | moves the instant by a duration, saturated |

## Non-member functions

| Function | Description |
|---|---|
| [operator+, operator-](operator_arith.md) | the instant moved by a duration, and the duration between two instants, saturated |
| [operator==, operator\<=\>](operator_cmp.md) | compare two instants, whatever their zones |
| [operator\<\<](to_string.md) | writes `to_string()` to a stream |

## Complexity

The instant and the arithmetic of `+` and `-` are constant. What the zone's clock shows — the fields, the offset,
the calendar's arithmetic, the text — looks the instant up among the zone's changes: logarithmic in their number (a
binary search), constant in UTC and in a fixed zone.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    time::zone new_york("America/New_York");

    // A meeting in New York, seen in Warsaw
    auto meeting = time::date(2026, 10, 30).at(9, 30, new_york);
    println("{} is {} in Warsaw", meeting, meeting.in(warsaw).format("%A %H:%M"));
    println(meeting == meeting.utc());  // the same instant

    // A day later is not always 24 hours later
    auto before = time::date(2026, 10, 24).at(12, 0, warsaw);
    println("{} {}", before.add_days(1) - before, before + 24 * hour);

    // The hour skipped in spring and the one repeated in autumn
    println("{} {}", time::date(2026, 3, 29).at(2, 30, warsaw),
            time::date(2026, 3, 29).at(2, 30, warsaw, time::earlier));
    println("{} {} {}", time::date(2026, 10, 25).at(2, 30, warsaw),
            time::date(2026, 10, 25).at(2, 30, warsaw, time::later),
            time::date(2026, 10, 25).try_at(2, 30, 0, warsaw).has_value());

    // A month from the 31st, the start of a day, a step of time
    auto invoice = time::date(2026, 1, 31).at(10, 0, warsaw);
    println("{} {}", invoice.add_months(1), invoice.start_of_day());
    auto t = time::datetime::from_unix_nano(1790246475122575000, warsaw);
    println("{} {} {}", t, t.truncate(15 * minute), t.round(second));

    // Before 1970 the division goes down
    auto half = time::datetime::from_unix_milli(-500, time::zone::utc());
    println("{} {}", half, half.unix());

    // The time now follows a test's clock
    async::manual_clock clock;
    clock.install();
    auto start = time::now();
    clock.advance(90 * minute);
    println(time::now() - start);
}
```

Output:

```text
2026-10-30T09:30:00-04:00 is Friday 14:30 in Warsaw
true
25h0m0s 2026-10-25T11:00:00+01:00
2026-03-29T03:30:00+02:00 2026-03-29T03:00:00+02:00
2026-10-25T02:30:00+02:00 2026-10-25T02:30:00+01:00 false
2026-02-28T10:00:00+01:00 2026-01-31T00:00:00+01:00
2026-09-24T12:41:15.122575+02:00 2026-09-24T12:30:00+02:00 2026-09-24T12:41:15+02:00
1969-12-31T23:59:59.5Z -1
1h30m0s
```

## See also

- [zone](../zone/README.md): the zones a datetime is seen in
- [date](../date/README.md): a date with no time of day, and [at](../date/at.md), [try_at](../date/try_at.md) that make a datetime
  of it
- [now](../now.md): the time now
- [layout](../layout/README.md): the formats known by name; [README: Patterns](../README.md#patterns): the patterns of `%`;
  [README: Formatting with txt](../README.md#formatting-with-txt): `txt::format` of a datetime
- [duration](../../core/duration/README.md): what `t2 - t1` is
- [stopwatch](../stopwatch/README.md): the time elapsed, on the monotonic clock
- [time](../README.md): the module
