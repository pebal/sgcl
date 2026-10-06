[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::is_nan

```cpp
bool is_nan() const noexcept;
```

Whether the value is [NaN](nan.md).

## Parameters

None.

## Return value

`true` for NaN, `false` for a number or an infinity.

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
    math::decimal inf = math::decimal::infinity();
    println("{} {} {}", (inf - inf).is_nan(), inf.is_nan(), math::decimal("NaN").is_nan());
}
```

Output:

```text
true false true
```

## See also

- [is_infinite](is_infinite.md), [is_finite](is_finite.md): the other questions
- [nan](nan.md): NaN
- [sgcl::math::decimal](README.md)
