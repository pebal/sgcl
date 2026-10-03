[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::issuer

```cpp
const name& issuer() const noexcept;
```

Returns the distinguished name of the CA that issued the certificate: its attributes in order, the common ones by
name, and the text Go's `pkix.Name.String()` writes. A self-signed certificate's issuer is its subject.

## Parameters

None.

## Return value

The issuer's [name](../x509-name/README.md).

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

    println("{}", cert.issuer().to_string());
    println("{}", cert.issuer().common_name());
}
```

Output:

```text
CN=sgcl test CA
sgcl test CA
```

## See also

- [raw_issuer](raw_issuer.md): the bytes a chain is built on
- [subject](subject.md)
- [sgcl::crypto::x509::certificate](README.md)
