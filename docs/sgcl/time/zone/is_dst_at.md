[sgcl](../../README.md) › [time](../README.md) › [zone](../zone.md)

# sgcl::time::zone::is_dst_at

```cpp
bool is_dst_at(const datetime& t) const noexcept;
```

Whether the zone's clock shows daylight saving time at the instant `t`. Never for UTC and a [fixed](fixed.md)
zone; all year for a rule that says so (`"EST5EDT,0/0,J365/25"`). A datetime asks its own zone the same,
`t.is_dst()`.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the instant |

## Return value

`true` when it is daylight saving time at `t`.

## Complexity

Logarithmic in the number of the zone's transitions up to the year 2100, as [offset_at](offset_at.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    time::zone lord_howe("Australia/Lord_Howe");  // half an hour of daylight saving time
    time::zone utc = time::zone::utc();
    auto july = time::date(2026, 7, 1).at(12, 0, utc);
    auto january = time::date(2026, 1, 1).at(12, 0, utc);
    println("{} {}", warsaw.is_dst_at(july), warsaw.is_dst_at(january));
    println("{} {}", lord_howe.is_dst_at(july), lord_howe.is_dst_at(january));
    println("{} {}", lord_howe.offset_at(july), lord_howe.offset_at(january));
}
```

Output:

```text
true false
false true
10h30m0s 11h0m0s
```

## See also

- [offset_at](offset_at.md): the offset at an instant
- [next_transition](next_transition.md): when it changes
- [sgcl::time::zone](../zone.md)
