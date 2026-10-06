[sgcl](../README.md) › [crypto](README.md) › [hpke](hpke.md)

# sgcl::crypto::hpke::aead

```cpp
#include "sgcl/crypto/hpke.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::hpke {
    enum class aead : uint16_t {
        aes128_gcm = 0x0001,
        aes256_gcm = 0x0002,
        chacha20_poly1305 = 0x0003,
        export_only = 0xffff
    };
}
```

The AEAD of a [suite](hpke-suite.md) (RFC 9180 §7.3), its value the registry's id: what seals the messages, each with a
tag of 16 bytes. `export_only` seals nothing: a context of it only exports secrets, and its `seal` and `open` are
`std::logic_error`.

| Value | Description |
|---|---|
| `aes128_gcm` | AES-128-GCM, the default |
| `aes256_gcm` | AES-256-GCM |
| `chacha20_poly1305` | ChaCha20-Poly1305 |
| `export_only` | no AEAD: exports alone |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    crypto::hpke::suite s{.aead = crypto::hpke::aead::chacha20_poly1305};
    auto sealed = crypto::hpke::seal(key.public_key(), "four", s);
    println("{}", sealed->size());
}
```

Output:

```text
52
```

## See also

- [suite](hpke-suite.md)
- [sgcl::crypto::hpke](hpke.md)
