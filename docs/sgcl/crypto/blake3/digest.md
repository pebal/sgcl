[sgcl](../../README.md) › [crypto](../README.md) › [blake3](README.md)

# sgcl::crypto::blake3::digest

```cpp
array<byte, 32> digest() const noexcept;
```

The same bytes as [value](value.md), under the name every hasher of the [hash module](../../hash/README.md) has for
its result as bytes. The hasher goes on.

## Parameters

None.

## Return value

32 bytes of output.

## Complexity

As [value](value.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::blake3 h;
    h.update("abc");
    println("{}", h.digest() == h.value());
}
```

Output:

```text
true
```

## See also

- [value](value.md): the first 32 bytes of the output
- [sgcl::crypto::blake3](README.md)
