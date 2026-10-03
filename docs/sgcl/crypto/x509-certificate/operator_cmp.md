[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::operator==, operator!= (sgcl::crypto::x509::certificate)

```cpp
friend bool operator==(const certificate& a, const certificate& b) noexcept;
```

Compares two certificates: equal when they are of the same bytes, two copies of one parse or two parses of one
encoding. `!=` is its negation, written by the compiler from `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the certificates to compare |

## Return value

`true` when the certificates have the same bytes, `false` otherwise.

## Complexity

Constant for two copies of one parse; linear in the length of the certificates otherwise.

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

    auto again = crypto::x509::certificate::parse(cert.raw());
    println("{} {}", again == cert, root != cert);
}
```

Output:

```text
true true
```

## See also

- [raw](raw.md): the bytes compared
- [certificate_pool::contains](../x509-certificate_pool/contains.md): a certificate in a pool
- [sgcl::crypto::x509::certificate](README.md)
