[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::add_days

```cpp
datetime add_days(int n) const noexcept;
```

The same time of the clock `n` days on (back for a negative `n`), in the same zone, with the same part of a second:
the calendar's arithmetic, the days of Go's `t.AddDate(0, 0, n)`. Across a change of the clock a day is 23 or 25
hours; `t + 24 * hour` is the exact arithmetic. A time of the clock that the new day does not have, or has twice, is
read by the compatible rule, as a date's [at](../date/at.md) reads it
([A time of the clock skipped or shown twice](../datetime.md#a-time-of-the-clock-skipped-or-shown-twice)): 02:30 a
day before the night the clock skips 02:00 to 03:00 is 03:30 a day later. A result past either end of the range is
the end.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the days to add, negative to go back |

## Return value

The datetime `n` days on, saturated at the ends of the range.

## Complexity

Logarithmic in the number of the zone's changes; constant in UTC and in a fixed zone.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    auto before = time::date(2026, 10, 24).at(12, 0, warsaw);
    println("{} {}", before.add_days(1), before.add_days(1) - before);
    println("{} {}", before + 24 * hour, before.add_days(-1));
    println(time::date(2026, 3, 28).at(2, 30, warsaw).add_days(1));  // 02:30 skipped
}
```

Output:

```text
2026-10-25T12:00:00+01:00 25h0m0s
2026-10-25T11:00:00+01:00 2026-10-23T12:00:00+02:00
2026-03-29T03:30:00+02:00
```

## See also

- [add_months](add_months.md), [add_years](add_years.md): months and years on
- [operator+](operator_arith.md): the exact arithmetic
- [date::add_days](../date/add_days.md): days on of a date
- [sgcl::time::datetime](../datetime.md)
