[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::date

```cpp
time::date date() const noexcept;
```

The date the zone's clock shows at the instant, a [date](../date/README.md) with no time of day and no zone: Go's
`t.Date()` as one value. The same instant may be on two dates in two zones.

## Parameters

None.

## Return value

The date of the zone's clock.

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
    auto t = time::date(2026, 9, 24).at(23, 30, time::zone("Europe/Warsaw"));
    println("{} {}", t.date(), t.in(time::zone("Asia/Tokyo")).date());
    println(t.date().add_days(1));
}
```

Output:

```text
2026-09-24 2026-09-25
2026-09-25
```

## See also

- [year](year.md), [month](month.md), [day](day.md): the date's fields one by one
- [start_of_day](start_of_day.md): the first instant of the date
- [sgcl::time::datetime](README.md)
