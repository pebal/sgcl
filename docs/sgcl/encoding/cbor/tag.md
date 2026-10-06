[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::tag

```cpp
uint64_t tag() const noexcept;
```

The number of a tag; the type of an [extension](extension.md) as its unsigned byte (-1, MessagePack's timestamp, is
255); 0 for every other kind.

## Parameters

None.

## Return value

The number.

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
    println(encoding::cbor::tagged(55799, encoding::cbor()).tag());
    println(encoding::cbor(5).tag());
}
```

Output:

```text
55799
0
```

## See also

- [content](content.md)
- [tagged](tagged.md)
- [sgcl::encoding::cbor](README.md)
