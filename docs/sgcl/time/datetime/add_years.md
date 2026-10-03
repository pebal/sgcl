[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::add_years

```cpp
datetime add_years(int n) const noexcept;
```

The same time of the clock `n` years on (back for a negative `n`), in the same zone, with the same part of a second.
The 29th of February in a year without one is cut to the 28th, as a date's [add_years](../date/add_years.md) cuts
it; Go's `t.AddDate(n, 0, 0)` carries it to the 1st of March. A time of the clock that the new day does not have, or
has twice, is read by the compatible rule
([A time of the clock skipped or shown twice](README.md#a-time-of-the-clock-skipped-or-shown-twice)). A result
past either end of the range is the end; Go wraps.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the years to add, negative to go back |

## Return value

The datetime `n` years on, saturated at the ends of the range.

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
    auto leap = time::date(2028, 2, 29).at(9, 0, time::zone::utc());
    println("{} {}", leap.add_years(1), leap.add_years(4));
    println(leap.add_years(1000));  // past the range: its end
}
```

Output:

```text
2029-02-28T09:00:00Z 2032-02-29T09:00:00Z
2262-04-11T23:47:16.854775807Z
```

## See also

- [add_days](add_days.md), [add_months](add_months.md): days and months on
- [date::add_years](../date/add_years.md): years on of a date
- [sgcl::time::datetime](README.md)
