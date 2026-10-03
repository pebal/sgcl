[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::signature

```cpp
const vector<byte>& signature() const noexcept;
```

Returns the issuer's signature over [raw_tbs](raw_tbs.md), as the certificate holds it: the BIT STRING's bytes, a DER
ECDSA-Sig-Value for ECDSA, the plain value for RSA and Ed25519.

## Parameters

None.

## Return value

The bytes of the signature.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    println("{} bytes", cert.signature().size());
}
```

Output:

```text
71 bytes
```

## See also

- [check_signature_from](check_signature_from.md): checks it under the issuer's key
- [signature_algorithm](signature_algorithm.md)
- [sgcl::crypto::x509::certificate](README.md)
