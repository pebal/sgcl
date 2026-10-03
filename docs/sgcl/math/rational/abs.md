[sgcl](../../README.md) › [math](../README.md) › [rational](../rational.md)

# sgcl::math::rational::abs

```cpp
rational abs() const noexcept;
```

The absolute value: the fraction with a numerator not below zero.

## Parameters

None.

## Return value

`-*this` when the fraction is below zero, `*this` otherwise.

## Complexity

Constant: the parts' objects are shared.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::rational(-3, 4).abs(), math::rational(5, 2).abs(),
            math::rational().abs());
}
```

Output:

```text
3/4 5/2 0
```

## See also

- [operator-](operator_arith.md): the negation
- [sgcl::math::rational](../rational.md)
