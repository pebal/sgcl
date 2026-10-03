[sgcl](../../README.md) › [math](../README.md) › [rational](README.md)

# sgcl::math::rational::ceil

```cpp
big_integer ceil() const noexcept;
```

The smallest whole number not below the fraction: toward plus infinity, so `ceil(-7/2)` is `-3` and `ceil(7/2)` is
`4`. A whole number is itself.

## Parameters

None.

## Return value

The whole number, a [big_integer](../big_integer/README.md).

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
    println("{} {} {}", math::rational(7, 2).ceil(), math::rational(-7, 2).ceil(),
            math::rational(-4).ceil());
}
```

Output:

```text
4 -3 -4
```

## See also

- [floor](floor.md): the largest whole number not above
- [to_decimal](to_decimal.md): rounded to a number of places
- [sgcl::math::rational](README.md)
