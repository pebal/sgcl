[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::format

```cpp
string format(layout format) const noexcept;            // (1)
string format(const string& pattern) const noexcept;    // (2)
```

The text of the datetime, Go's `t.Format`.

1. In a [layout](../layout/README.md), a format known by name, written by code of its own with no pattern walked:
   `time::rfc3339` (`2026-09-24T12:41:15+02:00`, `Z` for an offset of zero, Go's `RFC3339`), `time::rfc3339_nano`
   (the same with the fraction of a second, its trailing zeros left out, Go's `RFC3339Nano`), `time::http` (RFC 9110's
   IMF-fixdate, always in GMT: `Thu, 24 Sep 2026 10:41:15 GMT`), `time::email` (RFC 5322, in the datetime's zone:
   `Thu, 24 Sep 2026 12:41:15 +0200`) and `time::iso8601` (as `rfc3339_nano`).
2. By a [pattern](../README.md#patterns) of `%` specifiers, the language of `std::format` for `<chrono>` (and of C's
   `strftime` before it): `t.format("%d.%m.%Y %H:%M")`. Every specifier of C++20 is there, written as libc++'s
   `std::format` writes it, byte for byte, the names in English. A specifier it does not know, or one the value does
   not answer (`%Q`), is written as it stands: the writer never fails. `%S` and `%T` write the fraction of a second
   the datetime holds, nine digits (`12:41:15.000000000`); a text to the second is `%X`, or a layout: the header of
   HTTP is `t.format(time::http)`. `%Z` writes the zone's [abbreviation](abbreviation.md) (`CEST`, `UTC`, a fixed
   offset's name), `%z` the offset `+0200`, `%Ez` and `%Oz` `+02:00`; the seconds of an offset are dropped.

## Parameters

| Parameter | Description |
|---|---|
| `format` | the layout: `time::rfc3339`, `rfc3339_nano`, `http`, `email` or `iso8601` |
| `pattern` | the pattern, `%` specifiers and the characters written as they are between them |

## Return value

The text.

## Complexity

- (1) Constant, plus the look-up of the instant in the zone's changes.
- (2) Linear in the length of `pattern`, plus the look-up of the instant in the zone's changes.

## Exceptions

None.

## Notes

In [txt::format](../../txt/format.md) a datetime is a value like any other
([README: Formatting with txt](../README.md#formatting-with-txt)): `{}` writes its [to_string](to_string.md), a
pattern after the colon writes as (2) does (`{:%H:%M}`), and a width pads the whole (`{:>40}`). The pattern is
checked where the program is compiled: `{:%Q}` of a datetime does not compile. `txt::format_to` writes the text into
a caller's buffer, nothing allocated. The types of `<chrono>` the module
writes too — `sys_time` and `local_time` of an integral duration, `year_month_day`, `weekday`, `hh_mm_ss`,
`duration` — are written as `std::format` writes them, so code that holds a `system_clock::time_point` hands it to
`txt::format` as it is; `%T` of a `sys_seconds` has no fraction.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include <chrono>

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    auto t = time::date(2026, 9, 24).at(12, 41, 15, warsaw) + 122575 * microsecond;

    // The layouts
    println(t.format(time::rfc3339));
    println(t.format(time::rfc3339_nano));
    println(t.format(time::http));
    println(t.format(time::email));

    // Patterns
    println(t.format("%A, %d %B %Y, %H:%M %Z"));
    println("{} | {} | {} | {}", t.format("%T"), t.format("%X"), t.format("%z %Ez"),
            t.format("%H %Q"));

    // txt::format, of a datetime and of the types of <chrono>
    println("[{:%H:%M}] [{:>12%F}] [{}] [{:%a}]", t, t.date(), t.weekday(), t.weekday());
    auto sys = std::chrono::sys_seconds(std::chrono::seconds(t.unix()));
    println("{} {:%T} {}", sys, std::chrono::milliseconds(90500), std::chrono::minutes(90));
}
```

Output:

```text
2026-09-24T12:41:15+02:00
2026-09-24T12:41:15.122575+02:00
Thu, 24 Sep 2026 10:41:15 GMT
Thu, 24 Sep 2026 12:41:15 +0200
Thursday, 24 September 2026, 12:41 CEST
12:41:15.122575000 | 12:41:15 | +0200 +02:00 | 12 %Q
[12:41] [  2026-09-24] [Thursday] [Thu]
2026-09-24 10:41:15 00:01:30.500 90min
```

## See also

- [parse](parse.md): reads the text back
- [to_string](to_string.md): RFC 3339 with the fraction where there is one
- [layout](../layout/README.md): the formats known by name, what each writes and reads
- [README: Patterns](../README.md#patterns): every specifier
- [sgcl::time::datetime](README.md)
