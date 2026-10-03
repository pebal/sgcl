[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [private_key](../x25519-private_key.md)

# sgcl::crypto::x25519::private_key::from_pkcs8_der

```cpp
static expected<private_key, error> from_pkcs8_der(const slice<const byte>& der) noexcept;
```

Makes the key of a PKCS #8 PrivateKeyInfo (RFC 8410, OID 1.3.101.110), the DER that OpenSSL writes and Go's
`x509.ParsePKCS8PrivateKey` reads. The reading is strict DER. A version 1 PKCS #8 (RFC 5958's OneAsymmetricKey) may
carry the public key, which must be the one the private key gives, and attributes, which are skipped. The key goes
from `der` straight into the object; nothing of it is left in managed memory.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the DER of the PrivateKeyInfo |

## Return value

The key, or an [error](../error.md): `errc::unsupported` for another algorithm's key, `errc::malformed` for DER that
cannot be read, with the [offset](../error/offset.md) where it was found, and `errc::invalid_key` for a public key
that is not the private key's.

## Complexity

Linear in the size of `der`, and one fixed-base multiplication.

## Exceptions

None.

## Notes

The DER holds the secret: a program keeps it in a [secret_bytes](../secret_bytes.md), as
[to_pkcs8_der](to_pkcs8_der.md) gives it, and reads a file of it with [read_secret](../read_secret.md).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 7748 §6.1's private key of Alice in a PKCS #8, version 0
    auto v0 = encoding::hex::decode(
        "302e020100300506032b656e04220420"
        "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
    auto alice = crypto::x25519::private_key::from_pkcs8_der(v0);
    println("{}", encoding::hex::encode(alice->public_key().bytes()));

    // version 1, with a public key that is Bob's
    auto v1 = encoding::hex::decode(
        "3051020101300506032b656e04220420"
        "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a"
        "812100"
        "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
    println("{}", crypto::x25519::private_key::from_pkcs8_der(v1).error().message());
}
```

Output:

```text
8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a
the public key is not the private key's
```

## See also

- [to_pkcs8_der](to_pkcs8_der.md): the reverse
- [from_pem](from_pem.md): the key of PEM text
- [sgcl::crypto::x25519::private_key](../x25519-private_key.md)
