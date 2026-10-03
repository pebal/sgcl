[sgcl](../../README.md) › [time](../README.md) › [zone](README.md)

# sgcl::time::zone::previous_transition

```cpp
optional<datetime> previous_transition(const datetime& t) const noexcept;
```

The last change of the zone strictly before the instant `t` (a change at `t` itself is not one), seen in this
zone. A change is one of the offset, of daylight saving time or of the abbreviation. There is none for UTC and a
[fixed](fixed.md) zone, and none before the first change of a zone. Go has no way to ask for it.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the instant before which to look |

## Return value

The instant of the change, in this zone, or nothing.

## Complexity

Logarithmic in the number of the zone's transitions up to the year 2100; after 2100, the zone's rule over a few
years at most.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    auto summer = time::date(2026, 7, 1).at(12, 0, warsaw);
    auto change = warsaw.previous_transition(summer).value();
    println("{} {}", change, warsaw.previous_transition(change).value());

    time::zone tokyo("Asia/Tokyo");
    println("{}", tokyo.previous_transition(summer).value());  // its last change

    auto first = warsaw.next_transition(time::date(1800, 1, 1).at(0, 0, warsaw)).value();
    println("{} {}", first, warsaw.previous_transition(first).has_value());
}
```

Output:

```text
2026-03-29T03:00:00+02:00 2025-10-26T02:00:00+01:00
1951-09-09T00:00:00+09:00
1880-01-01T00:00:00+01:24 false
```

## See also

- [next_transition](next_transition.md): the first change after an instant
- [sgcl::time::zone](README.md)
