[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::to_double

```cpp
double to_double() const noexcept;
```

The nearest double, a tie to the even one, rounded once from the exact value: `"0.1"` gives the double `0.1`,
`"9007199254740993"` (2⁵³ + 1, a tie) gives 2⁵³. Past the largest double an infinity of the sign, below half the
smallest subnormal a zero of the sign; NaN and the infinities as themselves. A value of up to fifteen digits and a
scale within ±22 is one operation on doubles, both exact (Clinger's road); any other is the quotient of
[rational::to_double](../rational/to_double.md), from 800 significant digits and a sticky one in place of the rest
(a midpoint between two doubles has at most 767).

## Parameters

None.

## Return value

The double.

## Complexity

Constant for a short value; otherwise a power of ten of the scale and a division of numbers of up to about 800
digits.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {}", math::decimal("0.1").to_double(),
            math::decimal("9007199254740993").to_double());
    println("{} {} {}", math::decimal("1e400").to_double(), math::decimal("-1e-400").to_double(),
            math::decimal::nan().to_double());
    double x = 2.0 / 3;
    println(math::decimal(x).to_double() == x);
}
```

Output:

```text
0.1 9007199254740992
inf -0 nan
true
```

## See also

- [(constructor)](decimal.md), [shortest](shortest.md): a decimal from a double
- [rational::to_double](../rational/to_double.md): the same rounding of a fraction
- [sgcl::math::decimal](README.md)
