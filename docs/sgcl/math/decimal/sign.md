[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::sign

```cpp
int sign() const noexcept;
```

−1 for a value below zero, 0 for zero at any scale, 1 for a value above; the infinities by their sign, NaN 0
([is_nan](is_nan.md) tells it from zero).

## Parameters

None.

## Return value

−1, 0 or 1.

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
    println("{} {} {} {}", math::decimal("-0.01").sign(), math::decimal("0.000").sign(),
            math::decimal(7).sign(),
            (-math::decimal::infinity()).sign());
}
```

Output:

```text
-1 0 1 -1
```

## See also

- [abs](abs.md): the value without its sign
- [sgcl::math::decimal](README.md)
