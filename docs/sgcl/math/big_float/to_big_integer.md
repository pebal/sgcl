[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::to_big_integer

```cpp
big_integer to_big_integer(rounding mode = rounding::down) const;
```

The value rounded to a whole number, towards zero when no mode is given (Go's `Int`).

## Parameters

| Parameter | Description |
|---|---|
| `mode` | how the bits below the point are rounded |

## Return value

The [big_integer](../big_integer/README.md).

## Complexity

Linear in the mantissa and the exponent.

## Exceptions

- `domain_error` for an infinity, and for a value that is not whole under `rounding::unnecessary`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_float x(-2.5);
    println("{} {} {}", x.to_big_integer(), x.to_big_integer(rounding::floor),
            x.to_big_integer(rounding::half_even));
    println(math::big_float(1e30).to_big_integer());
}
```

Output:

```text
-2 -3 -2
1000000000000000019884624838656
```

## See also

- [is_integer](is_integer.md): whether nothing is rounded
- [sgcl::math::big_float](README.md)
