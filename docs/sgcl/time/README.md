# sgcl::time

What Go has in `time`, less what the [async](../async/README.md) module already has: dates of the calendar, the zones of the system's tz database, an instant seen in a zone, the time elapsed, and the text of all of them — the formats known by name (RFC 3339, the date of HTTP and of e-mail, ISO 8601) and patterns of `%` as `std::format` has them for `<chrono>`, both ways. `#include "sgcl/time/time.h"` brings the module in; it depends on [`core`](../core/README.md), [`concurrent`](../concurrent/README.md) (the registry of zones) and [`txt`](../txt/README.md) (`txt::format`); the stopwatch and `now()` follow the core's [clock](../core/clock.md), which a test's manual one (async's) moves, and the index of the whole interface is [`docs/sgcl/`](../README.md).


**A name to know.** Under `using namespace sgcl;` the namespace `sgcl::time` and the C library's `time()` meet: a bare `time(nullptr)` is ambiguous and does not compile. Write `std::time(nullptr)`; `time::now()` and the rest of the module are unaffected, since a name before `::` is looked up among namespaces and types only.

## What is here and what is elsewhere

A span of time is [`sgcl::duration`](../core/duration.md), a class of the core module, not of this one: the timers of async take one, so it lives below both. It is Go's `time.Duration` with Go's text (`"1h30m"`, `duration::parse`), and every `std::chrono` duration of an integral count converts into it. The timers themselves — `sleep`, `after`, `tick`, `timeout` — and the monotonic clock with a test's [manual clock](../async/manual_clock.md) are async's and stay there.

