[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [public_key](../x509-public_key.md)

# sgcl::crypto::x509::public_key::algorithm

```cpp
const string& algorithm() const noexcept;
```

Returns the OID of the SubjectPublicKeyInfo's algorithm as dotted text, whatever the kind: `"1.2.840.113549.1.1.1"`
(rsaEncryption), `"1.2.840.10045.2.1"` (id-ecPublicKey, the curve in its parameters), `"1.3.101.112"` (Ed25519). For a
key of `key_kind::none` it is the one name of the algorithm there is.

## Parameters

None.

## Return value

The dotted OID.

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

    auto root_text = io::read_text("tests/net/tls_testdata/ca.pem");  // the tree's test CA
    crypto::x509::certificate root = crypto::x509::certificate::from_pem(root_text);

    println("{} {}", cert.public_key().algorithm(), root.public_key().algorithm());
}
```

Output:

```text
1.2.840.113549.1.1.1 1.2.840.10045.2.1
```

## See also

- [kind](kind.md)
- [sgcl::crypto::x509::public_key](../x509-public_key.md)
