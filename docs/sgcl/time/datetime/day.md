[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::day

```cpp
int day() const noexcept;
```

The day of the month the zone's clock shows at the instant, Go's `t.Day()`: 1 to 31.

## Parameters

None.

## Return value

The day of the month, 1 to 31.

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
    println("{} {}", t.day(), t.in(time::zone("Asia/Tokyo")).day());
}
```

Output:

```text
24 25
```

## See also

- [year](year.md), [month](month.md): the rest of the date
- [year_day](year_day.md): the day of the year
- [sgcl::time::datetime](../datetime.md)
