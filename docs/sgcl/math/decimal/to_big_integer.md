[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::to_big_integer

```cpp
big_integer to_big_integer(rounding mode = rounding::down) const;
```

The value rounded to a whole number with the [rounding](../../core/rounding.md), towards zero when none is given, as a cast
of a double to an integer cuts: `-2.7` is −2, with `rounding::floor` −3, with `rounding::half_even` −3. A value whole
already is itself, `1.5e30` is 1500000000000000000000000000000.

## Parameters

| Parameter | Description |
|---|---|
| `mode` | how the digits after the point are rounded |

## Return value

The [big_integer](../big_integer/README.md).

## Complexity

A product or a division by a power of ten of the scale.

## Exceptions

- `domain_error` for NaN and the infinities, and when `mode` is `rounding::unnecessary` and the value is not whole.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::decimal d("-2.7");
    println("{} {} {}", d.to_big_integer(), d.to_big_integer(rounding::floor),
            d.to_big_integer(rounding::half_even));
    println(math::decimal("1.5e30").to_big_integer());
}
```

Output:

```text
-2 -3 -3
1500000000000000000000000000000
```

## See also

- [to_int64](to_int64.md): a whole value that fits
- [rescale](rescale.md): the rounding at scale 0, kept a decimal
- [sgcl::math::decimal](README.md)
