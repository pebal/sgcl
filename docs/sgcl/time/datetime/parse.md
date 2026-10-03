[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::parse

```cpp
/*(1)*/ static expected<datetime, error> parse(const string& text, layout format) noexcept;
/*(2)*/ static expected<datetime, error> parse(const string& text, const string& pattern,
                                               const time::zone& z = time::zone::utc()) noexcept;
```

Reads a datetime from a text that may not be one (from a person, a file, the network), Go's `time.Parse` and
`time.ParseInLocation`.

1. A text in a [layout](../layout.md), read by code of its own with no pattern walked: `time::rfc3339` and
   `time::rfc3339_nano` (a fraction of any length, `t` and `z` in small letters), `time::http` (the IMF-fixdate and
   the two obsolete forms a recipient must take, RFC 850's and asctime's), `time::email` (RFC 5322 with its obsolete
   forms) and `time::iso8601` (the broad profile: basic and extended forms, week and ordinal dates, a date alone). The
   datetime is in UTC where the text says UTC or GMT or says nothing, else in a fixed zone of the offset the text
   gives.
2. A text in a [pattern](../README.md#patterns) of `%` specifiers, read as `std::chrono::parse` reads one:
   `"%d.%m.%Y %H:%M"`. A date is needed — by year, month and day, by a day of the year, or by a week and a day of
   it —, the time of day is midnight where the pattern has none, and the text must end where the pattern does. An
   offset in the text (`%z`) makes a fixed zone, `+00:00` UTC. With none, the text is a time of the clock of `z`
   (UTC unless another is given), read by the compatible rule
   ([A time of the clock skipped or shown twice](../datetime.md#a-time-of-the-clock-skipped-or-shown-twice)): a
   skipped time moves on, a time shown twice is the first of the two; then a `%Z` naming an abbreviation that zone
   has at that time settles a time shown twice (`"25.10.2026 02:30 CET"` in Warsaw is the second 02:30), and `UTC`
   or `GMT` mean UTC.

A second of 60 is read only where it is the last second of a day in UTC, a leap second, and then as the first instant
of the next day, as `timegm` reads it; anywhere else it is an error. An instant beyond the range of a datetime, the
years 1677 to 2262, is an error.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to read |
| `format` | the layout: `time::rfc3339`, `rfc3339_nano`, `http`, `email` or `iso8601` |
| `pattern` | the pattern, `%` specifiers and the characters matched between them |
| `z` | the zone of a time without an offset |

## Return value

The datetime, or an [error](../error.md) with a sentence and the byte of the text where the field that failed starts:
`message()` and `offset()`.

## Complexity

Linear in the length of `text` (and of `pattern`), plus the look-up of the time in the zone's changes.

## Exceptions

None.

## Notes

A text the program itself writes is constructed, `time::datetime t("2026-09-24T12:41:15+02:00", time::rfc3339)`
([constructor](datetime.md)), and a wrong one throws. What each layout reads, and where this reads otherwise than
Go, is on the [layout](../layout.md) page; the reading of every specifier is in
[README: Patterns](../README.md#patterns).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    // The date of HTTP in its three forms, as a header brings it
    for (const char* text : {"Sun, 06 Nov 1994 08:49:37 GMT", "Sunday, 06-Nov-94 08:49:37 GMT",
                             "Sun Nov  6 08:49:37 1994"}) {
        println(time::datetime::parse(text, time::http).value());
    }
    println(time::datetime::parse("1990-12-31T15:59:60-08:00", time::rfc3339).value());
    println(time::datetime::parse("2026-W39-4T12:41,5+02", time::iso8601).value());

    // A pattern, a time of Warsaw's clock
    time::zone warsaw("Europe/Warsaw");
    println(time::datetime::parse("24.09.2026 12:41", "%d.%m.%Y %H:%M", warsaw).value());
    println(time::datetime::parse("25.10.2026 02:30 CET", "%d.%m.%Y %H:%M %Z", warsaw).value());
    println(time::datetime::parse("24.09.2026 12:41 +0530", "%d.%m.%Y %H:%M %z").value());

    // What is not a datetime says why and where
    auto bad = time::datetime::parse("Sun, 31 Feb 1994 08:49:37 GMT", time::http);
    println("{} (byte {})", bad.error().message(), bad.error().offset());
    for (const char* text : {"24.09.2026", "31.09.2026 12:00", "24.09.2026 12:00 CET"}) {
        auto t = time::datetime::parse(text, "%d.%m.%Y %H:%M", warsaw);
        println("\"{}\": {} (byte {})", text, t.error().message(), t.error().offset());
    }
}
```

Output:

```text
1994-11-06T08:49:37Z
1994-11-06T08:49:37Z
1994-11-06T08:49:37Z
1990-12-31T16:00:00-08:00
2026-09-24T12:41:30+02:00
2026-09-24T12:41:00+02:00
2026-10-25T02:30:00+01:00
2026-09-24T12:41:00+05:30
a day that the month has expected (byte 5)
"24.09.2026": an hour from 0 to 23 expected (byte 10)
"31.09.2026 12:00": a day that the month has expected (byte 0)
"24.09.2026 12:00 CET": the end of the text expected (byte 16)
```

## See also

- [format](format.md): writes the text
- [(constructor)](datetime.md): a datetime from a literal text
- [error](../error.md): why a text is not a datetime
- [layout](../layout.md): the formats known by name
- [sgcl::time::datetime](../datetime.md)
