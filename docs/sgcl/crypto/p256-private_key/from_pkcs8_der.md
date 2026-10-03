[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](../p256-private_key.md)

# sgcl::crypto::p256::private_key::from_pkcs8_der

```cpp
static expected<private_key, error> from_pkcs8_der(const slice<const byte>& der) noexcept;
```

Reads a PKCS #8 PrivateKeyInfo (RFC 5208) of this curve, Go's `x509.ParsePKCS8PrivateKey` for an ECDSA key: the DER
of a `PRIVATE KEY` block in PEM, the form OpenSSL's `genpkey` writes. The algorithm must be `id-ecPublicKey` with the
named curve P-256, and inside it the ECPrivateKey of SEC 1 with a scalar in [1, n − 1]. Version 2 of the structure,
the OneAsymmetricKey of RFC 5958, is read too: its attributes and its public key after the private one are skipped.
The DER is read strictly; a scalar with a zero byte too many or too few is read as Go and OpenSSL read it.

`p384::private_key::from_pkcs8_der` reads the keys whose named curve is P-384.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the DER of the PrivateKeyInfo |

## Return value

The key, or a [crypto::error](../error.md) with the offset of the byte where the reading stopped:

- `errc::malformed` for DER that is not a PrivateKeyInfo;
- `errc::unsupported` for a key of another algorithm or of another curve;
- `errc::invalid_key` for a scalar not in [1, n − 1].

## Complexity

Linear in the size of `der`, and one multiplication of the base point.

## Exceptions

None.

## Notes

The DER holds the secret scalar: it belongs in a [secret_bytes](../secret_bytes.md), as
[to_pkcs8_der](to_pkcs8_der.md) gives it and [read_secret](../read_secret.md) reads a file, never in managed memory.
A key in PEM is read by [from_pem](from_pem.md), which decodes the base64 straight into a `secret_bytes`.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::private_key::generate();
    auto stored = key.to_pkcs8_der();

    auto read = crypto::p256::private_key::from_pkcs8_der(stored);
    println("{}", read->public_key() == key.public_key());

    // a key of P-384 is not one of P-256
    auto other = crypto::p384::private_key::generate().to_pkcs8_der();
    println("{}", crypto::p256::private_key::from_pkcs8_der(other).error().message());

    auto broken = crypto::p256::private_key::from_pkcs8_der(stored.as_slice().first(20));
    println("{}", broken.error().code() == crypto::errc::malformed);
}
```

Output:

```text
true
offset 17: sgcl::crypto::p256: not a P-256 key
true
```

## See also

- [to_pkcs8_der](to_pkcs8_der.md): the PrivateKeyInfo of a key
- [from_sec1_der](from_sec1_der.md): the older form, `EC PRIVATE KEY`
- [from_pem](from_pem.md): either form in PEM
- [sgcl::crypto::p256::private_key](../p256-private_key.md)
