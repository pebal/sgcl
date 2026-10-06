[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [private_key](README.md)

# sgcl::crypto::hpke::private_key::from_bytes

```cpp
static expected<private_key, error> from_bytes(hpke::kem k, const slice<const byte>& bytes) noexcept;
```

A key of the KEM from its bytes (DeserializePrivateKey): 32 bytes of X25519 (clamped when used), or a scalar of P-256,
P-384 or P-521 (32, 48 or 66 bytes, big-endian) in [1, n - 1].

## Parameters

| Parameter | Description |
|---|---|
| `k` | the KEM |
| `bytes` | the key's bytes: a secret_bytes, or the program's own buffer |

## Return value

The key, or `errc::invalid_key` for bytes of another length or a scalar out of range.

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
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto again = crypto::hpke::private_key::from_bytes(key.kem(), key.bytes());
    println("{}", again->public_key() == key.public_key());
}
```

Output:

```text
true
```

## See also

- [bytes](bytes.md): the other way
- [sgcl::crypto::hpke::private_key](README.md)
