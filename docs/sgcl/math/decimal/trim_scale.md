[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::trim_scale

```cpp
decimal trim_scale() const noexcept;
```

The value at the smallest scale not below zero that holds it exactly, PostgreSQL's `trim_scale`: `1.500` is `1.5`,
`2.00` is `2`, `0.000` is `0`, and `1.5e+3` (scale −2) is `1500` at scale 0. Equal values give
[identical](identical.md) results, whatever scales they were written at. NaN and the infinities are themselves.

## Parameters

None.

## Return value

The value at its fewest places, never at a negative scale.

## Complexity

A division by ten for each zero dropped, by 10^18 at a time on a long unscaled part.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {} {}", math::decimal("1.500").trim_scale(), math::decimal("2.00").trim_scale(),
            math::decimal("0.000").trim_scale(), math::decimal("1.5e3").trim_scale().scale());
    println(math::decimal("1.50").trim_scale().identical(math::decimal("1.5000").trim_scale()));
}
```

Output:

```text
1.5 2 0 0
true
```

## See also

- [rescale](rescale.md): a value at a scale given
- [identical](identical.md): the same scale as well as the same value
- [sgcl::math::decimal](README.md)
