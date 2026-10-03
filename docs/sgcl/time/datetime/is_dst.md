[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::is_dst

```cpp
bool is_dst() const noexcept;
```

Whether the zone is on daylight saving time at the instant, Go's `t.IsDST()`: `true` in Warsaw in summer, `false` in
winter, in UTC and in a fixed zone.

## Parameters

None.

## Return value

`true` on daylight saving time.

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
    time::zone warsaw("Europe/Warsaw");
    println("{} {}", time::date(2026, 7, 1).at(12, 0, warsaw).is_dst(),
            time::date(2026, 12, 1).at(12, 0, warsaw).is_dst());
}
```

Output:

```text
true false
```

## See also

- [offset](offset.md), [abbreviation](abbreviation.md): the rest of what the zone is at the instant
- [zone::is_dst_at](../zone/is_dst_at.md): the same asked of a zone
- [sgcl::time::datetime](../datetime.md)
