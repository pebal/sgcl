[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::operator=

```cpp
big_float& operator=(const big_float&) noexcept = default;    // (1)
big_float& operator=(big_float&& other) noexcept;             // (2)
```

Replaces the value, its precision and its mode with it.

1. With a copy of another: the mantissa's object is shared.
2. A move, which is the copy (1): the value moved from keeps its value, as a [rational](../rational/README.md)'s.

A whole number converts at 64 bits, so `x = 5` assigns 5 at precision 64; a `double` does not convert by itself.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the value moved from |

The copy (1) takes the value copied, unnamed in the declaration.

## Return value

`*this`.

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
    math::big_float x(math::rational(2, 3), 30);
    math::big_float y;
    y = x;
    x = 5;
    println("{} {} {} {}", x, x.precision(), y, y.precision());
}
```

Output:

```text
5 64 0.666666667 30
```

## See also

- [(constructor)](big_float.md): the conversions
- [sgcl::math::big_float](README.md)
