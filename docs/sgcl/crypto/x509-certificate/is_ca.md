[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](../x509-certificate.md)

# sgcl::crypto::x509::certificate::is_ca

```cpp
bool is_ca() const noexcept;
```

Returns the cA of the basicConstraints: whether the certificate's key may sign certificates. Every issuer of a chain
but a root needs it.

## Parameters

None.

## Return value

`true` when there is a basicConstraints with cA TRUE, `false` otherwise.

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

    println("{} {}", root.is_ca(), cert.is_ca());
}
```

Output:

```text
true false
```

## See also

- [has_basic_constraints](has_basic_constraints.md)
- [allows](allows.md): keyCertSign, which an issuer with a keyUsage needs too
- [sgcl::crypto::x509::certificate](../x509-certificate.md)
