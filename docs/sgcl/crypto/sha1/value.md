[sgcl](../../README.md) › [crypto](../README.md) › [sha1](../sha1.md)

# sgcl::crypto::sha1::value

```cpp
array<byte, 20> value() const noexcept;
```

The digest of everything hashed in so far. The padding and the last blocks are computed on a copy of the state, so the
hasher goes on: more `update` after `value()` gives the digest of the longer message, as Go's `Sum` leaves its hash
as it was.

## Parameters

None.

## Return value

The digest, 20 bytes.

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
    crypto::sha1 h;
    h.update("a");
    println(encoding::hex::encode(h.value()));
    h.update("bc");
    println(encoding::hex::encode(h.value()));
}
```

Output:

```text
86f7e437faa5a7fce15d1ddcb9eaeaea377667b8
a9993e364706816aba3e25717850c26c9cd0d89d
```

## See also

- [digest](digest.md): the same bytes, under the name every hasher has
- [update](update.md): hashes bytes in
- [sgcl::crypto::sha1](../sha1.md)
