[sgcl](../../README.md) › [time](../README.md) › [date](../date.md)

# sgcl::time::date::start_of_day

```cpp
datetime start_of_day(const zone& z) const noexcept;
```

The first instant of this date in zone `z`: midnight, or where midnight was skipped (America/Santiago in
September, Asia/Beirut in some years) the change that skipped it, `at(0, 0, 0, z, earlier)`. A date skipped whole
(Pacific/Apia's 2011-12-30) starts where the next one does.

## Parameters

| Parameter | Description |
|---|---|
| `z` | the zone whose day it is |

## Return value

The [datetime](../datetime.md) of the day's first instant, in the zone `z`.

## Complexity

Logarithmic in the number of the zone's changes of the clock; constant for a fixed offset.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    println(time::date(2026, 9, 24).start_of_day(time::zone("Europe/Warsaw")));
    time::zone santiago("America/Santiago");
    println("{} {}", time::date(2026, 9, 5).start_of_day(santiago),
            time::date(2026, 9, 6).start_of_day(santiago));  // 00:00 skipped to 01:00
    println(time::date(2011, 12, 30).start_of_day(time::zone("Pacific/Apia")));
}
```

Output:

```text
2026-09-24T00:00:00+02:00
2026-09-05T00:00:00-04:00 2026-09-06T01:00:00-03:00
2011-12-31T00:00:00+14:00
```

## See also

- [at](at.md): an instant of a time of the clock
- [datetime::start_of_day](../datetime/start_of_day.md): the start of a datetime's day in its zone
- [sgcl::time::date](../date.md)
