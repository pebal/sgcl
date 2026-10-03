[sgcl](../../README.md) › [crypto](../README.md) › [sha256](../sha256.md)

# sgcl::crypto::sha256::value

```cpp
array<byte, 32> value() const noexcept;   // sha256
array<byte, 28> value() const noexcept;   // sha224
```

The digest of everything hashed in so far. The padding and the last blocks are computed on a copy of the state, so the
hasher goes on: more `update` after `value()` gives the digest of the longer message, as Go's `Sum` leaves its hash
as it was.

## Parameters

None.

## Return value

The digest, 32 bytes for `sha256` and 28 for `sha224`.

## Complexity

Constant: one or two compressions of the last block.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::sha256 h;
    h.update("a");
    println(encoding::hex::encode(h.value()));
    h.update("bc");  // goes on after "a"
    println(encoding::hex::encode(h.value()));

    // a copy branches: the prefix hashed once, then two ways
    crypto::sha256 left = h;
    left.update("d");
    h.update("e");
    println("{}", left.value() == h.value());
}
```

Output:

```text
ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
false
```

## See also

- [digest](digest.md): the same bytes, under the name every hasher has
- [update](update.md): hashes bytes in
- [sgcl::crypto::sha256](../sha256.md)
