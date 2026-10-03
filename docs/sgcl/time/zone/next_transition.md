[sgcl](../../README.md) › [time](../README.md) › [zone](README.md)

# sgcl::time::zone::next_transition

```cpp
optional<datetime> next_transition(const datetime& t) const noexcept;
```

The first change of the zone strictly after the instant `t` (a change at `t` itself is not one), seen in this
zone. A change is one of the offset, of daylight saving time or of the abbreviation. There is none for UTC and a
[fixed](fixed.md) zone, and none after the last change of a zone that stopped changing (`Asia/Tokyo` since 1951).
Go has no way to ask for it.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the instant after which to look |

## Return value

The instant of the change, in this zone, or nothing.

## Complexity

Logarithmic in the number of the zone's transitions up to the year 2100; after 2100, the zone's rule over a few
years at most.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    auto t = time::date(2026, 7, 1).at(12, 0, warsaw);
    for (int i : range(3)) {
        t = warsaw.next_transition(t).value();
        println("{} {}", t, warsaw.abbreviation_at(t));
    }
    println("{}", time::zone("Asia/Tokyo").next_transition(t).has_value());
    println("{}", time::zone::utc().next_transition(t).has_value());
}
```

Output:

```text
2026-10-25T02:00:00+01:00 CET
2027-03-28T03:00:00+02:00 CEST
2027-10-31T02:00:00+01:00 CET
false
false
```

## See also

- [previous_transition](previous_transition.md): the last change before an instant
- [sgcl::time::zone](README.md)
