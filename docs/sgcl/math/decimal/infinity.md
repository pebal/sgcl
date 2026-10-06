[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::infinity

```cpp
static decimal infinity() noexcept;
```

+Infinity, the value PostgreSQL's `NUMERIC` writes `'Infinity'` (since version 14); `-decimal::infinity()` is
−Infinity. Above every finite value and below NaN. A finite value added to it, or multiplied by it, gives an infinity
of the sign; an infinity less itself and an infinity times zero are NaN; a finite value divided by it is zero.

## Parameters

None.

## Return value

+Infinity; its [unscaled](unscaled.md) part is zero and its [scale](scale.md) 0.

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
    println("{} {} {}", inf, -inf, inf + math::decimal("1e1000"));
    println("{} {}", (inf - inf).is_nan(), math::decimal(5).div(inf, 2));
    println("{} {}", -inf < math::decimal("-1e1000"), inf.to_double());
}
```

Output:

```text
Infinity -Infinity Infinity
true 0.00
true inf
```

## See also

- [nan](nan.md): NaN
- [is_infinite](is_infinite.md): asks for it
- [sgcl::math::decimal](README.md)
