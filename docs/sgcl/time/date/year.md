[sgcl](../../README.md) › [time](../README.md) › [date](../date.md)

# sgcl::time::date::year

```cpp
constexpr int year() const noexcept;
```

The year of the calendar, -32767 to 32767, where 1 BC is year 0 and 2 BC is -1, as in ISO 8601 and `<chrono>`.

## Parameters

None.

## Return value

The year of the date.

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
    println("{} {}", time::date(2026, 9, 24).year(), time::date(-44, 3, 15).year());
    println("{}", time::date(1, 1, 1).add_days(-1).year());  // the last day of 1 BC
}
```

Output:

```text
2026 -44
0
```

## See also

- [month](month.md), [day](day.md): the other fields
- [is_leap_year](is_leap_year.md): whether the year has a 29th of February
- [sgcl::time::date](../date.md)
