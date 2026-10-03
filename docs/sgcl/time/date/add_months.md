[sgcl](../../README.md) › [time](../README.md) › [date](README.md)

# sgcl::time::date::add_months

```cpp
constexpr date add_months(int n) const noexcept;
```

The same day of the month `n` months later, or earlier for a negative `n`, cut to the month's last day where the
month is shorter: 2026-01-31 plus one month is 2026-02-28 and plus two is 2026-03-31. That is what "a month from
now" means to a person and to Java, .NET and PostgreSQL; Go's `AddDate` carries into March instead (2026-03-03).
A result past either end of the calendar is that end.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the months to move by |

## Return value

The date moved, saturated at the ends of the calendar.

## Complexity

Constant.

## Exceptions

None.

## Notes

The steps are not associative: plus one month and plus one month again from 2026-01-31 is 2026-03-28, plus two
months at once is 2026-03-31. Nothing that cuts to the month's end can be.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::date invoice(2026, 1, 31);
    for (int i : {1, 2, 3, -2}) {
        println("{} {}", i, invoice.add_months(i));
    }
    println(invoice.add_months(1).add_months(1));
}
```

Output:

```text
1 2026-02-28
2 2026-03-31
3 2026-04-30
-2 2025-11-30
2026-03-28
```

## See also

- [add_years](add_years.md): twelve months
- [add_days](add_days.md): days
- [sgcl::time::date](README.md)
