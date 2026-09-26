# The text of time: layouts and patterns

```cpp
#include "sgcl/time/layout.h"   // or "sgcl/time/time.h"

namespace sgcl::time {
    class layout;                                  // a format known by name
    inline constexpr layout rfc3339, rfc3339_nano, http, email, iso8601;
}
// and txt::format of datetime, date, weekday, duration and the types of <chrono>
```

Two ways to say how a time is written and read.

**A layout** is one of a closed set of formats known by name, each written and read by code of its own with no pattern walked: `t.format(time::http)`, `datetime::parse(text, time::http)`.

| layout | written | read as well |
|---|---|---|
| `rfc3339` | `2026-09-24T12:41:15+02:00`, `Z` for a zero offset (Go's `RFC3339`) | a fraction of any length, `t` and `z` in small letters (RFC 3339 5.6), a leap second at 23:59:60 UTC |
| `rfc3339_nano` | `2026-09-24T12:41:15.122575+02:00`, the fraction without its trailing zeros (Go's `RFC3339Nano`) | as `rfc3339` |
| `http` | `Thu, 24 Sep 2026 10:41:15 GMT`, RFC 9110's IMF-fixdate, always in GMT | the two obsolete forms a recipient must take: RFC 850 (`Thursday, 24-Sep-26 10:41:15 GMT`) and asctime (`Thu Sep 24 10:41:15 2026`) |
| `email` | `Thu, 24 Sep 2026 12:41:15 +0200`, RFC 5322 | the obsolete forms of RFC 5322: no day of the week, no seconds, a year of two or three digits, zones by name (`EST`, `GMT`, a military letter), comments and folding white space |
| `iso8601` | as `rfc3339_nano` | the broad profile of ISO 8601: basic and extended forms, week and ordinal dates, a date alone, hours or minutes with a fraction, a comma for the point, `24:00`, `±hh`, `±hhmm`, `±hh:mm` |

**A pattern** of `%` specifiers is the language of `std::format` for `<chrono>` (and of C's `strftime` and `strptime` before it): `t.format("%d.%m.%Y %H:%M")`, `txt::format("{:%H:%M}", t)`, `datetime::parse(text, "%d.%m.%Y %H:%M", zone)`, `date::parse(text, "%B %d, %Y")`. Every specifier of C++20 is there — `%Y %y %C %G %g %m %d %e %j %U %W %V %u %w %a %A %b %B %h %H %I %M %S %p %R %T %r %c %x %X %D %F %z %Ez %Oz %Z %n %t %%` and the `E` and `O` forms of the C locale — written as libc++'s `std::format` writes them, byte for byte (compared over the whole range of a datetime and the years -32767 to 32767 of a date), and read as `std::chrono::parse` reads them (compared with Howard Hinnant's date library, its reference implementation). Names are English, as `std::format` writes them without a locale.

Why `%` and not Go's reference time (`"02.01.2006 15:04"`) or CLDR's letters (`"dd.MM.yyyy HH:mm"`): it is one language with `txt::format` (`{:%F}` means there what it means in `std::format`), known from C, C++ and Python, with no letters to quote; Go's and CLDR's both have their traps of a silent wrong answer (`yyyy`/`YYYY`, `mm`/`MM`, a digit in a literal). Names of months in other languages (`"24 września"`) need CLDR's data and are not here.

In `txt::format` a time is a value like any other: `{}` writes its `to_string()` (a datetime RFC 3339 with the fraction where there is one, a date `%F`, a weekday its name, a [duration](../core/duration.md) Go's text), a pattern after the colon writes the pattern, and a width pads the whole: `{:>12%F}`. The field is the one `std::format` gives `<chrono>` — `[[fill]align][width]` and then the pattern from its first `%` to the brace, colons and all — and it is checked where the program is compiled: `{:%Q}` of a datetime or `{:%H}` of a date does not compile. The types of `<chrono>` — `sys_time` and `local_time` of an integral duration, `year_month_day`, `weekday`, `hh_mm_ss`, `duration` — are written by the same writer as `std::format` writes them, so code that holds a `system_clock::time_point` hands it to `txt::format` as it is.

## Rules

- A writer never fails: a specifier it does not know, or one the value does not answer (`%H` of a date, `%Q` of a datetime), is written as it stands.
- `%S` and `%T` write the fraction of a second the value holds, as `std::format` does: nine digits for a datetime (`12:41:15.000000000`), none for a `sys_seconds`. A text to the second is `%X`, or a layout: the header of HTTP is `t.format(time::http)`.
- `%Z` writes the zone's abbreviation (`CEST`; `UTC`; a fixed offset's name), `%z` the offset `+0200`, `%Ez` and `%Oz` `+02:00`; seconds of an offset are dropped.
- Reading, a pattern's white space matches none or more of it, `%n` one and `%t` none or one; names are read in any case and as prefixes (`%a%b` reads `SunSep`); a number is read to its specifier's width, `%Y` four digits after a sign (`%5Y` five); `%S` reads a fraction of at most nine digits after a point or a comma; `%z` takes `+hh` or `+hhmm`, the sign optional, `%Ez` a colon; `%y` alone is 1969 to 2068, as POSIX has it, with `%C` its century.
- The fields read must agree: a day of the week, a month or a day of the year that are not the date's are an error. A date is needed — by year, month and day, by a day of the year, by a week and a day of it — and the text must end where the pattern does.
- An offset in the text makes a fixed zone (`+00:00` UTC). Without one the text is a time of the zone given (UTC unless another is), read by the compatible rule of [datetime](datetime.md); a `%Z` naming an abbreviation that zone shows then settles a time shown twice (`"2026-10-25 02:30 CET"` in Warsaw is the second 02:30), and `UTC` or `GMT` mean UTC.
- A date's pattern refuses the specifiers of a time and of a zone.
- A second of 60 is read only where it is the last second of a day in UTC, a leap second, and then as the first instant of the next day (`timegm`'s reading); RFC 3339 shows such times. Anywhere else it is an error.
- A year of two digits: in RFC 850 dates of HTTP the latest year with those digits not more than 50 years ahead of `time::now()` (RFC 9110); in e-mail 1950 to 2049 (RFC 5322 4.3); in a pattern's `%y` 1969 to 2068 (POSIX).
- Every refusal is an [`error`](README.md#errors) with a sentence and the byte of the text where the field that failed starts.
- Where this reads otherwise than Go, by design: Go takes one digit where the RFCs ask for two, any abbreviation where RFC 9110 asks for GMT and where RFC 5322 lists ten names (reading `EST` as UTC), offsets past 23:59, text after an e-mail date; Go refuses a leap second, `t` and `z` in small letters, RFC 5322's obsolete forms; Go's two-digit years pivot at 1969 everywhere.
- Where this writes otherwise than libc++, following the standard's text: `%G` of the years -999 to -1 (`-0999`, libc++ `-999`), `%EC` of a negative year (floored, libc++ truncates), the `-` of a negative duration once before the first specifier (libc++ before each), no precision for a duration (the standard's text cuts characters — `{:.3}` of 1.23456s is `1.2` — which is refused where the program is compiled).

## Members

```cpp
// datetime (datetime.md)
string format(layout format) const;
string format(const string& pattern) const;
static expected<datetime, error> parse(const string& text, layout format);
static expected<datetime, error> parse(const string& text, const string& pattern, const zone& z = zone::utc());

// date (date.md)
string format(const string& pattern) const;
static expected<date, error> parse(const string& text, const string& pattern);

// txt::format
txt::format("{}", t);                  // to_string()
txt::format("{:%d.%m.%Y %H:%M}", t);   // a pattern, checked where the program is compiled
txt::format("{:>12%F}", d);            // a field padded
txt::format_to(buffer, "{:%R}", t);    // into a caller's buffer, nothing allocated
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include "sgcl/time/time.h"
#include <chrono>
#include <iostream>

using namespace sgcl;

int main() {
    auto warsaw = time::zone::load("Europe/Warsaw").value();
    auto t = time::date(2026, 9, 24).at(12, 41, 15, warsaw) + 122575 * microsecond;

    // The layouts
    std::cout << t.format(time::rfc3339) << "\n" << t.format(time::rfc3339_nano) << "\n"
              << t.format(time::http) << "\n" << t.format(time::email) << "\n";

    // A pattern, and txt::format
    std::cout << t.format("%A, %d %B %Y, %H:%M %Z") << "\n";
    std::cout << txt::format("[{:%H:%M}] [{:>12%F}] [{}] [{:%a}]", t, t.date(), t.weekday(), t.weekday()) << "\n";
    auto sys = std::chrono::sys_seconds(std::chrono::seconds(t.unix()));
    std::cout << txt::format("{} {:%T} {}", sys, std::chrono::milliseconds(90500), std::chrono::minutes(90)) << "\n";

    // Reading the date of HTTP in its three forms
    for (auto text : {"Sun, 06 Nov 1994 08:49:37 GMT", "Sunday, 06-Nov-94 08:49:37 GMT", "Sun Nov  6 08:49:37 1994"}) {
        std::cout << time::datetime::parse(text, time::http).value() << "\n";
    }

    // RFC 3339, ISO 8601, a pattern in a zone
    std::cout << time::datetime::parse("1990-12-31T15:59:60-08:00", time::rfc3339).value() << "\n";
    std::cout << time::datetime::parse("2026-W39-4T12:41,5+02", time::iso8601).value() << "\n";
    std::cout << time::datetime::parse("25.10.2026 02:30 CET", "%d.%m.%Y %H:%M %Z", warsaw).value() << "\n";
    std::cout << time::date::parse("September 24, 2026", "%B %d, %Y").value() << "\n";

    // What is not a date says why and where
    auto bad = time::datetime::parse("Sun, 31 Feb 1994 08:49:37 GMT", time::http);
    std::cout << bad.error().message() << " (byte " << bad.error().offset() << ")\n";
    return 0;
}
```

Output:

```text
2026-09-24T12:41:15+02:00
2026-09-24T12:41:15.122575+02:00
Thu, 24 Sep 2026 10:41:15 GMT
Thu, 24 Sep 2026 12:41:15 +0200
Thursday, 24 September 2026, 12:41 CEST
[12:41] [  2026-09-24] [Thursday] [Thu]
2026-09-24 10:41:15 00:01:30.500 90min
1994-11-06T08:49:37Z
1994-11-06T08:49:37Z
1994-11-06T08:49:37Z
1990-12-31T16:00:00-08:00
2026-09-24T12:41:30+02:00
2026-10-25T02:30:00+01:00
2026-09-24
a day that the month has expected (byte 5)
```

## See also

- [datetime](datetime.md), [date](date.md), [zone](zone.md)
- [txt::format](../txt/format.md): the patterns of values in text
- [duration](../core/duration.md): a span of time and Go's text of it
