[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::round_precision

```cpp
decimal round_precision(int32_t digits, rounding mode = rounding::half_even) const;
```

The value rounded to `digits` significant digits with the [rounding](../../core/rounding.md), and unchanged when it has no
more: `123.456` to 4 digits is `123.5`, `0.00123456` to 2 is `0.0012`, `999.96` to 4 is `1000` (a carry past the
first digit takes one more zero off, so the result keeps `digits` digits: `1.000e+3`). What Python does to a value in
a context of that precision (`+x`), Java's `round(MathContext)`. NaN and the infinities are themselves.

## Parameters

| Parameter | Description |
|---|---|
| `digits` | the significant digits kept, at least 1 |
| `mode` | how the digits dropped are rounded |

## Return value

The value with `digits` digits or fewer.

## Complexity

The length of the unscaled part, and a division by a power of ten of the digits dropped.

## Exceptions

- `domain_error` when `digits` is below 1, and when `mode` is `rounding::unnecessary` and a dropped digit is not
  zero.
- `length_error` when the scale of the result is past `int32_t`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::decimal("123.456").round_precision(4),
            math::decimal("0.00123456").round_precision(2),
            math::decimal("999.96").round_precision(4));
    println("{} {}", math::decimal("999.96").round_precision(4).to_scientific(),
            math::decimal("2.5").round_precision(5));
}
```

Output:

```text
123.5 0.0012 1000
1.000e+3 2.5
```

## See also

- [rescale](rescale.md): a value to a number of places
- [precision](precision.md): the digits a value has
- [sgcl::math::decimal](README.md)
