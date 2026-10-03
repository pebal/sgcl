[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::second

```cpp
int second() const noexcept;
```

The second the zone's clock shows at the instant, Go's `t.Second()`: 0 to 59. A leap second read from a text is
the first instant of the next day, so 60 is never shown.

## Parameters

None.

## Return value

The second, 0 to 59.

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
    println("{} {}", t.second(), (t + 1500 * millisecond).second());
}
```

Output:

```text
15 16
```

## See also

- [hour](hour.md), [minute](minute.md), [nanosecond](nanosecond.md): the rest of the time of day
- [sgcl::time::datetime](../datetime.md)
