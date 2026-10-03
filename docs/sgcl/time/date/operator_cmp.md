[sgcl](../../README.md) › [time](../README.md) › [date](../date.md)

# sgcl::time::operator==, operator\<=\> (sgcl::time::date)

```cpp
friend constexpr bool operator==(const date&, const date&) noexcept = default;       // (1)
friend constexpr std::strong_ordering operator<=>(const date&,                       // (2)
                                                 const date&) noexcept = default;
```

Compare two dates by their days from 1970-01-01. `!=`, `<`, `<=`, `>` and `>=` are made from these by the
compiler. A `std::chrono::year_month_day` and a `std::chrono::sys_days` convert to a date, so `d == 2026y / 9 / 24`
compares as dates.

1. `true` when the two are the same day.
2. The order of the two days, the earlier the less.

## Parameters

The two dates compared, the operands, unnamed in the declarations.

## Return value

- (1) Whether the dates are equal.
- (2) `std::strong_ordering::less`, `equal` or `greater`.

## Complexity

Constant: one comparison of two numbers.

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
    println("{} {} {}", d == time::date(2026, 8, 55), d < time::date(2026, 12, 25), d >= d);
    println("{}", d == 2026y / 9 / 24);
}
```

Output:

```text
true true true
true
```

## See also

- [operator+, operator-](operator_arith.md): the arithmetic
- [sgcl::time::date](../date.md)
