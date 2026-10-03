[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](../x509-certificate.md)

# sgcl::crypto::x509::certificate::raw_subject

```cpp
slice<const byte> raw_subject() const noexcept;
```

Returns the subject's distinguished name as the bytes of its encoding: what a child's [raw_issuer](raw_issuer.md) is
compared with when a chain is built, and what a [certificate_pool](../x509-certificate_pool.md) indexes its
certificates by.

## Parameters

None.

## Return value

The DER of the subject's Name.

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

    println("{} bytes: {}", cert.raw_subject().size(), cert.subject().to_string());
}
```

Output:

```text
22 bytes: CN=localhost
```

## See also

- [subject](subject.md): the same name, read
- [raw_issuer](raw_issuer.md)
- [sgcl::crypto::x509::certificate](../x509-certificate.md)
