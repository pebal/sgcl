[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](../rsa-private_key.md)

# sgcl::crypto::rsa::private_key::from_pkcs1_der

```cpp
static expected<private_key, error> from_pkcs1_der(const slice<const byte>& der) noexcept;
```

Reads an RSAPrivateKey of PKCS #1 (RFC 8017 §A.1.2, `RSA PRIVATE KEY` in PEM) of two primes (version 0), in strict DER
and nothing after it. The key is checked whole: n = p·q, qInv·q = 1 mod p, dP = d mod (p − 1) and e·dP = 1 mod (p −
1), and the same for q, in constant time, since they are secrets; the public key is checked as
[public_key::from_modulus](../rsa-public_key/from_modulus.md) checks it.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the DER of the RSAPrivateKey |

## Return value

The key, or a [crypto::error](../error.md):

- `errc::malformed` with the offset of the byte for DER that is not one RSAPrivateKey in strict DER;
- `errc::unsupported` for a multi-prime key (version 1), a modulus of fewer than 1024 bits or more than 16384, an
  exponent above 2³¹ − 1;
- `errc::invalid_key` for an even modulus, an exponent even or below 3, a prime that is even or 1, and numbers that do
  not agree.

## Complexity

Cubic in the bits of the modulus: the checks are products and reductions of numbers of its length.

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

    auto der = key.to_pkcs1_der();
    auto again = crypto::rsa::private_key::from_pkcs1_der(der);
    println("{}", again->public_key() == key.public_key());

    auto pkcs8 = key.to_pkcs8_der();
    println("{}", crypto::rsa::private_key::from_pkcs1_der(pkcs8).error().message());
}
```

Output:

```text
true
offset 7: sgcl::crypto::rsa: DER: the modulus is a non-negative INTEGER
```

## See also

- [to_pkcs1_der](to_pkcs1_der.md): writes the encoding
- [from_pem](from_pem.md): the key from PEM, either encoding
- [sgcl::crypto::rsa::private_key](../rsa-private_key.md)
