[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::shortest

```cpp
static decimal shortest(double value) noexcept;
```

The decimal with the fewest digits that reads back as `value` — the digits `std::to_chars` and Python's `repr`
write for a double — at the smallest scale not below zero that holds it: `shortest(0.1)` is `0.1`, `shortest(1e16)`
is `10000000000000000`, `shortest(2.5)` is `2.5`. The decimal a double was written as, where the
[constructor](decimal.md) from a double gives the value the double is (`0.1000000000000000055511151231257827021181583404541015625`).
NaN and the infinities are the decimal's own; −0.0 is 0.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the double |

## Return value

The decimal; its [to_double](to_double.md) is `value` again.

## Complexity

Constant: at most seventeen digits.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    double third = 1.0 / 3;
    println("{} {} {}", math::decimal::shortest(0.1), math::decimal::shortest(1e16),
            math::decimal::shortest(third));
    println(math::decimal::shortest(third).to_double() == third);
    println("{} {}", math::decimal::shortest(0.1 + 0.2),
            math::decimal("0.1") + math::decimal("0.2"));
}
```

Output:

```text
0.1 10000000000000000 0.3333333333333333
true
0.30000000000000004 0.3
```

## See also

- [(constructor)](decimal.md): the double exactly
- [to_double](to_double.md): back to a double
- [sgcl::math::decimal](README.md)
