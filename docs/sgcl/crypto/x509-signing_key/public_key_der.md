[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [signing_key](README.md)

# sgcl::crypto::x509::signing_key::public_key_der

```cpp
vector<byte> public_key_der() const;
```

The SubjectPublicKeyInfo (RFC 5280 §4.1.2.7) of the key's public half, in DER: what a self-signed certificate made by
[create_certificate](../x509-create_certificate.md) carries, and what `to_pkix_der()` of the key's public key gives.

## Parameters

None.

## Return value

The DER of the SubjectPublicKeyInfo.

## Complexity

Constant for the curves, linear in the modulus for RSA.

## Exceptions

None but running out of memory.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::private_key::generate();
    crypto::x509::signing_key signer = key;
    auto der = signer.public_key_der();
    println("{} bytes, the key's own: {}", der.size(), der == key.public_key().to_pkix_der());
}
```

Output:

```text
91 bytes, the key's own: true
```

## See also

- [create_certificate](../x509-create_certificate.md)
- [sgcl::crypto::x509::signing_key](README.md)
