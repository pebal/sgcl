[sgcl](../../README.md) › [time](../README.md) › [zone](../zone.md)

# sgcl::time::zone::fixed

```cpp
static zone fixed(duration offset);
```

A zone that is always `offset` east of UTC, named for it: `"+05:30"`, `"-03:30"`, `"+05:30:15"` where there are
seconds. The offset is taken in whole seconds, the rest dropped, and must be less than a day either way; one of a
day or more is a mistake of the program, and throws. `fixed` of zero is [UTC](utc.md).

A fixed zone is never daylight saving time, has no transitions, and its abbreviation is its name. Its data is
kept for the rest of the program, one per offset, of which there are finitely many: two fixed zones of the same
offset are the same zone.

## Parameters

| Parameter | Description |
|---|---|
| `offset` | how far east of UTC the zone's clock is, negative for west; less than 24 hours either way |

## Return value

The zone of the offset.

## Complexity

Constant.

## Exceptions

`invalid_argument` when the offset is a day or more either way.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone india = time::zone::fixed(5 * hour + 30 * minute);
    time::zone newfoundland = time::zone::fixed(-(3 * hour + 30 * minute));
    println("{} {} {}", india.name(), newfoundland.name(),
            time::zone::fixed(5 * hour + 30 * minute + 15 * second).name());

    auto t = time::date(2026, 7, 1).at(12, 0, india);
    println("{} {}", t, india.abbreviation_at(t));
    println("{}", india == time::zone::fixed(330 * minute));
    try {
        time::zone::fixed(24 * hour);
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
+05:30 -03:30 +05:30:15
2026-07-01T12:00:00+05:30 +05:30
true
sgcl::time::zone::fixed: an offset of a day or more
```

## See also

- [utc](utc.md): the offset of zero
- [from_posix](from_posix.md): an offset with the rules of daylight saving time
- [sgcl::time::zone](../zone.md)
