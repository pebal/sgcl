[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::signature_algorithm_oid

```cpp
const string& signature_algorithm_oid() const noexcept;
```

Returns the OID of the signature algorithm as dotted text, `"1.2.840.113549.1.1.11"` for SHA-256 with RSA: the
algorithm's name even when [signature_algorithm](signature_algorithm.md) is `unknown`.

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

    println("{}", cert.signature_algorithm_oid());
}
```

Output:

```text
1.2.840.10045.4.3.2
```

## See also

- [signature_algorithm](signature_algorithm.md): the algorithm by name
- [sgcl::crypto::x509::certificate](README.md)
