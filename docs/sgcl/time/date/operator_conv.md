[sgcl](../../README.md) › [time](../README.md) › [date](../date.md)

# sgcl::time::date::operator std::chrono::year_month_day, operator std::chrono::sys_days

```cpp
constexpr explicit operator std::chrono::year_month_day() const noexcept;    // (1)
constexpr explicit operator std::chrono::sys_days() const noexcept;          // (2)
```

Converts the date into the calendar of `<chrono>`, explicitly: `std::chrono::year_month_day(d)`,
`static_cast<std::chrono::sys_days>(d)`. A date is made from either implicitly ([constructor](date.md)), so a
comparison of a date with one of them goes the one way, through the date.

1. The year, the month and the day, a `year_month_day` that is always `ok()`.
2. The days from 1970-01-01.

## Parameters

None.

## Return value

- (1) The date as a `std::chrono::year_month_day`.
- (2) The date as a `std::chrono::sys_days`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include <chrono>

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    time::date d(2026, 9, 24);
    auto ymd = std::chrono::year_month_day(d);
    auto days = static_cast<std::chrono::sys_days>(d);
    println("{} {}", ymd == 2026y / 9 / 24, days.time_since_epoch().count());
    println("{}", d == 2026y / 9 / 24);  // the year_month_day converted to a date
}
```

Output:

```text
true 20720
true
```

## See also

- [(constructor)](date.md): a date from the calendar of `<chrono>`
- [sgcl::time::date](../date.md)
