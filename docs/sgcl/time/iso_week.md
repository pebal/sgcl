[sgcl](../README.md) › [time](README.md)

# sgcl::time::iso_week

```cpp
#include "sgcl/time/date.h"   // or "sgcl/time.h"

namespace sgcl::time {
    struct iso_week {
        int year = 0;
        int week = 0;
    };
}
```

`sgcl::time::iso_week` is a week of ISO 8601 and the year it belongs to: what a [date](date/iso_week.md) and a
[datetime](datetime.md) answer with, as Go's `ISOWeek()` answers with two numbers. The weeks start on Monday, and
week 1 is the one with the year's first Thursday, so the few days around New Year may belong to a week of the year
next to theirs: 2024-12-30 is in week 1 of 2025. A year has 52 weeks, or 53 when it starts on a Thursday, or on a
Wednesday in a leap year: 2026 has 53. The two fields are taken apart as Go's two results are,
`auto [year, week] = d.iso_week()`.

## Rules

- An aggregate of two `int`s, trivially copyable: it lives anywhere.
- Two weeks compare with `==` and `!=`; there is no order.
- The year may be the year before or after the date's; for the first two days of a date's calendar,
  -32767-01-01 and -32767-01-02, it is -32768, outside it.

## Member objects

| Field | Description |
|---|---|
| `year` | the year the week belongs to; 0 by default |
| `week` | the week of that year, 1 to 52 or 53; 0 by default |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](iso_week/operator_cmp.md) | compares two weeks |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include <type_traits>

using namespace sgcl;

int main() {
    auto [year, week] = time::date(2024, 12, 30).iso_week();
    println("week {} of {}", week, year);
    time::iso_week last = time::date(2026, 12, 31).iso_week();
    println("{} {}", last.week, last == time::iso_week{2026, 53});
    println("{}", std::is_trivially_copyable_v<time::iso_week>);
}
```

Output:

```text
week 1 of 2025
53 true
true
```

## See also

- [date::iso_week](date/iso_week.md): the week of a date
- [weekday](weekday.md): the day of the week, numbered as ISO 8601 numbers it
- [time](README.md)
