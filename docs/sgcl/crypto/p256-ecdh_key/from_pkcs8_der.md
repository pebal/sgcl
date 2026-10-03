[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [ecdh_key](README.md)

# sgcl::crypto::p256::ecdh_key::from_pkcs8_der

```cpp
static expected<ecdh_key, error> from_pkcs8_der(const slice<const byte>& der) noexcept;
```

Reads a PKCS #8 PrivateKeyInfo (RFC 5208) of this curve: the DER of a `PRIVATE KEY` block in PEM. The format is the
one of an ECDSA key, `id-ecPublicKey` with the named curve P-256 and the SEC 1 ECPrivateKey inside, so the PKCS #8
of a [private_key](../p256-private_key/README.md) reads as an ECDH key of the same scalar. Version 2 of the structure
(RFC 5958) is read too, its attributes and public key skipped. The DER is read strictly; a scalar with a zero byte
too many or too few is read as Go and OpenSSL read it.

`p384::ecdh_key::from_pkcs8_der` reads the keys whose named curve is P-384.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the DER of the PrivateKeyInfo |

## Return value

The key, or a [crypto::error](../error/README.md) with the offset of the byte where the reading stopped:

- `errc::malformed` for DER that is not a PrivateKeyInfo;
- `errc::unsupported` for a key of another algorithm or of another curve;
- `errc::invalid_key` for a scalar not in [1, n − 1].

## Complexity

Linear in the size of `der`, and one multiplication of the base point.

## Exceptions

None.

## Notes

The DER holds the secret scalar: it belongs in a [secret_bytes](../secret_bytes/README.md), as
[to_pkcs8_der](to_pkcs8_der.md) gives it and [read_secret](../read_secret.md) reads a file, never in managed memory.
There is no SEC 1 form of an ECDH key: an `EC PRIVATE KEY` is read as an ECDSA key and made an ECDH one by
[to_ecdh](../p256-private_key/to_ecdh.md).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::ecdh_key::generate();
    auto read = crypto::p256::ecdh_key::from_pkcs8_der(key.to_pkcs8_der());
    println("{}", read->public_key() == key.public_key());

    // an ECDSA key's PKCS #8 is the same format
    auto signer = crypto::p256::private_key::generate();
    auto agreement = crypto::p256::ecdh_key::from_pkcs8_der(signer.to_pkcs8_der());
    println("{}", agreement->public_key() == signer.public_key());
}
```

Output:

```text
true
true
```

## See also

- [to_pkcs8_der](to_pkcs8_der.md): the PrivateKeyInfo of a key
- [from_pem](from_pem.md): the same DER in PEM
- [sgcl::crypto::p256::ecdh_key](README.md)
