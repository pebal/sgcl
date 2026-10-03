[sgcl](../../README.md) › [time](../README.md) › [date](../date.md)

# sgcl::time::date::days_in_month

```cpp
constexpr int days_in_month() const noexcept;
```

The number of days of the date's month, 28 to 31: the month's last day.

## Parameters

None.

## Return value

The days of the month.

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
    time::date d(2024, 2, 10);
    println("{} {}", d.days_in_month(), time::date(2026, 2, 10).days_in_month());
    println("{}", time::date(d.year(), d.month(), d.days_in_month()));
}
```

Output:

```text
29 28
2024-02-29
```

## See also

- [day](day.md): the day of the month
- [is_leap_year](is_leap_year.md): whether February has 29 days
- [sgcl::time::date](../date.md)
