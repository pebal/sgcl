[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::to_double

```cpp
double to_double() const noexcept;
```

The nearest double, a tie to the even one: 53 bits, or the bits a subnormal has; an infinity past the largest double, a zero of the sign below half the smallest. Go's `Float64`.

## Parameters

None.

## Return value

The double.

## Complexity

Linear in the mantissa.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_float third(math::rational(1, 3), 200);
    println("{} {}", third.to_double(), math::big_float::parse("1e400").value().to_double());
}
```

Output:

```text
0.3333333333333333 inf
```

## See also

- [(constructor)](big_float.md): from a double, exactly
- [sgcl::math::big_float](README.md)
