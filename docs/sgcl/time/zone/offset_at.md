[sgcl](../../README.md) › [time](../README.md) › [zone](../zone.md)

# sgcl::time::zone::offset_at

```cpp
duration offset_at(const datetime& t) const noexcept;
```

How far east of UTC the zone's clock is at the instant `t`: `+2h` in Warsaw in summer, in whole seconds; zero for
UTC, the offset itself for a [fixed](fixed.md) zone. The zone `t` is seen in plays no part, only its instant. A
datetime asks its own zone the same, `t.offset()`.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the instant |

## Return value

The offset from UTC, negative west of it.

## Complexity

Logarithmic in the number of the zone's transitions up to the year 2100 (a binary search); after 2100, the rule of
the zone asked, a few years at most.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    time::zone utc = time::zone::utc();
    auto summer = time::date(2026, 7, 1).at(12, 0, utc);
    auto winter = time::date(2026, 1, 1).at(12, 0, utc);
    println("{} {}", warsaw.offset_at(summer), warsaw.offset_at(winter));
    println("{}", time::zone("Asia/Kathmandu").offset_at(summer));
    println("{}", warsaw.offset_at(time::date(1850, 1, 1).at(0, 0, utc)));  // local mean time
}
```

Output:

```text
2h0m0s 1h0m0s
5h45m0s
1h24m0s
```

## See also

- [abbreviation_at](abbreviation_at.md), [is_dst_at](is_dst_at.md): the rest of what a zone is at an instant
- [sgcl::time::zone](../zone.md)
