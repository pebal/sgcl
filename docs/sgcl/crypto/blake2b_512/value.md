[sgcl](../../README.md) › [crypto](../README.md) › [blake2b_512](README.md)

# sgcl::crypto::blake2b_512::value

```cpp
array<byte, digest_size> value() const noexcept;
```

The digest of everything hashed in so far: `digest_size` bytes, 64 for `blake2b_512`, 48, 32, 32 and 16 for the
others. The last block is compressed on a copy of the state, so the hasher goes on: more `update` after `value()`
gives the digest of the longer message. The copy of a keyed hasher is zeroed before the call returns.

## Parameters

None.

## Return value

The digest, `digest_size` bytes; with a key, the MAC.

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
    crypto::blake2s_256 h;
    h.update("a");
    h.update("bc");
    println(encoding::hex::encode(h.value()));

    // a copy branches: the prefix hashed once, then two ways
    crypto::blake2s_256 left = h;
    left.update("d");
    h.update("e");
    println("{}", left.value() == h.value());
}
```

Output:

```text
508c5e8c327c14e2e1a72ba34eeb452f37458b209ed63a294d999b4c86675982
false
```

## See also

- [digest](digest.md): the same bytes, under the name every hasher has
- [verify](verify.md): a received tag checked against it in constant time
- [sgcl::crypto::blake2b_512](README.md)
