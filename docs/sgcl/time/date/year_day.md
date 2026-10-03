[sgcl](../../README.md) › [time](../README.md) › [date](README.md)

# sgcl::time::date::year_day

```cpp
constexpr int year_day() const noexcept;
```

The day of the year, 1 for the 1st of January to 365, or 366 for the 31st of December of a leap year: the number
ISO 8601's ordinal date writes (`"2026-267"`).

## Parameters

None.

## Return value

The day of the year.

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
    println("{} {}", time::date(2026, 9, 24).year_day(), time::date(2024, 12, 31).year_day());
    println("{}", time::date(2026, 1, 267));  // a day of the year carried into its month
}
```

Output:

```text
267 366
2026-09-24
```

## See also

- [weekday](weekday.md), [iso_week](iso_week.md): the other numbers of a day
- [parse](parse.md): reads the ordinal date
- [sgcl::time::date](README.md)
