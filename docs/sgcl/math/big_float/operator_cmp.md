[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::operator==, operator\<=\> (sgcl::math::big_float)

```cpp
friend bool operator==(const big_float& a, const big_float& b) noexcept;                   // (1)
friend std::weak_ordering operator<=>(const big_float& a, const big_float& b) noexcept;    // (2)
```

Compare two values, whatever their precisions: 0.5 at 53 bits equals 0.5 at 200, +0 equals −0, −∞ is below every
number and +∞ above. The ordering is weak, +0 and −0 being equivalent and not the same. With no NaN the order is
total. A whole number on either side converts.

1. Whether the values are equal.
2. The order of the values.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the values |

## Return value

1. `true` when the values are equal.
2. `less`, `equivalent` or `greater`.

## Complexity

The exponents first; for one exponent, the mantissas, linear.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_float a(0.5);
    math::big_float b(0.5, 200);
    println("{} {} {}", a == b, math::big_float(0.0) == math::big_float(-0.0), a < 1);
    println("{}", -math::big_float::infinity() < math::big_float(-1e300));
}
```

Output:

```text
true true true
true
```

## See also

- [operator+, operator-, operator\*, operator/](operator_arith.md): the arithmetic
- [sgcl::math::big_float](README.md)
