[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::in

```cpp
datetime in(const time::zone& z) const noexcept;
```

The same instant seen in the zone `z`, Go's `t.In(loc)`: the fields are what that zone's clock shows, and the result
is equal to the datetime it came from.

## Parameters

| Parameter | Description |
|---|---|
| `z` | the zone to see the instant in |

## Return value

The datetime of the same instant in `z`.

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
    auto meeting = time::date(2026, 10, 30).at(9, 30, time::zone("America/New_York"));
    auto there = meeting.in(time::zone("Europe/Warsaw"));
    println("{} {}", meeting, there);
    println(meeting == there);
}
```

Output:

```text
2026-10-30T09:30:00-04:00 2026-10-30T14:30:00+01:00
true
```

## See also

- [utc](utc.md), [local](local.md): in UTC and in the local zone
- [zone](zone.md): the zone of a datetime
- [sgcl::time::datetime](../datetime.md)
