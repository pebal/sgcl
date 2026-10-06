[sgcl](../README.md) › [crypto](README.md) › [hpke](hpke.md)

# sgcl::crypto::hpke::kem

```cpp
#include "sgcl/crypto/hpke.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::hpke {
    enum class kem : uint16_t {
        dhkem_p256 = 0x0010,
        dhkem_p384 = 0x0011,
        dhkem_p521 = 0x0012,
        dhkem_x25519 = 0x0020
    };
}
```

The KEM of an HPKE ciphersuite (RFC 9180 §7.1), a key's, its value the registry's id: how the sender and the recipient
come
to one shared secret. Each is a Diffie-Hellman KEM over a curve of the module with the HKDF of its own hash.

| Value | Description |
|---|---|
| `dhkem_p256` | DHKEM(P-256, HKDF-SHA256): a public key and `enc` of 65 bytes (uncompressed), a shared secret of 32 |
| `dhkem_p384` | DHKEM(P-384, HKDF-SHA384): 97 bytes, a shared secret of 48 |
| `dhkem_p521` | DHKEM(P-521, HKDF-SHA512): 133 bytes, a shared secret of 64 |
| `dhkem_x25519` | DHKEM(X25519, HKDF-SHA256): 32 bytes, a shared secret of 32; the default |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_p256);
    println("{}", key.public_key().bytes().size());
}
```

Output:

```text
65
```

## See also

- [private_key](hpke-private_key/README.md), [public_key](hpke-public_key/README.md)
- [sgcl::crypto::hpke](hpke.md)
