[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](README.md)

# sgcl::crypto::rsa::private_key::to_pkcs8_der

```cpp
secret_bytes to_pkcs8_der() const;
```

Writes the key as a PKCS #8 PrivateKeyInfo (`PRIVATE KEY` in PEM), version 0 with the `rsaEncryption` identifier, as
Go's `x509.MarshalPKCS8PrivateKey` and OpenSSL write it, byte for byte. The bytes hold the secret: a
[secret_bytes](../secret_bytes/README.md).

## Parameters

None.

## Return value

The DER of the PrivateKeyInfo.

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

    auto der = key.to_pkcs8_der();
    println("{} bytes", der.size());
    println("{}", crypto::rsa::private_key::from_pkcs8_der(der).has_value());
}
```

Output:

```text
1218 bytes
true
```

## See also

- [from_pkcs8_der](from_pkcs8_der.md): reads it back
- [to_pem](to_pem.md): the same as PEM
- [sgcl::crypto::rsa::private_key](README.md)
