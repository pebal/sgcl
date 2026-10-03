[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [ecdh_key](README.md)

# sgcl::crypto::p256::ecdh_key::to_pkcs8_der

```cpp
secret_bytes to_pkcs8_der() const;
```

The key as a PKCS #8 PrivateKeyInfo (RFC 5208): `id-ecPublicKey` with the named curve, and inside it the SEC 1
ECPrivateKey with the scalar and the public key, byte for byte as Go's `x509.MarshalPKCS8PrivateKey` and OpenSSL
write it, the same form as an ECDSA key's. It is the DER of a `PRIVATE KEY` block in PEM ([to_pem](to_pem.md)), and
what [from_pkcs8_der](from_pkcs8_der.md) reads: 138 bytes on P-256, 185 on P-384.

## Parameters

None.

## Return value

The DER, in a [secret_bytes](../secret_bytes/README.md): it holds the secret scalar, so it is zeroed when it goes and never
lies in managed memory.

## Complexity

Constant.

## Exceptions

`std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::ecdh_key::generate();
    auto stored = key.to_pkcs8_der();
    println("{} bytes", stored.size());

    // read back as an ECDSA key: the format is the same
    auto signer = crypto::p256::private_key::from_pkcs8_der(stored);
    println("{}", signer->public_key() == key.public_key());
}
```

Output:

```text
138 bytes
true
```

## See also

- [from_pkcs8_der](from_pkcs8_der.md): the key of its PrivateKeyInfo
- [to_pem](to_pem.md): the same DER as PEM
- [sgcl::crypto::p256::ecdh_key](README.md)
