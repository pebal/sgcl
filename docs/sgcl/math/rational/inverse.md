[sgcl](../../README.md) › [math](../README.md) › [rational](README.md)

# sgcl::math::rational::inverse

```cpp
rational inverse() const;
```

One over the fraction: the parts swapped, the sign kept on the numerator, so the inverse of `-2/3` is `-3/2`. The
inverse of zero is `domain_error`. A division, [operator/](operator_arith.md), is the product with the inverse.

## Parameters

None.

## Return value

`1/x`, in lowest terms.

## Complexity

Constant: the parts' objects are shared.

## Exceptions

`domain_error` when the fraction is zero.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::rational(-2, 3).inverse(), math::rational(5).inverse(),
            math::rational(1, 7).inverse());
    try {
        math::rational().inverse();
    } catch (const domain_error& e) {
        println(e.what());
    }
}
```

Output:

```text
-3/2 1/5 7
sgcl::math::rational::inverse: the inverse of zero
```

## See also

- [operator/](operator_arith.md): the division
- [pow](pow.md): a negative power is a power of the inverse
- [sgcl::math::rational](README.md)
