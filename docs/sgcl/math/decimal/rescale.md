[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::rescale

```cpp
decimal rescale(int32_t scale, rounding mode = rounding::half_even) const;
```

The value at `scale` places, rounded with the [rounding](../../core/rounding.md) when that drops digits and exact when it
adds zeros: `2.675` to 2 places is `2.68`, `2.665` is `2.66` (half-even), `1.5` to 3 places `1.500`, `1234` to −2
`1.2e+3` (written `1200`). Java's `setScale`, Python's `quantize`, PostgreSQL's `round(value, places)` with
`rounding::half_up`. The value of money brought back to cents after a product: `(price * rate).rescale(2)`. NaN and
the infinities are themselves.

## Parameters

| Parameter | Description |
|---|---|
| `scale` | the places of the result, or the zeros before the point when negative |
| `mode` | how the digits dropped are rounded |

## Return value

The value at `scale`.

## Complexity

A product or a division by a power of ten of the difference of the scales; up to eighteen digits, on the processor's
own numbers. A value far below the last place kept is known to round from zero without the power being made.

## Exceptions

- `domain_error` when `mode` is `rounding::unnecessary` and a dropped digit is not zero.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::decimal("2.675").rescale(2), math::decimal("2.665").rescale(2),
            math::decimal("2.665").rescale(2, rounding::half_up));
    println("{} {} {}", math::decimal("1.5").rescale(3), math::decimal("1234").rescale(-2),
            math::decimal("-0.5").rescale(0, rounding::floor));
    math::decimal price("19.99");
    println((price * math::decimal("0.0825")).rescale(2));
}
```

Output:

```text
2.68 2.66 2.67
1.500 1200 -1
1.65
```

## See also

- [round_precision](round_precision.md): a value to a number of digits
- [trim_scale](trim_scale.md): a value at its fewest places
- [rounding](../../core/rounding.md): the modes
- [sgcl::math::decimal](README.md)
