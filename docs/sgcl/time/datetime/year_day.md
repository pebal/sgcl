[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::year_day

```cpp
int year_day() const noexcept;
```

The day of the year the zone's clock shows at the instant, Go's `t.YearDay()`: 1 to 365, 366 in a leap year.

## Parameters

None.

## Return value

The day of the year, 1 to 366.

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
    time::zone warsaw("Europe/Warsaw");
    println(time::date(2026, 9, 24).at(12, 0, warsaw).year_day());
    println(time::date(2028, 12, 31).at(12, 0, warsaw).year_day());
}
```

Output:

```text
267
366
```

## See also

- [day](day.md): the day of the month
- [date](date.md): the date as one value
- [sgcl::time::datetime](../datetime.md)
