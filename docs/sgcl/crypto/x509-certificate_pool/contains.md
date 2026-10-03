[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_pool](README.md)

# sgcl::crypto::x509::certificate_pool::contains

```cpp
[[nodiscard]] bool contains(const certificate& c) const noexcept;
```

Checks whether the pool has this very certificate: one of the same bytes, by the SHA-256 of the bytes.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the certificate |

## Return value

`true` when the pool has the certificate, `false` otherwise.

## Complexity

Linear in the length of the certificate.

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

    auto pool = crypto::x509::certificate_pool::from_file("tests/net/tls_testdata/ca.pem");
    println("{} {}", pool->contains(root), pool->contains(cert));
}
```

Output:

```text
true false
```

## See also

- [add](add.md)
- [certificate::operator==](../x509-certificate/operator_cmp.md)
- [sgcl::crypto::x509::certificate_pool](README.md)
