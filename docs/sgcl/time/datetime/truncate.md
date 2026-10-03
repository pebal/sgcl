[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::truncate

```cpp
datetime truncate(duration step) const noexcept;
```

The instant down to a whole number of steps, in the same zone: Go's `t.Truncate(d)`. The steps are counted as Go
counts them, from Go's zero time, 0001-01-01T00:00:00Z — for a step that divides a day the same as counting from
midnight UTC, not from the zone's midnight: an hour in Kolkata, 30 minutes off UTC, is truncated to half past, and a
day to 05:30. A step of zero or less leaves the time as it is. A step below the start of the range gives the start.

## Parameters

| Parameter | Description |
|---|---|
| `step` | the step to truncate to |

## Return value

The latest instant not after this one that is a whole number of steps from Go's zero time; the datetime itself when
`step` is zero or less.

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
    println("{} {}", t.truncate(15 * minute), t.truncate(7 * minute));
    auto kolkata = time::date(2026, 9, 24).at(12, 41, time::zone("Asia/Kolkata"));
    println("{} {}", kolkata.truncate(hour), kolkata.truncate(24 * hour));
    println(t.truncate(duration()));
}
```

Output:

```text
2026-09-24T12:30:00+02:00 2026-09-24T12:36:00+02:00
2026-09-24T12:30:00+05:30 2026-09-24T05:30:00+05:30
2026-09-24T12:41:15.122575+02:00
```

## See also

- [round](round.md): to the nearest step
- [start_of_day](start_of_day.md): the first instant of the zone's day
- [duration::truncate](../../core/duration/truncate.md): a duration toward zero
- [sgcl::time::datetime](../datetime.md)
