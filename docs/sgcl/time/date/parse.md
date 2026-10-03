[sgcl](../../README.md) › [time](../README.md) › [date](../date.md)

# sgcl::time::date::parse

```cpp
/*(1)*/ static expected<date, error> parse(const string& text) noexcept;
/*(2)*/ static expected<date, error> parse(const string& text, const string& pattern) noexcept;
```

Reads a date from a text.

1. ISO 8601: the three forms of a date, each extended or basic — the calendar date (`"2026-09-24"`,
   `"20260924"`), the week date (`"2026-W39-4"`, `"2026W394"`) and the ordinal date (`"2026-267"`, `"2026267"`). The
   year has four digits, or five in the extended forms, and a sign or none (`"-0044-03-15"`, `"+10000-01-01"`);
   `-0000`, which ISO 8601 forbids, is refused, and year zero is `0000`.
2. A date in a [pattern](../README.md#patterns) of `std::format`'s specifiers for `<chrono>`, read as
   `std::chrono::parse` reads one: `"%d.%m.%Y"`, `"%B %e, %Y"`, `"%G-W%V-%u"`. The pattern needs a date — by year,
   month and day, by a day of the year, or by a week and a day of it — and the fields read must agree (a day of the
   week that is not the date's is an error); a specifier of a time of day or of a zone is refused, a date having
   neither.

- (1–2) The date must exist (not `"2026-02-30"`) and be the whole text: `"2026-09-24T10:00"` is refused.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to read |
| `pattern` | the pattern the text is in |

## Return value

The date, or an [error](../error.md) with a sentence and the byte of the text where the field that failed starts.
Of (1):

- `"a year of four digits expected"`, `"a year of four or five digits expected"`,
  `"a date expected: 2026-09-24, 2026-W39-4 or 2026-267"`: the year or the shape of the date;
- `"a year of zero has no minus sign"`: `-0000`;
- `"a year from -32767 to 32767 expected"`,
  `"a date from -32767-01-01 to 32767-12-31 expected"`: a date outside the calendar;
- `"a month from 01 to 12 expected"`, `"a day that the month has expected"`,
  `"a hyphen and a day of the month expected"`: the calendar date;
- `"a week from 01 to 52, or 53 in a long year, expected"`, `"a day of the week from 1 to 7 expected"`,
  `"a hyphen and a day of the week expected"`: the week date;
- `"a day of the year from 001 to 365 expected"` (or `366` in a leap year): the ordinal date;
- `"the end of the date expected"`: more text after the date.

Of (2), the sentence of the field that failed, among them
`"a specifier of a time of day or of a zone in the pattern of a date"`, `"a day of the week that is not the date's"`
and `"the end of the text expected"`.

## Complexity

Linear in the length of `text` (and of `pattern`).

## Exceptions

None.

## Notes

A text from outside the program (a setting, the user, a file) is parsed, and its error is a value; a text the
program itself writes is constructed, `time::date d("2026-09-24")` ([constructor](date.md)), and a wrong one throws.
The first two days of the calendar, -32767-01-01 and -32767-01-02, belong to a week of the year -32768, outside
it: they have no week date that (1) reads.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    for (const char* text : {"2026-09-24", "2026W394", "2026-267", "-0044-03-15", "+10000-01-01"}) {
        println("{} -> {}", text, time::date::parse(text).value());
    }
    for (const char* text : {"2026-02-29", "2026-09-24T10:00", "-0000-01-01", "2026-W54-1"}) {
        auto d = time::date::parse(text);
        println("{}: {} (byte {})", text, d.error().message(), d.error().offset());
    }

    println(time::date::parse("September 24, 2026", "%B %e, %Y").value());
    println(time::date::parse("2026-W39-4", "%G-W%V-%u").value());
    auto timed = time::date::parse("24.09.2026 10:00", "%d.%m.%Y %H:%M");
    println("{} (byte {})", timed.error().message(), timed.error().offset());
}
```

Output:

```text
2026-09-24 -> 2026-09-24
2026W394 -> 2026-09-24
2026-267 -> 2026-09-24
-0044-03-15 -> -0044-03-15
+10000-01-01 -> 10000-01-01
2026-02-29: a day that the month has expected (byte 8)
2026-09-24T10:00: the end of the date expected (byte 10)
-0000-01-01: a year of zero has no minus sign (byte 0)
2026-W54-1: a week from 01 to 52, or 53 in a long year, expected (byte 6)
2026-09-24
2026-09-24
a specifier of a time of day or of a zone in the pattern of a date (byte 11)
```

## See also

- [to_string](to_string.md), [format](format.md): write the text
- [(constructor)](date.md): a date from a literal text
- [error](../error.md): why a text is not a date
- [Patterns](../README.md#patterns): the specifiers of `%` and how they are read
- [sgcl::time::date](../date.md)
