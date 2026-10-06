[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [public_key](README.md)

# sgcl::crypto::hpke::public_key::from_bytes

```cpp
static expected<public_key, error> from_bytes(hpke::kem k, const slice<const byte>& bytes) noexcept;
```

A key of the KEM from its bytes (DeserializePublicKey, RFC 9180 §7.1.1): 32 bytes of X25519, or a point of P-256 or
P-384 or P-521 uncompressed (65, 97 or 133 bytes) and on its curve.

## Parameters

| Parameter | Description |
|---|---|
| `k` | the KEM |
| `bytes` | the key's bytes |

## Return value

The key, or `errc::invalid_key` for bytes of another length or a point that is not on the curve (a compressed one
among them).

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
    auto key = crypto::hpke::public_key::from_bytes(crypto::hpke::kem::dhkem_p256, "not a point");
    println("{}", key.error().message());
}
```

Output:

```text
sgcl::crypto::hpke: not a public key of the KEM
```

## See also

- [bytes](bytes.md): the other way
- [sgcl::crypto::hpke::public_key](README.md)
