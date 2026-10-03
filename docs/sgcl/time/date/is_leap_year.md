[sgcl](../../README.md) › [time](../README.md) › [date](README.md)

# sgcl::time::date::is_leap_year

```cpp
constexpr bool is_leap_year() const noexcept;
```

Checks whether the date's year is a leap year of the Gregorian calendar: divisible by 4, and by 400 when it is
by 100. The calendar is extended backwards as ISO 8601 extends it, with a year 0, which is a leap year.

## Parameters

None.

## Return value

`true` when the year has a 29th of February, `false` otherwise.

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
    for (int year : {2024, 2026, 1900, 2000, 0}) {
        println("{} {}", year, time::date(year, 1, 1).is_leap_year());
    }
}
```

Output:

```text
2024 true
2026 false
1900 false
2000 true
0 true
```

## See also

- [days_in_month](days_in_month.md): the days of the date's month
- [year](year.md): the year
- [sgcl::time::date](README.md)
