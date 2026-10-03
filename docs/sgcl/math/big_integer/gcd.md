[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::gcd

```cpp
big_integer gcd(const big_integer& other) const noexcept;
```

The greatest common divisor of the number and `other`, never negative, whatever the signs: `gcd(a, 0)` is `|a|`
and `gcd(0, 0)` is 0. Lehmer's method: the steps of Euclid's algorithm that the top bits of the two numbers decide
are found on single words, and applied to the whole numbers in one pass. Go's `GCD` gives the cofactors besides;
here the one question they usually answer, an inverse modulo a number, is [mod_inverse](mod_inverse.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the other number |

## Return value

The greatest common divisor, zero or more.

## Complexity

Quadratic in the length of the numbers (Lehmer's method, as Go's).

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::big_integer(-12).gcd(18), math::big_integer(-7).gcd(0),
            math::big_integer(0).gcd(0));
    math::big_integer a = math::big_integer(6).pow(50);
    math::big_integer b = math::big_integer(10).pow(30);
    println("{}", a.gcd(b) == math::big_integer(2).pow(30));
}
```

Output:

```text
6 7 0
true
```

## See also

- [lcm](lcm.md): the least common multiple
- [mod_inverse](mod_inverse.md): the inverse modulo a number
- [benchmarks](../benchmarks.md#big_integer): the time against Go's `math/big`
- [sgcl::math::big_integer](README.md)
