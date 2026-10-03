[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [public_key](../ed25519-public_key.md)

# sgcl::crypto::ed25519::public_key::bytes

```cpp
const array<byte, 32>& bytes() const noexcept;
```

Returns the 32 bytes of the key, the encoding of its point as RFC 8032 §5.1.2 writes it: y little-endian, the sign
of x in the top bit. They are what [from_bytes](from_bytes.md) takes back and what a peer is given.

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
    // RFC 8032's TEST 2 seed and the public key it gives
    auto key = crypto::ed25519::private_key::from_seed(
        encoding::hex::decode("4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb"));
    println("{}", encoding::hex::encode(key->public_key().bytes()));
}
```

Output:

```text
3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c
```

## See also

- [from_bytes](from_bytes.md): the reverse
- [to_pkix_der](to_pkix_der.md): the key in a SubjectPublicKeyInfo
- [sgcl::crypto::ed25519::public_key](../ed25519-public_key.md)
