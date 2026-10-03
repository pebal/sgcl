[sgcl](../../README.md) › [math](../README.md) › [rational](README.md)

# sgcl::math::rational::to_double

```cpp
double to_double() const noexcept;
```

The nearest double to the exact value, a tie to the even one: what Go's `Float64` gives. It is rounded once, from
the exact value — not the quotient of the two parts' doubles, which rounds three times and overflows for long
parts. Past the largest double the result is an infinity of the sign, and below half the smallest subnormal a
zero of the sign; between those, a subnormal is rounded to its fewer bits the same way.

## Parameters

None.

## Return value

The double nearest to the fraction.

## Complexity

A division of the numerator, shifted to 53 bits of quotient, by the denominator; a whole number is
[big_integer::to_double](../big_integer/to_double.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {}", math::rational(1, 3).to_double(), math::rational(-7, 8).to_double());
    math::big_integer huge = math::big_integer(1) << 2000;
    println("{} {}", math::rational(huge).to_double(), (math::rational(-1) / huge).to_double());
    math::rational long_parts(math::big_integer(10).pow(400), math::big_integer(10).pow(399) * 3);
    println("{}", long_parts.to_double());
}
```

Output:

```text
0.3333333333333333 -0.875
inf -0
3.3333333333333335
```

## See also

- [to_decimal](to_decimal.md): the decimal with a number of places
- [(constructor)](rational.md): the fraction a double is, exactly
- [sgcl::math::rational](README.md)
