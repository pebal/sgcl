[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::has_basic_constraints

```cpp
bool has_basic_constraints() const noexcept;
```

Checks whether the certificate has a basicConstraints extension. A version 3 certificate without one cannot sign
another, even with a keyUsage of keyCertSign, as in Go (OpenSSL takes such a root as a trust anchor).

## Parameters

None.

## Return value

`true` when the certificate has a basicConstraints, `false` otherwise.

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

    println("{}", cert.has_basic_constraints());
}
```

Output:

```text
true
```

## See also

- [is_ca](is_ca.md), [max_path_length](max_path_length.md): what it says
- [sgcl::crypto::x509::certificate](README.md)
