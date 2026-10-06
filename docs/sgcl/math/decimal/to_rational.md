[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::to_rational

```cpp
rational to_rational() const;
```

The value as a [rational](../rational/README.md), exactly, in lowest terms: `1.50` is 3/2, `0.125` is 1/8, `1.5e3`
is 1500. Every finite decimal is a fraction over a power of ten; the way back, with a rounding, is the
[constructor](decimal.md) from a fraction and a scale.

## Parameters

None.

## Return value

The fraction.

## Complexity

A power of ten of the scale and the reduction of the fraction.

## Exceptions

- `domain_error` for NaN and the infinities.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::decimal("1.50").to_rational(), math::decimal("0.125").to_rational(),
            math::decimal("1.5e3").to_rational());
    math::rational third = math::decimal("0.1").to_rational() + math::rational(7, 30);
    println("{} {}", third, math::decimal(third, 6));
}
```

Output:

```text
3/2 1/8 1500
1/3 0.333333
```

## See also

- [(constructor)](decimal.md): a fraction rounded to a scale
- [rational](../rational/README.md): the exact fractions
- [sgcl::math::decimal](README.md)
