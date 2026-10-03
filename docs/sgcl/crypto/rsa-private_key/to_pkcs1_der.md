[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](../rsa-private_key.md)

# sgcl::crypto::rsa::private_key::to_pkcs1_der

```cpp
secret_bytes to_pkcs1_der() const;
```

Writes the key as PKCS #1's RSAPrivateKey (RFC 8017 §A.1.2, `RSA PRIVATE KEY` in PEM), as Go's
`x509.MarshalPKCS1PrivateKey` and OpenSSL write it, byte for byte. The bytes hold the secret: they are a
[secret_bytes](../secret_bytes.md), zeroed when it goes, never managed memory.

## Parameters

None.

## Return value

The DER of the RSAPrivateKey.

## Complexity

Linear in the bits of the modulus.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    auto der = key.to_pkcs1_der();
    println("{} bytes", der.size());
    println("{}", crypto::rsa::private_key::from_pkcs1_der(der).has_value());
}
```

Output:

```text
1192 bytes
true
```

## See also

- [from_pkcs1_der](from_pkcs1_der.md): reads it back
- [to_pkcs8_der](to_pkcs8_der.md): the encoding with the algorithm's identifier
- [sgcl::crypto::rsa::private_key](../rsa-private_key.md)
