[sgcl](../README.md) › [crypto](README.md) › [hpke](hpke.md)

# sgcl::crypto::hpke::kdf

```cpp
#include "sgcl/crypto/hpke.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::hpke {
    enum class kdf : uint16_t {
        hkdf_sha256 = 0x0001,
        hkdf_sha384 = 0x0002,
        hkdf_sha512 = 0x0003
    };
}
```

The KDF of a [suite](hpke-suite.md) (RFC 9180 §7.2), its value the registry's id: what the key schedule derives the
AEAD's key, its base nonce and the exporter secret with. An export is at most 255 times the hash's length.

| Value | Description |
|---|---|
| `hkdf_sha256` | HKDF-SHA256, the default; exports to 8160 bytes |
| `hkdf_sha384` | HKDF-SHA384; exports to 12240 bytes |
| `hkdf_sha512` | HKDF-SHA512; exports to 16320 bytes |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    crypto::hpke::suite s{.kdf = crypto::hpke::kdf::hkdf_sha512};
    auto ctx = crypto::hpke::sender::setup(key.public_key(), s);
    println("{}", ctx->export_secret("label", 64).size());
}
```

Output:

```text
64
```

## See also

- [suite](hpke-suite.md)
- [sgcl::crypto::hpke](hpke.md)
