[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](../rsa-public_key.md)

# sgcl::crypto::rsa::public_key::from_pkix_der

```cpp
static expected<public_key, error> from_pkix_der(const slice<const byte>& der) noexcept;
```

Reads a SubjectPublicKeyInfo (RFC 5280 §4.1.2.7, RFC 3279 §2.3.1, `PUBLIC KEY` in PEM) of an `rsaEncryption` key: the
algorithm identifier with NULL parameters, as Go reads it, and the RSAPublicKey in a BIT STRING of whole bytes. The
numbers are checked as [from_modulus](from_modulus.md) checks them.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the DER of the SubjectPublicKeyInfo |

## Return value

The key, or a [crypto::error](../error.md):

- `errc::malformed` with the offset of the byte for DER that is not one SubjectPublicKeyInfo in strict DER, or
  parameters other than NULL;
- `errc::unsupported` for an RSASSA-PSS key (`id-RSASSA-PSS`: read the key as `rsaEncryption`) or a key of another
  algorithm;
- `errc::invalid_key` and `errc::unsupported` for the numbers, as [from_modulus](from_modulus.md) has them.

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
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a certificate of the tree
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);
    auto pub = crypto::rsa::public_key::from_pkix_der(cert.raw_subject_public_key_info());
    println("{} bits, e = {}", pub->bits(), pub->exponent());

    auto ca = crypto::x509::certificate::from_pem(io::read_text("tests/net/tls_testdata/ca.pem"));
    auto p256 = crypto::rsa::public_key::from_pkix_der(ca->raw_subject_public_key_info());
    println("{}", p256.error().message());
}
```

Output:

```text
2048 bits, e = 65537
offset 4: sgcl::crypto::rsa: not an RSA key
```

## See also

- [to_pkix_der](to_pkix_der.md): writes the encoding
- [x509::certificate::raw_subject_public_key_info](../x509-certificate/raw_subject_public_key_info.md): the
  SubjectPublicKeyInfo of a certificate
- [sgcl::crypto::rsa::public_key](../rsa-public_key.md)
