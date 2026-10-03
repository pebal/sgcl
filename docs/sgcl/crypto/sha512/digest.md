[sgcl](../../README.md) › [crypto](../README.md) › [sha512](README.md)

# sgcl::crypto::sha512::digest

```cpp
array<byte, 64> digest() const noexcept;   // sha512
array<byte, 48> digest() const noexcept;   // sha384
array<byte, 32> digest() const noexcept;   // sha512_256
```

The digest of everything hashed in so far, the same bytes as [value](value.md). Every hasher has both: `value()` is
the result in its natural type, a number for a CRC or an xxHash, and `digest()` is the result as bytes, the most
significant first, so that code over any hasher (`hash::req::hasher`) reads it one way. For a digest of this module
the natural type is the bytes.

## Parameters

None.

## Return value

The digest: 64 bytes for `sha512`, 48 for `sha384`, 32 for `sha512_256`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

// Any hasher of the library: the bytes of its result
size_t digest_bytes(hash::req::hasher auto h) {
    return h.digest().size();
}

int main() {
    println("{} {} {}", digest_bytes(crypto::sha512()), digest_bytes(crypto::sha384()),
            digest_bytes(crypto::sha512_256()));
}
```

Output:

```text
64 48 32
```

## See also

- [value](value.md): the same bytes
- [hash::mixin::hasher](../../hash/mixin/hasher/README.md): the shape every hasher shares
- [sgcl::crypto::sha512](README.md)
