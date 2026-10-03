[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::signature_algorithm

```cpp
x509::signature_algorithm signature_algorithm() const noexcept;
```

Returns the algorithm the issuer signed the certificate with, by name: those over MD2, MD5 and SHA-1 are named so that
they can be refused by name, and one the module does not know is `unknown`.

## Parameters

None.

## Return value

The [signature_algorithm](../x509-signature_algorithm.md).

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

    auto algorithm = cert.signature_algorithm();
    println("{}", algorithm == crypto::x509::signature_algorithm::ecdsa_with_sha256);
}
```

Output:

```text
true
```

## See also

- [signature_algorithm_oid](signature_algorithm_oid.md): the algorithm's OID
- [signature](signature.md)
- [sgcl::crypto::x509::certificate](README.md)
