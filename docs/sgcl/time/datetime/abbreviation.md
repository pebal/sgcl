[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::abbreviation

```cpp
string abbreviation() const noexcept;
```

The abbreviation of the zone at the instant, the first half of Go's `t.Zone()`: `"CEST"` in Warsaw in summer,
`"CET"` in winter, `"UTC"` in UTC. A fixed zone has none of its own and answers with its name, `"+01:30"`. It is what
the specifier `%Z` of a [pattern](../README.md#patterns) writes.

## Parameters

None.

## Return value

The abbreviation, or the zone's name where it has none.

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
    println("{} {}", time::date(2026, 7, 1).at(12, 0, warsaw).abbreviation(),
            time::date(2026, 12, 1).at(12, 0, warsaw).abbreviation());
    println("{} {}", time::datetime().abbreviation(),
            time::date(2026, 7, 1).at(12, 0, time::zone::fixed(90 * minute)).abbreviation());
}
```

Output:

```text
CEST CET
UTC +01:30
```

## See also

- [offset](offset.md), [is_dst](is_dst.md): the rest of what the zone is at the instant
- [zone::abbreviation_at](../zone/abbreviation_at.md): the same asked of a zone
- [sgcl::time::datetime](README.md)
