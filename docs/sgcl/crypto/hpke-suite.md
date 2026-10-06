[sgcl](../README.md) › [crypto](README.md) › [hpke](hpke.md)

# sgcl::crypto::hpke::suite

```cpp
#include "sgcl/crypto/hpke.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::hpke {
    struct suite {
        hpke::kdf kdf = hpke::kdf::hkdf_sha256;
        hpke::aead aead = hpke::aead::aes128_gcm;

        friend bool operator==(const suite&, const suite&) noexcept = default;
    };
}
```

`sgcl::crypto::hpke::suite` is the KDF and the AEAD of an HPKE ciphersuite: the [kdf](hpke-kdf.md) of the key schedule
and the [aead](hpke-aead.md) of the messages. The third part of the ciphersuite, the [kem](hpke-kem.md), is the key's:
the recipient's public key on the sender's side, its private key on the recipient's, as Go's `hpke.Seal` takes it. The
default, HKDF-SHA256 with AES-128-GCM, is the suite of ECH and MLS; a call names the fields it changes:
`{.aead = hpke::aead::chacha20_poly1305}`. Both sides must use the same suite.

## Member objects

| Member | Description |
|---|---|
| `kdf` | the KDF of the key schedule: HKDF-SHA256 by default |
| `aead` | the AEAD of the messages: AES-128-GCM by default |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::hpke::suite ech;
    crypto::hpke::suite cnsa{crypto::hpke::kdf::hkdf_sha384, crypto::hpke::aead::aes256_gcm};
    println("{}", ech == cnsa);
}
```

Output:

```text
false
```

## See also

- [sgcl::crypto::hpke](hpke.md)
