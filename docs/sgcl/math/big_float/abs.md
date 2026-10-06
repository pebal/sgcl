[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::abs

```cpp
big_float abs() const noexcept;
```

The value without its sign, at the same precision and mode: |−1.5| is 1.5, |−∞| is +∞, |−0| is +0.

## Parameters

None.

## Return value

The absolute value.

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
    println("{} {}", math::big_float(-1.5).abs(), (-math::big_float::infinity()).abs());
}
```

Output:

```text
1.5 +Inf
```

## See also

- [sign](sign.md), [signbit](signbit.md): the sign alone
- [sgcl::math::big_float](README.md)
