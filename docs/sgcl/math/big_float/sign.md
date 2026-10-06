[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::sign

```cpp
int sign() const noexcept;
```

−1 for a value below zero, 0 for ±0, 1 for a value above; the infinities by their sign.

## Parameters

None.

## Return value

−1, 0 or 1.

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
    println("{} {} {}", math::big_float(-2).sign(), math::big_float(-0.0).sign(),
            math::big_float::infinity().sign());
}
```

Output:

```text
-1 0 1
```

## See also

- [signbit](signbit.md): tells −0 from +0
- [sgcl::math::big_float](README.md)
