[sgcl](../../README.md) › [math](../README.md) › [rational](../rational.md)

# sgcl::math::rational::floor

```cpp
big_integer floor() const noexcept;
```

The largest whole number not above the fraction: toward minus infinity, so `floor(-7/2)` is `-4` and `floor(7/2)`
is `3`. A whole number is itself.

## Parameters

None.

## Return value

The whole number, a [big_integer](../big_integer.md).

## Complexity

One division of the numerator by the denominator, with the remainder.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::rational(7, 2).floor(), math::rational(-7, 2).floor(),
            math::rational(-4).floor());
}
```

Output:

```text
3 -4 -4
```

## See also

- [ceil](ceil.md): the smallest whole number not below
- [to_decimal](to_decimal.md): rounded to a number of places
- [sgcl::math::rational](../rational.md)
