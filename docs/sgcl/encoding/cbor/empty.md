[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::empty

```cpp
bool empty() const noexcept;
```

Whether [size](size.md) is 0: an empty array or map, and every value of another kind.

## Parameters

None.

## Return value

`true` when there is nothing inside.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {}", encoding::cbor::array({}).empty(), encoding::cbor::array({0}).empty());
}
```

Output:

```text
true false
```

## See also

- [size](size.md)
- [sgcl::encoding::cbor](README.md)
