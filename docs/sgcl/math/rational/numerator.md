[sgcl](../../README.md) › [math](../README.md) › [rational](../rational.md)

# sgcl::math::rational::numerator

```cpp
const big_integer& numerator() const noexcept;
```

The numerator of the fraction in lowest terms. It carries the sign of the fraction: `rational(6, -4)` has the
numerator `-3`. Zero is `0/1`.

## Parameters

None.

## Return value

The numerator, a [big_integer](../big_integer.md) with the sign of the fraction.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::rational r(6, -4);
    println("{} {}", r.numerator(), r.denominator());
    println("{}", math::rational().numerator());
}
```

Output:

```text
-3 2
0
```

## See also

- [denominator](denominator.md): the denominator, above zero
- [sgcl::math::rational](../rational.md)
