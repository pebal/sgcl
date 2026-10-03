[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::hour

```cpp
int hour() const noexcept;
```

The hour the zone's clock shows at the instant, Go's `t.Hour()`: 0 to 23.

## Parameters

None.

## Return value

The hour, 0 to 23.

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
    println("{} {}", t.hour(), t.utc().hour());
}
```

Output:

```text
12 10
```

## See also

- [minute](minute.md), [second](second.md), [nanosecond](nanosecond.md): the rest of the time of day
- [sgcl::time::datetime](README.md)
