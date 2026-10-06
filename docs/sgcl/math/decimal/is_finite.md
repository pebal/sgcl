[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::is_finite

```cpp
bool is_finite() const noexcept;
```

Whether the value is a number: neither NaN nor an infinity.

## Parameters

None.

## Return value

`true` for a number, `false` for NaN and the infinities.

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
    println("{} {} {} {}", math::decimal("0").is_finite(), math::decimal("-1e-999999").is_finite(),
            math::decimal::infinity().is_finite(), math::decimal::nan().is_finite());
}
```

Output:

```text
true true false false
```

## See also

- [is_nan](is_nan.md), [is_infinite](is_infinite.md): the other questions
- [sgcl::math::decimal](README.md)
