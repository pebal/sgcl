[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::minute

```cpp
int minute() const noexcept;
```

The minute the zone's clock shows at the instant, Go's `t.Minute()`: 0 to 59. A zone whose offset is not
whole hours shows other minutes than UTC.

## Parameters

None.

## Return value

The minute, 0 to 59.

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
    auto t = time::date(2026, 9, 24).at(12, 41, 15, time::zone("Europe/Warsaw"));
    println("{} {}", t.minute(), t.in(time::zone("Asia/Kolkata")).minute());
}
```

Output:

```text
41 11
```

## See also

- [hour](hour.md), [second](second.md), [nanosecond](nanosecond.md): the rest of the time of day
- [sgcl::time::datetime](../datetime.md)
