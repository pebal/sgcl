[sgcl](../../README.md) › [math](../README.md) › [rational](README.md)

# sgcl::math::rational::denominator

```cpp
const big_integer& denominator() const noexcept;
```

The denominator of the fraction in lowest terms: above zero, with no factor in common with the
[numerator](numerator.md). A whole number has the denominator 1.

## Parameters

None.

## Return value

The denominator, a [big_integer](../big_integer/README.md) above zero.

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
    math::rational r(10, 4);
    println("{} {}", r.denominator(), (r * 2).denominator());
    println("{}", math::rational(0.375).denominator());
}
```

Output:

```text
2 1
8
```

## See also

- [numerator](numerator.md): the numerator, with the sign
- [sgcl::math::rational](README.md)
