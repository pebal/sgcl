[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::scale

```cpp
int32_t scale() const noexcept;
```

The scale: the digits after the point when positive, the zeros before it when negative, the value being
`unscaled() × 10^-scale()`. `1.50` has 2, `7` has 0, `1.5e3` read from text has −2. NaN and the infinities have 0.
The scale of a sum is the larger of the two, of a product their sum.

## Parameters

None.

## Return value

The scale.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::decimal a("1.50");
    math::decimal b("0.125");
    println("{} {} {} {}", a.scale(), (a + b).scale(), (a * b).scale(),
            math::decimal("1.5e3").scale());
}
```

Output:

```text
2 3 5 -2
```

## See also

- [unscaled](unscaled.md): the other part
- [rescale](rescale.md): the value at another scale
- [sgcl::math::decimal](README.md)
