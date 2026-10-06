[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::div

```cpp
decimal div(const decimal& by, int32_t scale, rounding mode = rounding::half_even) const;
```

The quotient of this value by `by`, rounded to `scale` places with the [rounding](../../core/rounding.md): `1.div(3, 4)` is
`0.3333`, `2.div(3, 2)` is `0.67`, `2.div(3, 0, rounding::down)` is `0`. The result is at `scale` whatever the
scales of the two — what Java's `divide(divisor, scale, roundingMode)` is, and the form for money, where the places
are known. Rounded once, from the exact quotient.

NaN divided by anything, or anything by NaN, is NaN; an infinity by a finite value is an infinity of the quotient's
sign, by an infinity NaN; a finite value by an infinity is zero at `scale`.

## Parameters

| Parameter | Description |
|---|---|
| `by` | the divisor |
| `scale` | the places of the quotient, or the zeros before the point when negative |
| `mode` | how the quotient is rounded to them |

## Return value

The quotient at `scale`.

## Complexity

One division of the unscaled parts, the dividend's brought up by a power of ten for the places asked; up to eighteen
digits, on the processor's own numbers. A quotient far below the last place asked is known to round from zero
without the power of ten being made.

## Exceptions

- `domain_error` when `by` is zero, and when `mode` is `rounding::unnecessary` and the quotient does not end within
  `scale` places.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::decimal one(1);
    println("{} {} {}", one.div(3, 4), math::decimal(2).div(3, 2),
            math::decimal(2).div(3, 0, rounding::down));

    // a bill of 100.00 split three ways, the cents rounded down and the rest to the first
    math::decimal bill("100.00");
    math::decimal share = bill.div(3, 2, rounding::down);
    println("{} {}", share, bill - share * 2);

    println("{} {}", math::decimal("1e6").div(math::decimal("7"), -3),
            math::decimal("-1").div(8, 2));
}
```

Output:

```text
0.3333 0.67 0
33.33 33.34
143000 -0.12
```

## See also

- [div_precision](div_precision.md): the quotient to a number of digits
- [div_rem](div_rem.md): the whole quotient with the remainder
- [rounding](../../core/rounding.md): the modes
- [sgcl::math::decimal](README.md)
