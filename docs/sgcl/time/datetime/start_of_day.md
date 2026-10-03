[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::start_of_day

```cpp
datetime start_of_day() const noexcept;
```

The first instant of the datetime's [date](date.md) in its zone, `date().start_of_day(zone())`: midnight, or, where
midnight was skipped (America/Santiago in September), the change that skipped it. The time of day is the zone's, not
UTC's: the start of a day in Warsaw is 22:00 or 23:00 UTC of the day before. A date skipped whole (Pacific/Apia's
2011-12-30) is never a datetime's date; a date's [start_of_day](../date/start_of_day.md) says where it starts.

## Parameters

None.

## Return value

The first instant of the date, in the datetime's zone.

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
    auto t = time::date(2026, 9, 24).at(12, 41, 15, time::zone("Europe/Warsaw"));
    println("{} {}", t.start_of_day(), t.utc().start_of_day());
    auto santiago = time::date(2026, 9, 6).at(12, 0, time::zone("America/Santiago"));
    println(santiago.start_of_day());  // midnight skipped: 00:00 to 01:00
}
```

Output:

```text
2026-09-24T00:00:00+02:00 2026-09-24T00:00:00Z
2026-09-06T01:00:00-03:00
```

## See also

- [date](date.md): the date of the zone's clock
- [truncate](truncate.md): down to a whole number of steps, counted in UTC
- [date::start_of_day](../date/start_of_day.md): the start of a date in a zone
- [sgcl::time::datetime](../datetime.md)
