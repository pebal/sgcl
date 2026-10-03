[sgcl](../../README.md) › [time](../README.md) › [zone](README.md)

# sgcl::time::zone::utc

```cpp
static zone utc() noexcept;
```

UTC: the same as a default-constructed zone, the null pointer, nothing allocated. Its name and its abbreviation
are `"UTC"`, its offset is zero, it is never daylight saving time and it has no transitions. `load("UTC")` and
`fixed` of zero are this zone.

## Parameters

None.

## Return value

The zone UTC.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone utc = time::zone::utc();
    auto t = time::date(2026, 7, 1).at(12, 0, utc);
    println("{} {} {}", utc.name(), utc.abbreviation_at(t), utc.offset_at(t));
    println("{} {}", utc == time::zone(), utc.next_transition(t).has_value());
    println("{} {}", time::zone::load("UTC").value() == utc, time::zone::fixed(0 * second) == utc);
}
```

Output:

```text
UTC UTC 0s
true false
true true
```

## See also

- [fixed](fixed.md): another offset
- [(constructor)](zone.md): a default-constructed zone
- [sgcl::time::zone](README.md)
