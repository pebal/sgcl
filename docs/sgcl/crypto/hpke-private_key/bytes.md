[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [private_key](README.md)

# sgcl::crypto::hpke::private_key::bytes

```cpp
secret_bytes bytes() const;
```

The key's bytes (SerializePrivateKey), in plain memory: the scalar of P-256, P-384 or P-521, big-endian; X25519's 32 bytes
clamped (RFC 9180 §7.1.2), so that they may differ from what [from_bytes](from_bytes.md) read and still be the same
key.

## Parameters

None.

## Return value

The bytes, in a secret_bytes.

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
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_p384);
    println("{}", key.bytes().size());
}
```

Output:

```text
48
```

## See also

- [from_bytes](from_bytes.md): the other way
- [sgcl::crypto::hpke::private_key](README.md)
