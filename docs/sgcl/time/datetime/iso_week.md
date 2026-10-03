[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::iso_week

```cpp
time::iso_week iso_week() const noexcept;
```

The week of ISO 8601 the zone's clock shows at the instant, Go's `t.ISOWeek()`: an [iso_week](../iso_week.md), the
year the week belongs to and the week, 1 to 52 or 53. A week belongs to the year of its Thursday, so the first days of
January may be in the last week of the year before.

## Parameters

None.

## Return value

The year of the week and the week.

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
    auto t = time::date(2026, 9, 24).at(12, 0, time::zone("Europe/Warsaw"));
    println("{} {}", t.iso_week().year, t.iso_week().week);
    auto w = time::date(2027, 1, 1).at(12, 0, time::zone("Europe/Warsaw")).iso_week();
    println("{} {}", w.year, w.week);
}
```

Output:

```text
2026 39
2026 53
```

## See also

- [iso_week](../iso_week.md): the type
- [weekday](weekday.md): the day of the week
- [sgcl::time::datetime](../datetime.md)
