[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::exponent

```cpp
int64_t exponent() const noexcept;
```

The exponent of the value written 0.m × 2^exp with ½ ≤ m < 1, Go's `MantExp`: 1 for 1, 0 for 0.75, −3 for 0.1; 0 for zero and the infinities. It lies within `int32_t`, the range of the exponent.

## Parameters

None.

## Return value

The exponent.

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
    println("{} {} {}", math::big_float(1.0).exponent(), math::big_float(0.75).exponent(),
            math::big_float(0.1).exponent());
}
```

Output:

```text
1 0 -3
```

## See also

- [to_hex](to_hex.md): the mantissa and the exponent in hexadecimal
- [sgcl::math::big_float](README.md)
