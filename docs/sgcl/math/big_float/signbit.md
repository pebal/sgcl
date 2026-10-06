[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::signbit

```cpp
bool signbit() const noexcept;
```

Whether the sign is set: the negative values, −∞ and −0, which [sign](sign.md) and `==` take for zero. As `std::signbit` of a double.

## Parameters

None.

## Return value

`true` for a value with the sign.

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
    math::big_float x(1.5);
    math::big_float down(1.5, 53, rounding::floor);
    println("{} {} {}", math::big_float(-0.0).signbit(), (x - x).signbit(), (down - x).signbit());
}
```

Output:

```text
true false true
```

## See also

- [sign](sign.md): −1, 0 or 1
- [sgcl::math::big_float](README.md)
