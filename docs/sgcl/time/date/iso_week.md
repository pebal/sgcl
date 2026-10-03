[sgcl](../../README.md) › [time](../README.md) › [date](../date.md)

# sgcl::time::date::iso_week

```cpp
constexpr time::iso_week iso_week() const noexcept;
```

The week of ISO 8601 the date is in, and the year that week belongs to, an [iso_week](../iso_week.md): what Go's
`ISOWeek()` returns. The weeks start on Monday, and week 1 is the one with the year's first Thursday, so the few
days around New Year may belong to a week of the year next to theirs: 2024-12-30 is in week 1 of 2025, and
2027-01-01 in week 53 of 2026, a year of 53 weeks.

## Parameters

None.

## Return value

The year the week belongs to, which may be the year before or after `year()`, and the week, 1 to 52 or 53.

## Complexity

Constant.

## Exceptions

None.

## Notes

The first two days of the calendar, -32767-01-01 and -32767-01-02, are in a week of the year -32768, outside it:
those two have no week date that [parse](parse.md) reads.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto [year, week] = time::date(2026, 9, 24).iso_week();
    println("week {} of {}", week, year);
    for (time::date d : {time::date(2024, 12, 30), time::date(2027, 1, 1)}) {
        println("{}: week {} of {}", d, d.iso_week().week, d.iso_week().year);
    }
}
```

Output:

```text
week 39 of 2026
2024-12-30: week 1 of 2025
2027-01-01: week 53 of 2026
```

## See also

- [weekday](weekday.md): the day of the week
- [iso_week](../iso_week.md): the year and the week
- [sgcl::time::date](../date.md)
