[sgcl](../../README.md) › [crypto](../README.md) › [blake2b_512](README.md)

# sgcl::crypto::blake2b_512::digest

```cpp
array<byte, digest_size> digest() const noexcept;
```

The same bytes as [value](value.md), under the name every hasher of the [hash module](../../hash/README.md) has for
its result as bytes; for a digest of the crypto module the two are one. The hasher goes on.

## Parameters

None.

## Return value

The digest, `digest_size` bytes.

## Complexity

Constant: one compression of the last block.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::blake2s_128 h;
    println(encoding::hex::encode(h.digest()));
    println("{}", h.digest() == h.value());
}
```

Output:

```text
64550d6ffe2c0a01a14aba1eade0200c
true
```

## See also

- [value](value.md): the digest
- [sgcl::crypto::blake2b_512](README.md)
