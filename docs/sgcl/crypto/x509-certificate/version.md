[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::version

```cpp
int version() const noexcept;
```

Returns the version of the certificate: 3 for nearly every certificate in use, 1 or 2 for an old one, which has no
extensions.

## Parameters

None.

## Return value

1, 2 or 3.

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

    println("{}", cert.version());
}
```

Output:

```text
3
```

## See also

- [extensions](extensions.md): what a version 3 certificate has
- [sgcl::crypto::x509::certificate](README.md)
