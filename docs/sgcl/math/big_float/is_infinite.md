[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::is_infinite

```cpp
bool is_infinite() const noexcept;
```

Whether the value is +∞ or −∞.

## Parameters

None.

## Return value

`true` for an infinity.

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
    println("{} {}", (math::big_float(1) / 0).is_infinite(), math::big_float(1e300).is_infinite());
}
```

Output:

```text
true false
```

## See also

- [infinity](infinity.md): an infinity
- [sgcl::math::big_float](README.md)
