[sgcl](../README.md) › [time](README.md)

# sgcl::time::layout

```cpp
#include "sgcl/time/layout.h"   // or "sgcl/time.h"

namespace sgcl::time {
    class layout;

    inline constexpr layout rfc3339 = /* unspecified */;
    inline constexpr layout rfc3339_nano = /* unspecified */;
    inline constexpr layout http = /* unspecified */;
    inline constexpr layout email = /* unspecified */;
    inline constexpr layout iso8601 = /* unspecified */;
}
```

`sgcl::time::layout` is a format of a time known by its name, one of a closed set: RFC 3339, the date of HTTP, the
date of e-mail and ISO 8601. Each is written and read by code of its own, with no pattern walked:
`t.format(time::http)` ([format](datetime/format.md)) writes it, `datetime::parse(text, time::http)`
([parse](datetime/parse.md)) reads it, and a literal of the program is constructed,
`time::datetime t("2026-09-24T12:41:15+02:00", time::rfc3339)` ([constructor](datetime/datetime.md)), which throws
`parse`'s error. The code is written from RFC 3339, RFC 9110 5.6.7 and RFC 5322 3.3.

A layout writes what its RFC asks for and reads what it allows, the obsolete forms a recipient must take
included. The other way to say how a time is written and read is a [pattern](README.md#patterns) of `%`, the
language of `std::format` for `<chrono>`.

## Rules

- A layout is one byte, trivially copyable, `constexpr`: nothing to build and nothing to check where it runs. The
  five constants are all there are; a program does not make one.
- What each layout writes, and what it reads besides what it writes:

| Layout | Written | Read as well |
|---|---|---|
| `rfc3339` | `2026-09-24T12:41:15+02:00`: RFC 3339 to the second, in the datetime's zone, `Z` for an offset of zero (Go's `RFC3339`) | a fraction of a second of any length (the nanoseconds kept), `t` and `z` in small letters (RFC 3339 5.6), a leap second at 23:59:60 UTC |
| `rfc3339_nano` | `2026-09-24T12:41:15.122575+02:00`: the same with the fraction of a second, its trailing zeros left out and none where it is zero (Go's `RFC3339Nano`) | as `rfc3339` |
| `http` | `Thu, 24 Sep 2026 10:41:15 GMT`: RFC 9110's IMF-fixdate, always in GMT, what `Date:`, `Expires:`, `Last-Modified:` and a cookie's `Expires` carry | the two obsolete forms a recipient must take: RFC 850's (`Thursday, 24-Sep-26 10:41:15 GMT`) and asctime's (`Thu Sep 24 10:41:15 2026`, the day padded with a space or a nought); names in any case; the day of the week is read and not checked against the date, as Go and the recipients RFC 9110 has in mind do |
| `email` | `Thu, 24 Sep 2026 12:41:15 +0200`: RFC 5322, in the datetime's zone | the obsolete forms of RFC 5322: no day of the week, no seconds, a year of two digits or three (1900 on), the zone as one of ten names (`UT`, `GMT`, `EST`, `EDT`, `CST`, `CDT`, `MST`, `MDT`, `PST`, `PDT`) or a military letter, which is `-0000`, UTC; comments in parentheses and folding white space between the parts |
| `iso8601` | as `rfc3339_nano` | the broad profile of ISO 8601: the three forms of a date (`2026-09-24`, `2026-W39-4`, `2026-267`), each basic or extended (`20260924`, `2026W394`, `2026267`); a date alone (its midnight), or with `T` and a time of hours, of hours and minutes or of all three, basic or extended (`12`, `1241`, `124115`, `12:41`, `12:41:15`); a fraction on the last of them with a point or a comma (`12:41:15,5`; `12.5` is half past twelve); `24:00` for the end of a day; an offset `Z`, `±hh`, `±hhmm` or `±hh:mm`, UTC where there is none |

- A second of 60 is read only where it is the last second of a day in UTC, a leap second, and then as the first
  instant of the next day (`timegm`'s reading); RFC 3339 shows such times. Anywhere else it is an error.
- A year of two digits: in RFC 850's dates of HTTP, the latest year with those digits not more than 50 years ahead
  of [time::now()](now.md) (RFC 9110); in e-mail, 1950 to 2049 (RFC 5322 4.3).
- Every refusal is an [error](error.md) with a sentence and the byte of the text where the field that failed
  starts.
- Where this reads otherwise than Go, by design: Go takes one digit where the RFCs ask for two, any abbreviation
  where RFC 9110 asks for GMT and where RFC 5322 lists ten names (reading `EST` as UTC), offsets past 23:59, and
  text after an e-mail date; Go refuses a leap second, `t` and `z` in small letters, and RFC 5322's obsolete forms;
  Go's two-digit years pivot at 1969 everywhere.

### From code written for Go

| With Go | With sgcl::time |
|---|---|
| `time.RFC3339` | `time::rfc3339` |
| `time.RFC3339Nano` | `time::rfc3339_nano` |
| `time.RFC1123Z` | `time::email` |
| `http.TimeFormat`, `http.ParseTime` | `time::http`, which reads RFC 850 and asctime too |
| `net/mail.ParseDate` | `time::email`, which reads RFC 5322's obsolete forms |
| — | `time::iso8601`, which Go has not |
| `time.RFC1123` | the pattern `"%a, %d %b %Y %T %Z"` |
| `time.RFC822` | the pattern `"%d %b %y %H:%M %Z"` |
| `time.RFC850` | the pattern `"%A, %d-%b-%y %T %Z"` |
| `time.ANSIC` | the pattern `"%c"` |
| `time.Kitchen` | the pattern `"%I:%M%p"` |
| `time.Stamp` | the pattern `"%b %e %T"` |

Of the [patterns](README.md#patterns), `%T` writes the fraction of a second a datetime holds, where Go's layouts
stop at the second, and `%X` in its place writes to the second; `%I` writes the hour with a nought, `03:04PM`
where Go's `Kitchen` writes `3:04PM`.

## Non-member functions

| Function | Description |
|---|---|
| [operator==](layout/operator_cmp.md) | whether two layouts are the same |

#### Constants

| Constant | Description |
|---|---|
| `rfc3339` | RFC 3339 to the second (Go's `RFC3339`), `inline constexpr layout` |
| `rfc3339_nano` | RFC 3339 with the fraction of a second (Go's `RFC3339Nano`) |
| `http` | the date of HTTP, RFC 9110 5.6.7, in GMT |
| `email` | the date of e-mail, RFC 5322 3.3 |
| `iso8601` | written as `rfc3339_nano`, read in the broad profile of ISO 8601 |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    auto t = time::date(2026, 9, 24).at(12, 41, 15, warsaw) + 122575 * microsecond;
    for (time::layout format : {time::rfc3339, time::rfc3339_nano, time::http, time::email,
                                time::iso8601}) {
        println(t.format(format));
    }

    // The three forms of the date of HTTP, as a header brings it
    for (const char* text : {"Sun, 06 Nov 1994 08:49:37 GMT", "Sunday, 06-Nov-94 08:49:37 GMT",
                             "Sun Nov  6 08:49:37 1994"}) {
        println(time::datetime::parse(text, time::http).value());
    }
}
```

Output:

```text
2026-09-24T12:41:15+02:00
2026-09-24T12:41:15.122575+02:00
Thu, 24 Sep 2026 10:41:15 GMT
Thu, 24 Sep 2026 12:41:15 +0200
2026-09-24T12:41:15.122575+02:00
1994-11-06T08:49:37Z
1994-11-06T08:49:37Z
1994-11-06T08:49:37Z
```

## See also

- [datetime::format](datetime/format.md), [datetime::parse](datetime/parse.md): write and read a layout
- [patterns](README.md#patterns): the other way, a pattern of `%`
- [error](error.md): why a text is not a time
- [sgcl::time](README.md)
