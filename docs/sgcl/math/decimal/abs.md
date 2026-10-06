[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::abs

```cpp
decimal abs() const noexcept;
```

The absolute value, at the same scale: `abs()` of `-1.50` is `1.50`, of −Infinity +Infinity, of NaN NaN.

## Parameters

None.

## Return value

The value without its sign.

## Complexity

Constant: the unscaled part's object is shared.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::decimal("-1.50").abs(), math::decimal("2.5").abs(),
            (-math::decimal::infinity()).abs());
}
```

Output:

```text
1.50 2.5 Infinity
```

## See also

- [sign](sign.md): the sign alone
- [operator-](operator_arith.md): the negation
- [sgcl::math::decimal](README.md)
