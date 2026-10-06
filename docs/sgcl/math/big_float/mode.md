[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::mode

```cpp
rounding mode() const noexcept;
```

The [rounding](../../core/rounding.md) of the results of which the value is the left operand; `half_even` unless another was asked for. A whole number on the right comes in at 64 bits and so raises the precision of the result: the divisor of the example has the precision of the dividend.

## Parameters

None.

## Return value

The mode.

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
    math::big_float down(2, 10, rounding::down);
    math::big_float three(3, 10);
    println("{} {}", down.mode() == rounding::down, (down / three).to_hex());
    println((math::big_float(2, 10) / three).to_hex());
}
```

Output:

```text
true 0x1.55p-1
0x1.558p-1
```

## See also

- [precision](precision.md): the bits
- [sgcl::math::big_float](README.md)
