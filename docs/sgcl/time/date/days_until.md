[sgcl](../../README.md) › [time](../README.md) › [date](../date.md)

# sgcl::time::date::days_until

```cpp
constexpr int days_until(date other) const noexcept;
```

The days from this date to `other`: `other` minus this, negative for an `other` in the past. `a - b`
([operator-](operator_arith.md)) is `b.days_until(a)`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the date counted to |

## Return value

The number of days, positive when `other` is later.

## Complexity

Constant: one subtraction.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::date d(2026, 9, 24), christmas(2026, 12, 25);
    println("{} {}", d.days_until(christmas), christmas.days_until(d));
    println("{}", christmas - d);
}
```

Output:

```text
92 -92
92
```

## See also

- [add_days](add_days.md): the step the other way
- [operator-](operator_arith.md): the same in an operator
- [sgcl::time::date](../date.md)
