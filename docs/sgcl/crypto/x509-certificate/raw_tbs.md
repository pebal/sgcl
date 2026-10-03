[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::raw_tbs

```cpp
slice<const byte> raw_tbs() const noexcept;
```

Returns the TBSCertificate as the bytes of its encoding: the part of the certificate its issuer signed, everything but
the signature and its algorithm. A check of a signature by other means than
[check_signature_from](check_signature_from.md) starts from these bytes.

## Parameters

None.

## Return value

The DER of the TBSCertificate, a part of [raw](raw.md).

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

    println("{} bytes of {}", cert.raw_tbs().size(), cert.raw().size());
}
```

Output:

```text
585 bytes of 675
```

## See also

- [signature](signature.md), [signature_algorithm](signature_algorithm.md): the signature over these bytes
- [sgcl::crypto::x509::certificate](README.md)
