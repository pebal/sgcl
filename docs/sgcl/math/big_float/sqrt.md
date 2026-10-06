[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::sqrt

```cpp
big_float sqrt() const;
```

The square root, rounded once to this value's precision by its mode: the whole square root of the mantissa widened to two bits more than the precision, with what it leaves as the sticky bit — correctly rounded in every mode (Go's `Sqrt`). The root of −0 is −0, of +∞ +∞.

## Parameters

None.

## Return value

The root.

## Complexity

One integer square root of a number of about twice the precision.

## Exceptions

- `domain_error` for a value below zero, −∞ included, and for an inexact root under `rounding::unnecessary`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{:.40f}", math::big_float(2, 150).sqrt());
    println("{} {}", math::big_float(math::rational(9, 16), 10).sqrt(),
            math::big_float(-0.0).sqrt());
}
```

Output:

```text
1.4142135623730950488016887242096980785697
0.75 -0
```

## See also

- [operator+, operator-, operator\*, operator/](operator_arith.md): the other operations
- [sgcl::math::big_float](README.md)
