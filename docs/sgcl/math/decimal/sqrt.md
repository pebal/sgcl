[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::sqrt

```cpp
decimal sqrt(int32_t digits, rounding mode = rounding::half_even) const;
```

The square root, rounded to `digits` significant digits with the [rounding](../../core/rounding.md): `2.sqrt(10)` is
`1.414213562`. Rounded once, from the exact root: the whole square root of the value scaled by a power of ten, with
what it leaves as the deciding remainder, so every mode gives the root on its side (a root is never exactly half way
between two). An exact root drops the zeros at its end down to half the scale of the value, as Python's `sqrt` does:
the root of `4` is `2`, of `0.0625` is `0.25`, of `1e4` is `1e+2`. The root of +Infinity is +Infinity, of NaN NaN.

## Parameters

| Parameter | Description |
|---|---|
| `digits` | the significant digits of the root, at least 1 |
| `mode` | how the root is rounded to them |

## Return value

The root: `digits` digits or fewer, at the scale they take.

## Complexity

One integer square root of a number of about twice `digits` digits ([big_integer::sqrt](../big_integer/sqrt.md)).

## Exceptions

- `domain_error` when the value is below zero (−Infinity too), when `digits` is below 1, and when `mode` is
  `rounding::unnecessary` and the root has more than `digits` digits.
- `length_error` when the scale of the root is past `int32_t`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {}", math::decimal(2).sqrt(10), math::decimal(2).sqrt(10, rounding::up));
    println("{} {} {}", math::decimal(4).sqrt(28), math::decimal("0.0625").sqrt(28),
            math::decimal("1e4").sqrt(5));
    println(math::decimal(2).sqrt(60));
}
```

Output:

```text
1.414213562 1.414213563
2 0.25 100
1.41421356237309504880168872420969807856967187537694807317668
```

## See also

- [div_precision](div_precision.md): the quotient to a number of digits
- [big_integer::sqrt](../big_integer/sqrt.md): the whole square root
- [sgcl::math::decimal](README.md)
