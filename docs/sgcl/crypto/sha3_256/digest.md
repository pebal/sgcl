[sgcl](../../README.md) › [crypto](../README.md) › [sha3_256](../sha3_256.md)

# sgcl::crypto::sha3_256::digest

```cpp
array<byte, 32> digest() const noexcept;   // sha3_256
array<byte, 28> digest() const noexcept;   // sha3_224
array<byte, 48> digest() const noexcept;   // sha3_384
array<byte, 64> digest() const noexcept;   // sha3_512
```

The digest of everything hashed in so far, the same bytes as [value](value.md). Every hasher has both: `value()` is
the result in its natural type, a number for a CRC or an xxHash, and `digest()` is the result as bytes, the most
significant first, so that code over any hasher (`hash::req::hasher`) reads it one way. For a digest of this module
the natural type is the bytes.

## Parameters

None.

## Return value

The digest: 32 bytes for `sha3_256`, 28 for `sha3_224`, 48 for `sha3_384`, 64 for `sha3_512`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

// Any hasher of the library, its result as bytes
string hex_of(hash::req::hasher auto h, const string& text) {
    h.update(text);
    return encoding::hex::encode(h.digest());
}

int main() {
    println(hex_of(crypto::sha3_384(), "abc"));
}
```

Output:

```text
ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25
```

## See also

- [value](value.md): the same bytes
- [hash::mixin::hasher](../../hash/mixin/hasher.md): the shape every hasher shares
- [sgcl::crypto::sha3_256](../sha3_256.md)
