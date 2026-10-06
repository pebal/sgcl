[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::content

```cpp
cbor content() const noexcept;
```

The value a tag tags; null for every other kind.

## Parameters

None.

## Return value

The content.

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
    encoding::cbor t = encoding::cbor::tagged(37, encoding::cbor::bytes(vector<byte>(16)));
    println(t.content().to_string());
}
```

Output:

```text
h'00000000000000000000000000000000'
```

## See also

- [tag](tag.md)
- [sgcl::encoding::cbor](README.md)
