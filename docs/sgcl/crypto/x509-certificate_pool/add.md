[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_pool](../x509-certificate_pool.md)

# sgcl::crypto::x509::certificate_pool::add

```cpp
void add(const certificate& c) noexcept;
```

Adds the certificate, unless the pool has one of the same bytes already. The certificate is shared, not copied: a
pool holds certificates as any variable does, a pointer each. Every copy of the pool sees it.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the certificate |

## Return value

None.

## Complexity

Linear in the length of the certificate: its SHA-256 is computed.

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

    crypto::x509::certificate_pool pool;
    auto shared = pool;
    pool.add(cert);
    pool.add(cert);
    println("{} {}", pool.size(), shared.size());
}
```

Output:

```text
1 1
```

## See also

- [append_pem](append_pem.md): the certificates of a PEM text
- [contains](contains.md)
- [sgcl::crypto::x509::certificate_pool](../x509-certificate_pool.md)
