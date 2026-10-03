[sgcl](../../README.md) › [time](../README.md) › [date](README.md)

# sgcl::time::date::add_days

```cpp
constexpr date add_days(int n) const noexcept;
```

The date `n` days later, or earlier for a negative `n`. A result past either end of the calendar, -32767-01-01
and 32767-12-31, is that end. `d + n` and `d - n` ([operator+](operator_arith.md)) say the same.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the days to move by |

## Return value

The date moved, saturated at the ends of the calendar.

## Complexity

Constant: a date is a count of days, and a step of days one addition.

## Exceptions

None.

## Notes

A day of the calendar is not a [duration](../../core/duration/README.md): where the clock changes it is 23 or 25 hours,
which is why a date steps by days, and an instant of a [datetime](../datetime/README.md) moved a day keeps its time of the
clock by its own `add_days`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::date d(2026, 9, 24);
    println("{} {}", d.add_days(100), d.add_days(-267));
    println("{}", time::date(32767, 12, 31).add_days(1));
}
```

Output:

```text
2027-01-02 2025-12-31
32767-12-31
```

## See also

- [add_months](add_months.md), [add_years](add_years.md): the calendar's other steps
- [days_until](days_until.md): the days between two dates
- [operator+, operator-](operator_arith.md): the same in operators
- [sgcl::time::date](README.md)
