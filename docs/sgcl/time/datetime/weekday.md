[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::weekday

```cpp
time::weekday weekday() const noexcept;
```

The day of the week the zone's clock shows at the instant, Go's `t.Weekday()`: a [weekday](../weekday.md) in ISO's
numbering, Monday 1 to Sunday 7 (Go's Sunday is 0). It is written as its English name.

## Parameters

None.

## Return value

The day of the week.

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
    println("{} {}", t.weekday(), int(t.weekday()));
    println(t.in(time::zone("Asia/Tokyo")).weekday());
}
```

Output:

```text
Thursday 4
Friday
```

## See also

- [weekday](../weekday.md): the enumeration
- [iso_week](iso_week.md): the week of ISO 8601
- [sgcl::time::datetime](../datetime.md)
