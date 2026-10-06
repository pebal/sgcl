[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::is_integer

```cpp
bool is_integer() const noexcept;
```

Whether the value is a whole number: ±0 and every value without bits below the point; the infinities are not.

## Parameters

None.

## Return value

`true` for a whole number.

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
    println("{} {} {}", math::big_float(1e30).is_integer(), math::big_float(2.5).is_integer(),
            math::big_float::infinity().is_integer());
}
```

Output:

```text
true false false
```

## See also

- [to_big_integer](to_big_integer.md): the whole number
- [sgcl::math::big_float](README.md)
