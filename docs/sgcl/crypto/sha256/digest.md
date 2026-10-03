[sgcl](../../README.md) › [crypto](../README.md) › [sha256](../sha256.md)

# sgcl::crypto::sha256::digest

```cpp
array<byte, 32> digest() const noexcept;   // sha256
array<byte, 28> digest() const noexcept;   // sha224
```

The digest of everything hashed in so far, the same bytes as [value](value.md). Every hasher has both: `value()` is
the result in its natural type, a number for a CRC or an xxHash, and `digest()` is the result as bytes, the most
significant first, so that code over any hasher (`hash::req::hasher`) reads it one way. For a digest of this module
the natural type is the bytes.

## Parameters

None.

## Return value

The digest, 32 bytes for `sha256` and 28 for `sha224`.

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
    println(hex_of(hash::crc32(), "abc"));
    println(hex_of(crypto::sha256(), "abc"));
    println(hex_of(crypto::sha224(), "abc"));
}
```

Output:

```text
352441c2
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7
```

## See also

- [value](value.md): the same bytes
- [hash::mixin::hasher](../../hash/mixin/hasher.md): the shape every hasher shares
- [sgcl::crypto::sha256](../sha256.md)
