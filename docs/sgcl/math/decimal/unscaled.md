[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::unscaled

```cpp
const big_integer& unscaled() const noexcept;
```

The unscaled part, with the sign: the value is `unscaled() × 10^-scale()`. `1.50` has 150, `-0.003` has −3, `1.5e3`
read from text has 15. NaN and the infinities have zero.

## Parameters

None.

## Return value

The [big_integer](../big_integer/README.md), a reference to the decimal's own.

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
    for (const char* text : {"1.50", "-0.003", "1.5e3", "123456789012345678901234567890.1"}) {
        math::decimal d(text);
        println("{} {}", d.unscaled(), d.scale());
    }
}
```

Output:

```text
150 2
-3 3
15 -2
1234567890123456789012345678901 1
```

## See also

- [scale](scale.md): the other part
- [(constructor)](decimal.md): a decimal of the two parts
- [sgcl::math::decimal](README.md)
