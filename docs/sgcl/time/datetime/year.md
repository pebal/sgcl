[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::year

```cpp
int year() const noexcept;
```

The year the zone's clock shows at the instant, Go's `t.Year()`: 1677 to 2262, the range of a datetime.

## Parameters

None.

## Return value

The year.

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
    auto t = time::date(2026, 12, 31).at(23, 30, time::zone::utc());
    println("{} {}", t.year(), t.in(time::zone("Europe/Warsaw")).year());
}
```

Output:

```text
2026 2027
```

## See also

- [month](month.md), [day](day.md): the rest of the date
- [date](date.md): the date as one value
- [sgcl::time::datetime](README.md)
