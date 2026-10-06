[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::is_infinite

```cpp
bool is_infinite() const noexcept;
```

Whether the value is +Infinity or −Infinity.

## Parameters

None.

## Return value

`true` for an infinity, `false` for a number or NaN.

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
    println("{} {} {}", (-math::decimal::infinity()).is_infinite(),
            math::decimal("1e999999").is_infinite(),
            math::decimal::nan().is_infinite());
}
```

Output:

```text
true false false
```

## See also

- [is_nan](is_nan.md), [is_finite](is_finite.md): the other questions
- [infinity](infinity.md): +Infinity
- [sgcl::math::decimal](README.md)
