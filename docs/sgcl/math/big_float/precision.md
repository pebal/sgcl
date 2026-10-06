[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::precision

```cpp
uint32_t precision() const noexcept;
```

The bits of the mantissa the value carries and its results are rounded to: 64 for a whole number, 53 for a double, what was asked for otherwise, 0 for the default value (+0), which takes the other operand's.

## Parameters

None.

## Return value

The precision in bits.

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
    math::big_float a = 3;
    math::big_float b(0.5);
    math::big_float c(math::rational(1, 3), 200);
    println("{} {} {} {} {}", a.precision(), b.precision(), c.precision(), (a + c).precision(),
            math::big_float().precision());
}
```

Output:

```text
64 53 200 200 0
```

## See also

- [mode](mode.md): how the results are rounded
- [(constructor)](big_float.md): a value at another precision
- [sgcl::math::big_float](README.md)
