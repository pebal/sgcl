[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [public_key](../x25519-public_key.md)

# sgcl::crypto::x25519::public_key::bytes

```cpp
const array<byte, 32>& bytes() const noexcept;
```

Returns the 32 bytes of the key, as they go to the peer: the u coordinate, little-endian, as RFC 7748 encodes it. They
are the bytes the key was made of, not reduced.

## Parameters

None.

## Return value

A reference to the key's bytes, valid while the key lives.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 7748 §6.1's private key of Bob
    auto bob = crypto::x25519::private_key::from_bytes(
        encoding::hex::decode("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb"));
    println("{}", encoding::hex::encode(bob->public_key().bytes()));
}
```

Output:

```text
de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f
```

## See also

- [from_bytes](from_bytes.md): the reverse
- [to_pkix_der](to_pkix_der.md): the key in a SubjectPublicKeyInfo
- [sgcl::crypto::x25519::public_key](../x25519-public_key.md)
