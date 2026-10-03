[sgcl](../../README.md) › [time](../README.md) › [date](README.md)

# sgcl::time::date::date

```cpp
constexpr date() noexcept = default;                              // (1)
constexpr date(int year, int month, int day) noexcept;            // (2)
constexpr date(int year, time::month month, int day) noexcept;    // (3)
constexpr date(std::chrono::year_month_day ymd) noexcept;         // (4)
constexpr date(std::chrono::sys_days days) noexcept;              // (5)
explicit date(const string& text);                                // (6)
explicit date(const string& text, const string& pattern);         // (7)
```

Constructs a date.

1. 1970-01-01.
2. The date of a year, a month and a day, carried as Go's `time.Date` carries: a day or a month outside its range
   carries into the next, so `date(2026, 2, 30)` is 2026-03-02, `date(2026, 13, 1)` is 2027-01-01,
   `date(2026, 3, 0)` is the last day of February and `date(2026, 1, 267)` the 267th day of 2026. A date past
   either end of the calendar, -32767-01-01 and 32767-12-31, is that end.
3. The same, the month named: `date(2026, time::month::september, 25)`.
4. From the calendar of `<chrono>`, implicitly, so that code written with `year_month_day` passes one on as it is
   and a date compares with one (`d == 2026y / 9 / 24`). A `year_month_day` that is not `ok()` because its day is
   beyond its month is carried as in (2).
5. From a `std::chrono::sys_days`, implicitly: the days from 1970-01-01, which is what a date holds. A point
   beyond the calendar is its end.
6. From a literal the program itself spells, in ISO 8601 (`date("2026-09-24")`): what [parse](parse.md) reads, or
   `bad_expected_access<time::error>` with `parse`'s message.
7. From a literal in a [pattern](../README.md#patterns) of `%` (`date("24.09.2026", "%d.%m.%Y")`): what
   `parse(text, pattern)` reads, or `bad_expected_access<time::error>` with its message.

A text from outside the program (a setting, the user, a file) is parsed, and its error is a value; a text the
program itself writes is constructed. The conversions back to `year_month_day` and `sys_days` are explicit
([operator_conv](operator_conv.md)), so that a comparison of a date with one of them has a single way to go.

## Parameters

| Parameter | Description |
|---|---|
| `year` | the year, 1 BC being year 0 |
| `month` | the month, 1 to 12 or a [month](../month.md); carried when outside |
| `day` | the day of the month; carried when outside the month |
| `ymd` | the date of the calendar of `<chrono>` |
| `days` | the days from 1970-01-01 |
| `text` | the date's text |
| `pattern` | the pattern the text is in |

## Complexity

- (1–5) Constant.
- (6–7) Linear in the length of `text` (and of `pattern`).

## Exceptions

- (1–5) None.
- (6–7) `bad_expected_access<time::error>` when `text` is not a date; its `error()` is the [error](../error/README.md) of
  `parse`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include <chrono>

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    time::date epoch;
    time::date day(2026, time::month::september, 24);
    println("{} {}", epoch, day);
    println("{} {} {} {}", time::date(2026, 2, 30), time::date(2026, 13, 1), time::date(2026, 3, 0),
            time::date(2026, 1, 267));

    time::date from_chrono = 2026y / 2 / 30;  // not ok(), carried
    println("{} {}", from_chrono, time::date(40000, 1, 1));

    time::date spelled("2026-09-24"), patterned("24.09.2026", "%d.%m.%Y");
    println("{} {}", spelled == day, patterned == day);
    try {
        time::date wrong("2026-02-30");
    } catch (const bad_expected_access<time::error>& e) {
        println("{} (byte {})", e.error().message(), e.error().offset());
    }
}
```

Output:

```text
1970-01-01 2026-09-24
2026-03-02 2027-01-01 2026-02-28 2026-09-24
2026-03-02 32767-12-31
true true
a day that the month has expected (byte 8)
```

## See also

- [parse](parse.md): reads a text from outside the program
- [is_valid](is_valid.md): whether numbers need no carrying
- [operator std::chrono::year_month_day](operator_conv.md): the conversions back
- [sgcl::time::date](README.md)
