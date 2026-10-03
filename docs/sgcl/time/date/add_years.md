[sgcl](../../README.md) › [time](../README.md) › [date](README.md)

# sgcl::time::date::add_years

```cpp
constexpr date add_years(int n) const noexcept;
```

The same day `n` years later, or earlier for a negative `n`: [add_months](add_months.md) of twelve times `n`, so a
29th of February in a year that has none is cut to the 28th: 2024-02-29 plus a year is 2025-02-28 (Go's `AddDate`
carries it into the 1st of March). A result past either end of the calendar is that end.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the years to move by |

## Return value

The date moved, saturated at the ends of the calendar.

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
    time::date leap(2024, 2, 29);
    println("{} {} {}", leap.add_years(1), leap.add_years(4), leap.add_years(-2024));
    println("{}", time::date(2026, 9, 24).add_years(40000));
}
```

Output:

```text
2025-02-28 2028-02-29 0000-02-29
32767-12-31
```

## See also

- [add_months](add_months.md): months, cut to the month's end
- [sgcl::time::date](README.md)