| | |
|---|---|
| [date](date.md) | a date of the Gregorian calendar, with no time of day and no zone: the fields, the day of the week in ISO's numbering, the ISO week, the day of the year, the calendar's arithmetic (`add_days`, `add_months` cut to the month's end, `add_years`, `days_until`), ISO 8601's three forms of a date and patterns of `%` read and written, `at(9, 30, zone)` |
| [zone](zone.md) | a time zone: UTC, a fixed offset, a zone of the system's tz database by name, one from the bytes of a TZif file or from a POSIX TZ string, the local one; the offset, the abbreviation and daylight saving time at an instant, the changes before and after it |
| [datetime](datetime.md) | an instant and the zone it is seen in (Go's `time.Time`): the fields of the zone's clock, `t + d` and `t2 - t1`, the calendar's arithmetic across a change of the clock, the times of the clock skipped and shown twice, `truncate`, `round`; `time::now()` |
| [text](layout.md) | `rfc3339`, `rfc3339_nano`, `http`, `email`, `iso8601` written and read; patterns of `%` written and read; `txt::format("{:%H:%M}", t)` and the types of `<chrono>` in `txt::format` |
| [stopwatch](#stopwatch) | the time elapsed since a start, on the library's monotonic clock |
| [error](#errors) | why a text is not a date or a file not a zone: `message()` and `offset()` |

`weekday` (Monday 1 to Sunday 7) and `iso_week` (a year and a week) are the two small types a date answers with; they are on the [date](date.md) page.

The namespace is `sgcl::time`, so a call reads as Go's does, `time::date(2026, 9, 24)`, and never meets libc's `::time()`: a name before `::` is looked up among namespaces and types only.

## Errors

Nothing in the module throws but `zone::fixed` of a day or more, a mistake of the program. A text that may not be what it should (a date from a person, from a file, from the network) and a zone that may not be there are read into an [`expected<T, time::error>`](../core/expected.md): the value, or an error with a sentence and the byte of the text or of the file the reading stopped on, `message()` and `offset()`. What cannot fail does not: a date built from numbers carries a day or a month out of its range into the next, as Go's `time.Date` does, a time of the clock that a change of the clock skipped is moved on, and a result past either end of the range is the end.

```cpp
class error {
public:
    explicit error(const string& message, size_t offset = 0);
    string message() const;                   // "a month from 01 to 12 expected"
    size_t offset() const noexcept;           // the byte of the input
    friend bool operator==(const error&, const error&) noexcept;
};
```

## stopwatch

```cpp
class stopwatch {
public:
    stopwatch() noexcept;                     // started at once
    duration elapsed() const noexcept;        // since the start
    duration restart() noexcept;              // what had elapsed, and a new start from now
};
```

The time elapsed, on [`sgcl::clock`](../core/clock.md): the steady clock, which a change of the system's wall clock does not move, unless a test has installed a manual clock, and then the test's time, so that code that measures itself is tested with no real waiting. Go keeps such a monotonic reading inside every `time.Time`, where it is invisible and lost by the first `Round(0)` or a trip through text; here it has a name of its own, and a date and time stays a plain value. Eight bytes, a point of the clock.

```cpp
#include "sgcl/sgcl.h"
#include "sgcl/time/time.h"
#include <chrono>
#include <iostream>

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::manual_clock clock;               // a test's clock: time moves only by advance
    clock.install();
    time::stopwatch sw;
    clock.advance(1500ms);
    std::cout << sw.elapsed() << "\n";               // 1.5s
    std::cout << sw.restart() << " " << sw.elapsed() << "\n";   // 1.5s 0s
    clock.uninstall();

    time::stopwatch real;             // the steady clock again
    long sum = 0;
    for (int i : range(1000000)) {
        sum += i;
    }
    std::cout << (real.elapsed() < 10 * second) << " " << sum << "\n";   // 1 499999500000
    return 0;
}
```

Output:

```text
1.5s
1.5s 0s
1 499999500000
```

## SGCL and Go

| Go | sgcl::time | |
|---|---|---|
| `time.Time` | `datetime` | an instant and a zone, 16 bytes (Go's 24 carry a monotonic reading: here a `stopwatch`) |
| `time.Now()` | `time::now()` | the local zone, as in Go; follows a test's `manual_clock` |
| `time.Date(y, m, d, h, mi, s, ns, loc)` | `date(y, m, d).at(h, mi, s, zone)` | fields out of range carried as in Go; a skipped or repeated time by the compatible rule, `earlier`, `later`, `try_at` (Go: "not guaranteed") |
| `time.Unix`, `UnixMilli`, `t.Unix()`, `t.UnixNano()` | `from_unix`, `from_unix_milli`, `from_unix_nano`, `unix()`, `unix_milli()`, `unix_nano()` | |
| `t.Year()` … `t.Nanosecond()`, `Weekday`, `YearDay`, `ISOWeek`, `Date`, `Clock` | `year()` … `nanosecond()`, `weekday()`, `year_day()`, `iso_week()`, `date()` | the weekday in ISO's numbering, Monday 1 |
| `t.In(loc)`, `t.UTC()`, `t.Local()`, `t.Zone()`, `t.IsDST()` | `in(z)`, `utc()`, `local()`, `abbreviation()` and `offset()`, `is_dst()` | |
| `t.Add(d)`, `t.Sub(u)`, `t.AddDate(y, m, d)` | `t + d`, `t - u`, `add_years`, `add_months`, `add_days` | months cut to the month's end (Go carries: 31 January plus a month is 3 March); saturated (Go wraps) |
| `t.Before`, `t.After`, `t.Equal`, `==` | `<`, `>`, `==` | `==` by the instant; Go's `==` compares the zone too |
| `t.Truncate(d)`, `t.Round(d)` | `truncate(d)`, `round(d)` | the same steps, from Go's zero time |
| `t.Format(layout)`, `time.Parse`, `time.ParseInLocation` | `format(pattern)`, `datetime::parse(text, pattern, zone)` | a pattern of `%` (`std::format`'s), not Go's reference time |
| `time.RFC3339`, `RFC3339Nano`, `RFC1123Z`, `http.TimeFormat` and `http.ParseTime`, `net/mail.ParseDate` | `time::rfc3339`, `rfc3339_nano`, `email`, `http` | `http` reads RFC 850 and asctime too; `email` RFC 5322's obsolete forms; and `iso8601`, which Go has not |
| `RFC1123`, `RFC822`, `RFC850`, `ANSIC`, `Kitchen`, `Stamp` | a pattern: `"%a, %d %b %Y %T %Z"`, `"%d %b %y %H:%M %Z"`, `"%A, %d-%b-%y %T %Z"`, `"%c"`, `"%I:%M%p"`, `"%b %e %T"` | |
| `time.Duration`, `ParseDuration`, `d.String()`, `d.Seconds()` | [`sgcl::duration`](../core/duration.md), `duration::parse`, `to_string()`, `seconds()` | in core; the same text; saturated arithmetic |
| `time.Location`, `LoadLocation`, `FixedZone`, `UTC`, `Local` | `zone`, `zone::load`, `zone::fixed`, `zone::utc()`, `zone::local()` | loaded once per name (Go reads the file at every `LoadLocation`); `local().name()` is `"Europe/Warsaw"`, not `"Local"` |
| `LoadLocationFromTZData` | `zone::from_tzif` | and `from_posix`, which Go has not |
| — | `z.offset_at(t)`, `next_transition`, `previous_transition`, `zone::available()` | Go cannot list the changes of a zone or the zones |
| `time.Since(t)`, the monotonic reading inside a `Time` | `stopwatch` | on the library's clock, a test's manual one included |
| `time.Sleep`, `After`, `Tick`, `NewTimer`, `AfterFunc` | `sleep`, `after`, `tick`, `timeout` in [async](../async/timer.md) | not in this module |
| — | [`date`](date.md) | Go has no date without a time of day |
| `time/tzdata` (a database built in) | — | later, with Windows; `from_tzif` takes a database one carries |

## See also

- [date](date.md), [zone](zone.md), [datetime](datetime.md), [text](layout.md): the module's pages
- [duration](../core/duration.md): the span of time, in core
- [sleep, after, tick, timeout](../async/timer.md) and [clock](../core/clock.md), [manual_clock](../async/manual_clock.md): the timers and the clock the stopwatch reads
