[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](../x509-certificate.md)

# sgcl::crypto::x509::certificate::extensions

```cpp
const vector<extension>& extensions() const noexcept;
```

Returns every extension of the certificate in the order of the certificate, each its OID, whether it is critical and
the bytes of its value: those the module reads into fields (basicConstraints, keyUsage, the subject alternative
names, …) and those it does not.

## Parameters

None.

## Return value

The [extensions](../x509-extension.md).

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

    for (auto& e : cert.extensions()) {
        println("{}{} ({} bytes)", e.oid, e.critical ? " critical" : "", e.value.size());
    }
}
```

Output:

```text
2.5.29.17 (37 bytes)
2.5.29.19 critical (2 bytes)
2.5.29.15 critical (4 bytes)
2.5.29.37 (12 bytes)
2.5.29.14 (22 bytes)
2.5.29.35 (24 bytes)
```

## See also

- [unhandled_critical_extensions](unhandled_critical_extensions.md): the critical ones not read
- [sgcl::crypto::x509::certificate](../x509-certificate.md)
