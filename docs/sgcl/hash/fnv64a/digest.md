[sgcl](../../README.md) › [hash](../README.md) › [fnv64a](README.md)

# sgcl::hash::fnv64a::digest

```cpp
array<byte, 8> digest() const noexcept;
```

`value()` as eight bytes, the most significant first: Go's `h.Sum(nil)`, and the form a function written over any
hasher ([req::hasher](../req/hasher.md)) takes, [crypto::sha256](../../crypto/sha256/README.md) among them.

## Parameters

None.

## Return value

The hash as an `array<byte, 8>`, big-endian.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    hash::fnv64a h;
    h.update("foobar");
    println("{:016x}", h.value());
    println("{}", encoding::hex::encode(h.digest()));
}
```

Output:

```text
85944171f73967e8
85944171f73967e8
```

## See also

- [value](value.md): the hash as a number
- [sgcl::hash::fnv64a](README.md)
