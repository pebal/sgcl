[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::round

```cpp
datetime round(duration step) const noexcept;
```

The instant to the nearest whole number of steps, in the same zone: Go's `t.Round(d)`. The steps are counted as Go
counts them, from Go's zero time, 0001-01-01T00:00:00Z — for a step that divides a day the same as counting from
midnight UTC, not from the zone's midnight. Half a step rounds up. A step of zero or less leaves the time as it is.
A step past the end of the range gives the end.

## Parameters

| Parameter | Description |
|---|---|
| `step` | the step to round to |

## Return value

The instant a whole number of steps from Go's zero time nearest to this one, the later of two as near; the datetime
itself when `step` is zero or less.

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
    auto t = time::datetime::from_unix_nano(1790246475122575000, time::zone("Europe/Warsaw"));
    println("{} {}", t.round(second), t.round(15 * minute));
    auto half = time::date(2026, 9, 24).at(12, 0, 30, time::zone("Europe/Warsaw"));
    println("{} {}", half.round(minute), (half - nanosecond).round(minute));
    println(t.round(-second));
}
```

Output:

```text
2026-09-24T12:41:15+02:00 2026-09-24T12:45:00+02:00
2026-09-24T12:01:00+02:00 2026-09-24T12:00:00+02:00
2026-09-24T12:41:15.122575+02:00
```

## See also

- [truncate](truncate.md): down to a step
- [duration::round](../../core/duration/round.md): a duration to the nearest step
- [sgcl::time::datetime](README.md)
