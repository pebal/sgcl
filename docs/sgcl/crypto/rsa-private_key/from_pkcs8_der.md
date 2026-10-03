[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](../rsa-private_key.md)

# sgcl::crypto::rsa::private_key::from_pkcs8_der

```cpp
static expected<private_key, error> from_pkcs8_der(const slice<const byte>& der) noexcept;
```

Reads a PKCS #8 PrivateKeyInfo (`PRIVATE KEY` in PEM) of an `rsaEncryption` key, or its version 2 (RFC 5958) with
the public key after it, which is passed over: the RSAPrivateKey inside is read and checked as
[from_pkcs1_der](from_pkcs1_der.md) reads it.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the DER of the PrivateKeyInfo |

## Return value

The key, or a [crypto::error](../error.md): `errc::malformed` with the offset of the byte for DER that is not one
PrivateKeyInfo in strict DER; `errc::unsupported` for an RSASSA-PSS key or a key of another algorithm; and the errors
of [from_pkcs1_der](from_pkcs1_der.md) for the key inside.

## Complexity

Cubic in the bits of the modulus, as [from_pkcs1_der](from_pkcs1_der.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    auto der = key.to_pkcs8_der();
    auto again = crypto::rsa::private_key::from_pkcs8_der(der);
    println("{}", again->public_key() == key.public_key());

    auto spki = key.public_key().to_pkix_der();
    println("{}", crypto::rsa::private_key::from_pkcs8_der(spki).error().message());
}
```

Output:

```text
true
offset 4: sgcl::crypto::rsa: DER: a PKCS#8 version is 0 or 1
```

## See also

- [to_pkcs8_der](to_pkcs8_der.md): writes the encoding
- [from_pem](from_pem.md): the key from PEM, either encoding
- [sgcl::crypto::rsa::private_key](../rsa-private_key.md)
