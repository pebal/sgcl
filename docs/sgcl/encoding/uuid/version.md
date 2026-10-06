[sgcl](../../README.md) › [encoding](../README.md) › [uuid](README.md)

# sgcl::encoding::uuid::version

```cpp
constexpr int version() const noexcept;
```

The version field, the top four bits of the seventh byte, as written: 1 to 8 for the RFC's versions (1 and 6 of
the time and a node, 3 and 5 of a name hashed, 4 random, 7 of the Unix time, 8 one's own), 0 for the nil UUID, 15
for the max.

## Parameters

None.

## Return value

The field, 0 to 15.

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
    for (encoding::uuid id : {encoding::uuid("c232ab00-9414-11ec-b3c8-9f6bdeced846"),
                              encoding::uuid("2ed6657d-e927-568b-95e1-2665a8aea6a2"), encoding::uuid::v4(),
                              encoding::uuid::v7()}) {
        println(id.version());
    }
}
```

Output:

```text
1
5
4
7
```

## See also

- [variant](variant.md)
- [sgcl::encoding::uuid](README.md)
