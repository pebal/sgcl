[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](README.md)

# sgcl::crypto::p256::private_key::to_sec1_der

```cpp
secret_bytes to_sec1_der() const;
```

The key as a SEC 1 ECPrivateKey (SEC 1 §C.4, RFC 5915): version 1, the scalar, the named curve and the public key,
byte for byte as Go's `x509.MarshalECPrivateKey` writes it. It is the DER of an `EC PRIVATE KEY` block in PEM, the
form older programs read, and what [from_sec1_der](from_sec1_der.md) reads: 121 bytes on P-256, 167 on P-384. PKCS #8
([to_pkcs8_der](to_pkcs8_der.md)) is the form newer programs write.

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
    auto key = crypto::p256::private_key::generate();
    auto stored = key.to_sec1_der();
    println("{} bytes", stored.size());

    auto read = crypto::p256::private_key::from_sec1_der(stored);
    println("{}", read->public_key() == key.public_key());
}
```

Output:

```text
121 bytes
true
```

## See also

- [from_sec1_der](from_sec1_der.md): the key of its ECPrivateKey
- [to_pkcs8_der](to_pkcs8_der.md): the form `PRIVATE KEY`
- [sgcl::crypto::p256::private_key](README.md)
