[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::raw_issuer

```cpp
slice<const byte> raw_issuer() const noexcept;
```

Returns the issuer's distinguished name as the bytes of its encoding. A chain is built on these bytes, as Go builds
it: the parent of a certificate is one whose [raw_subject](raw_subject.md) is equal to its `raw_issuer()`, whatever
the two names read as text.

## Parameters

None.

## Return value

The DER of the issuer's Name.

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

    println("{}", cert.raw_issuer() == root.raw_subject());
    println("{}", root.raw_issuer() == root.raw_subject());
}
```

Output:

```text
true
true
```

## See also

- [issuer](issuer.md): the same name, read
- [raw_subject](raw_subject.md)
- [sgcl::crypto::x509::certificate](README.md)
