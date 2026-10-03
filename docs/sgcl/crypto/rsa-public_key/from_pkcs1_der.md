[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](../rsa-public_key.md)

# sgcl::crypto::rsa::public_key::from_pkcs1_der

```cpp
static expected<public_key, error> from_pkcs1_der(const slice<const byte>& der) noexcept;
```

Reads an RSAPublicKey of PKCS #1 (RFC 8017 §A.1.1, `RSA PUBLIC KEY` in PEM): `SEQUENCE { modulus, publicExponent }`,
in strict DER and nothing after it. The numbers are checked as [from_modulus](from_modulus.md) checks them.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the DER of the RSAPublicKey |

## Return value

The key, or a [crypto::error](../error.md): `errc::malformed` with the offset of the byte for DER that is not one
RSAPublicKey in strict DER; `errc::invalid_key` and `errc::unsupported` for the numbers, as
[from_modulus](from_modulus.md) has them.

## Complexity

Quadratic in the bits of the modulus.

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

    auto der = key.public_key().to_pkcs1_der();
    auto pub = crypto::rsa::public_key::from_pkcs1_der(der);
    println("{} bits", pub->bits());

    auto broken = crypto::rsa::public_key::from_pkcs1_der(der.as_slice().subslice(0, 100));
    println("{}", broken.error().message());
}
```

Output:

```text
2048 bits
sgcl::crypto::rsa: DER: an RSAPublicKey is a SEQUENCE
```

## See also

- [to_pkcs1_der](to_pkcs1_der.md): writes the encoding
- [from_pkix_der](from_pkix_der.md): the encoding of certificates
- [sgcl::crypto::rsa::public_key](../rsa-public_key.md)
