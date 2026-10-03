[sgcl](../README.md) › time

# sgcl::time

```cpp
#include "sgcl/time.h"   // namespace sgcl::time
```

What Go has in `time`, less what the [async](../async/README.md) module already has: dates of the calendar, the
zones of the system's tz database, an instant seen in a zone, the time elapsed, and the text of all of them — the
formats known by name (RFC 3339, the date of HTTP and of e-mail, ISO 8601) and patterns of `%` as `std::format`
has them for `<chrono>`, both ways. The module depends on [core](../core/README.md),
[concurrent](../concurrent/README.md) (the registry of zones) and [txt](../txt/README.md) (`txt::format`); the
index of the whole interface is [the modules](../README.md).

A span of time is [sgcl::duration](../core/duration.md), a class of core, not of this module: the timers of async
take one, so it lives below both. It is Go's `time.Duration` with Go's text (`"1h30m"`, `duration::parse`,
`to_string()`, `seconds()`, saturated arithmetic), and every `std::chrono` duration of an integral count converts
into it. The timers themselves — Go's `time.Sleep`, `After`, `Tick`, `NewTimer` and `AfterFunc` — are async's
`sleep`, `after`, `tick` and `timeout` ([timers](../async/README.md#time)), and so is the monotonic clock with a test's
[manual_clock](../async/manual_clock.md); the [stopwatch](stopwatch.md) and [now](now.md) follow the core's
[clock](../core/clock.md), which a test's manual clock moves, so code that measures itself or asks for the time is
tested with no real waiting.

A date, an instant and a zone are values: a [date](date.md) is four bytes of days, a [datetime](datetime.md) the
nanoseconds since 1970 and its [zone](zone.md), a zone one word. What cannot fail does not: a date built from
numbers carries a day or a month out of its range into the next, as Go's `time.Date` does, a time of the clock
that a change of the clock skipped is moved on, and a result past either end of a range is the end. What may fail —
a text from a person, a file or the network, a zone the system may not have — is read into an
[expected](../core/expected.md) of the value or an [error](error.md) that says why and at which byte.

## The rules

1. The namespace is `sgcl::time`, so a call reads as Go's does, `time::date(2026, 9, 24)`, `time::now()`. Under
   `using namespace sgcl;` the namespace and the C library's `time()` meet: a bare `time(nullptr)` is ambiguous and
   does not compile; write `std::time(nullptr)`. `time::now()` and the rest of the module are unaffected, since a
   name before `::` is looked up among namespaces and types only.
2. Nothing in the module throws but `zone::fixed` of an offset of a day or more, a mistake of the program
   (`invalid_argument`), and the constructors from a text the program itself writes — a date, a datetime or a zone
   spelled as a literal (`time::date d("2026-09-24")`, `time::zone warsaw("Europe/Warsaw")`) — which throw
   `bad_expected_access<time::error>` with the message of `parse` or `load`.
3. A text or a name that comes from outside the program (a setting, the user, a header) is read by `parse`,
   `load`, `from_tzif` or `from_posix` into an `expected<T, time::error>`: the value, or an [error](error.md) with a
   sentence and the byte of the text or of the file the reading stopped on, `message()` and `offset()`.
4. A [date](date.md) is a plain value, trivially copyable: it lives anywhere. A [zone](zone.md) is a `tracked_ptr`
   to its data, and a [datetime](datetime.md) holds one, so both live where a `tracked_ptr` may — on a stack, in a
   managed object, in a container of the library — as a [string](../core/string.md) does
   ([the rules of core](../core/README.md#the-rules), 1), and a function takes them as `const zone&` and
   `const datetime&`.
5. The zones are read from the files of the system's tz database, `/usr/share/zoneinfo` and the other places Unix
   systems keep it, once per name. Windows has no such directory: there `zone::load` finds nothing, and
   `zone::from_tzif` takes a database one carries; Go's `time/tzdata`, a database built into the program, has no
   counterpart yet.

### Patterns

A pattern of `%` specifiers is the language of `std::format` for `<chrono>` (and of C's `strftime` and `strptime`
before it): `t.format("%d.%m.%Y %H:%M")`, `txt::format("{:%H:%M}", t)`,
`datetime::parse(text, "%d.%m.%Y %H:%M", zone)`, `date::parse(text, "%B %d, %Y")`. Every specifier of C++20 is
there — `%Y %y %C %G %g %m %d %e %j %U %W %V %u %w %a %A %b %B %h %H %I %M %S %p %R %T %r %c %x %X %D %F %z %Ez %Oz
%Z %n %t %%` and the `E` and `O` forms of the C locale — written as libc++'s `std::format` writes them, byte for
byte (compared over the whole range of a datetime and the years -32767 to 32767 of a date), and read as
`std::chrono::parse` reads them (compared with Howard Hinnant's date library, its reference implementation). Names
are English, as `std::format` writes them without a locale; names of months in other languages (`"24 września"`)
need CLDR's data and are not here.

`%` rather than Go's reference time (`"02.01.2006 15:04"`) or CLDR's letters (`"dd.MM.yyyy HH:mm"`): it is one
language with `txt::format` (`{:%F}` means there what it means in `std::format`), known from C, C++ and Python,
with no letters to quote; Go's and CLDR's both have their traps of a silent wrong answer (`yyyy`/`YYYY`,
`mm`/`MM`, a digit in a literal).

- A writer never fails: a specifier it does not know, or one the value does not answer (`%H` of a date, `%Q` of a
  datetime), is written as it stands.
- `%S` and `%T` write the fraction of a second the value holds, as `std::format` does: nine digits for a datetime
  (`12:41:15.000000000`), none for a `sys_seconds`. A text to the second is `%X`, or a [layout](layout.md): the
  header of HTTP is `t.format(time::http)`.
- `%Z` writes the zone's abbreviation (`CEST`; `UTC`; a fixed offset's name), `%z` the offset `+0200`, `%Ez` and
  `%Oz` `+02:00`; seconds of an offset are dropped.
- Reading, a pattern's white space matches none or more of it, `%n` one and `%t` none or one; names are read in any
  case and as prefixes (`%a%b` reads `SunSep`); a number is read to its specifier's width, `%Y` four digits after a
  sign (`%5Y` five); `%S` reads a fraction of at most nine digits after a point or a comma; `%z` takes `+hh` or
  `+hhmm`, the sign optional, `%Ez` a colon; `%y` alone is 1969 to 2068, as POSIX has it, with `%C` its century.
- The fields read must agree: a day of the week, a month or a day of the year that are not the date's are an error.
  A date is needed — by year, month and day, by a day of the year, by a week and a day of it — and the text must
  end where the pattern does. A date's pattern refuses the specifiers of a time and of a zone.
- An offset in the text makes a fixed zone (`+00:00` UTC). Without one the text is a time of the zone given (UTC
  unless another is), read by the compatible rule of [at](date/at.md); a `%Z` naming an abbreviation that zone shows
  then settles a time shown twice (`"2026-10-25 02:30 CET"` in Warsaw is the second 02:30), and `UTC`, `GMT`, `UT`
  and `Z` mean UTC. A second of 60 is read only as a leap second, the last second of a day in UTC.
- Every refusal is an [error](error.md) with a sentence and the byte of the text where the field that failed
  starts.
- Where this writes otherwise than libc++, following the standard's text: `%G` of the years -999 to -1 (`-0999`,
  libc++ `-999`), `%EC` of a negative year (floored, libc++ truncates), the `-` of a negative duration once before
  the first specifier (libc++ before each), no precision for a duration (the standard's text cuts characters —
  `{:.3}` of 1.23456s is `1.2` — which is refused where the program is compiled).

### Formatting with txt

In `txt::format` a time is a value like any other: `{}` writes its `to_string()` (a datetime RFC 3339 with the
fraction where there is one, a date `%F`, a weekday and a month their English names, a
[duration](../core/duration.md) Go's text), a pattern after the colon writes the pattern, and a width pads the
whole: `{:>12%F}`. The field is the one `std::format` gives `<chrono>` — `[[fill]align][width]` and then the pattern
from its first `%` to the brace, colons and all — and it is checked where the program is compiled: `{:%Q}` of a
datetime or `{:%H}` of a date does not compile. A weekday takes `%a`, `%A`, `%u` and `%w`, a month `%b`, `%B`, `%h`
and `%m`. The types of `<chrono>` — `sys_time` and `local_time` of an integral duration, `year_month_day`,
`weekday`, `hh_mm_ss`, `duration` — are written by the same writer as `std::format` writes them, so code that holds
a `system_clock::time_point` hands it to `txt::format` as it is; `txt::format_to` writes into a caller's buffer with
nothing allocated ([txt::format](../txt/format.md)).

## Functions

| Function | Header | Description |
|---|---|---|
| [now](now.md) | `datetime.h` | the time now, on the system's clock, in the local zone; follows a test's `manual_clock` |
| [to_string, operator\<\<](to_string.md) | `date.h` | the English name of a month or a day of the week, as Go writes it: `"September"`, `"Monday"` |

## Classes

| Class | Header | Description |
|---|---|---|
| [date](date.md) | `date.h` | a date of the Gregorian calendar, with no time of day and no zone: the fields, the ISO week, the calendar's arithmetic (`add_months` cut to the month's end), ISO 8601 and patterns of `%` read and written, `at(9, 30, zone)` |
| [datetime](datetime.md) | `datetime.h` | an instant and the zone it is seen in (Go's `time.Time`): the fields of the zone's clock, `t + d` and `t2 - t1`, the calendar's arithmetic across a change of the clock, `truncate`, `round`, the layouts and patterns |
| [earlier_t, later_t](earlier_t.md) | `date.h` | the tags that choose which instant a time of the clock shown twice, or skipped, is read as |
| [error](error.md) | `error.h` | why a text is not a date or a file not a zone: `message()` and `offset()` |
| [iso_week](iso_week.md) | `date.h` | a week of ISO 8601: the year it belongs to and its number |
| [layout](layout.md) | `layout.h` | a format known by name, written and read by code of its own: `rfc3339`, `rfc3339_nano`, `http`, `email`, `iso8601` |
| [stopwatch](stopwatch.md) | `stopwatch.h` | the time elapsed since a start, on the library's monotonic clock; `measure(f)` |
| [zone](zone.md) | `zone.h` | a time zone: UTC, a fixed offset, a zone of the system's tz database, one from a TZif file or a POSIX TZ string, the local one; the offset, the abbreviation and daylight saving time at an instant, the changes around it |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [month](month.md) | `date.h` | a month of the year, January 1 to December 12, as Go's `time.Month` |
| [weekday](weekday.md) | `date.h` | a day of the week in ISO's numbering, Monday 1 to Sunday 7 |

## Constants

| Constant | Header | Description |
|---|---|---|
| `earlier` | `date.h` | the tag of the first of a time shown twice, and of the change for a skipped one ([earlier_t](earlier_t.md)) |
| `email` | `layout.h` | the date of e-mail, RFC 5322: `Thu, 24 Sep 2026 12:41:15 +0200` ([layout](layout.md)) |
| `http` | `layout.h` | the date of HTTP, RFC 9110's IMF-fixdate: `Thu, 24 Sep 2026 10:41:15 GMT` ([layout](layout.md)) |
| `iso8601` | `layout.h` | ISO 8601, written as `rfc3339_nano`, read in its broad profile ([layout](layout.md)) |
| `later` | `date.h` | the tag of the second of a time shown twice ([earlier_t](earlier_t.md)) |
| `rfc3339` | `layout.h` | RFC 3339 to the second: `2026-09-24T12:41:15+02:00` ([layout](layout.md)) |
| `rfc3339_nano` | `layout.h` | RFC 3339 with the fraction of a second: `2026-09-24T12:41:15.122575+02:00` ([layout](layout.md)) |

## See also

- [duration](../core/duration.md): the span of time, in core
- [sleep, after, tick, timeout](../async/README.md#time), [clock](../core/clock.md),
  [manual_clock](../async/manual_clock.md): the timers and the clock the stopwatch reads
- [txt::format](../txt/format.md): the patterns of values in text
- [The modules](../README.md)
