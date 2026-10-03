[sgcl](../../README.md) › [crypto](../README.md) › [sha1](README.md)

# sgcl::crypto::sha1::digest

```cpp
array<byte, 20> digest() const noexcept;
```

The digest of everything hashed in so far, the same bytes as [value](value.md). Every hasher has both: `value()` is
the result in its natural type, a number for a CRC or an xxHash, and `digest()` is the result as bytes, the most
significant first, so that code over any hasher (`hash::req::hasher`) reads it one way. For a digest of this module
the natural type is the bytes.

## Parameters

None.

## Return value

The digest, 20 bytes.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::sha1 h;
    h.update("abc");
    println("{} {}", h.digest() == h.value(), h.digest().size());
}
```

Output:

```text
true 20
```

## See also

- [value](value.md): the same bytes
- [hash::mixin::hasher](../../hash/mixin/hasher/README.md): the shape every hasher shares
- [sgcl::crypto::sha1](README.md)
