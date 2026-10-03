[sgcl](../../README.md) › [math](../README.md) › [rational](README.md)

# sgcl::math::rational::pow

```cpp
rational pow(int64_t exponent) const;
```

The fraction to the power `exponent`. The parts are raised each, by [big_integer::pow](../big_integer/pow.md): a
power of a fraction in lowest terms is in lowest terms, so no gcd is taken. A negative exponent is the power of
the [inverse](inverse.md), `(2/3)^-2` is `9/4`, `INT64_MIN` included; zero to a negative power is `domain_error`.
`0^0` is 1, as for a [big_integer](../big_integer/README.md).

## Parameters

| Parameter | Description |
|---|---|
| `exponent` | the power, any sign |

## Return value

`x^exponent`, in lowest terms.

## Complexity

The two powers of the parts, by squaring.

## Exceptions

- `domain_error` when the fraction is zero and `exponent` is below zero.
- `length_error` when a part of the result would pass the longest number a `big_integer` holds, before anything
  is computed.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::rational r(2, 3);
    println("{} {} {}", r.pow(3), r.pow(-2), math::rational(-1, 2).pow(5));
    println("{} {}", math::rational().pow(0), math::rational(1).pow(INT64_MIN));
    try {
        math::rational().pow(-1);
    } catch (const domain_error& e) {
        println(e.what());
    }
}
```

Output:

```text
8/27 9/4 -1/32
1 1
sgcl::math::rational::inverse: the inverse of zero
```

## See also

- [inverse](inverse.md): one over the fraction
- [big_integer::pow](../big_integer/pow.md): the power of a whole number
- [sgcl::math::rational](README.md)
