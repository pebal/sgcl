[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::size

```cpp
size_t size() const noexcept;
```

The elements of an array, the members of a map; 0 for every other kind.

## Parameters

None.

## Return value

The count.

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
    println("{} {} {}", encoding::cbor::array({1, 2}).size(), encoding::cbor::map({{1, 2}}).size(), encoding::cbor("ab").size());
}
```

Output:

```text
2 1 0
```

## See also

- [empty](empty.md)
- [sgcl::encoding::cbor](README.md)
