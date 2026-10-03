[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::add_months

```cpp
datetime add_months(int n) const noexcept;
```

The same time of the clock `n` months on (back for a negative `n`), in the same zone, with the same part of a
second. A day the new month does not have is cut to the month's last day, as a date's
[add_months](../date/add_months.md) cuts it: 2026-01-31 plus one month is 2026-02-28. Go's `t.AddDate(0, n, 0)`
carries instead (31 January plus a month is 3 March there). A time of the clock that the new day does not have, or
has twice, is read by the compatible rule
([A time of the clock skipped or shown twice](README.md#a-time-of-the-clock-skipped-or-shown-twice)). A result
past either end of the range is the end; Go wraps.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the months to add, negative to go back |

## Return value

The datetime `n` months on, saturated at the ends of the range.

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
    auto invoice = time::date(2026, 1, 31).at(10, 0, time::zone("Europe/Warsaw"));
    println("{} {}", invoice.add_months(1), invoice.add_months(-2));
    println(invoice.add_months(13));
}
```

Output:

```text
2026-02-28T10:00:00+01:00 2025-11-30T10:00:00+01:00
2027-02-28T10:00:00+01:00
```

## See also

- [add_days](add_days.md), [add_years](add_years.md): days and years on
- [date::add_months](../date/add_months.md): months on of a date
- [sgcl::time::datetime](README.md)
