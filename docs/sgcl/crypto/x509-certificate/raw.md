[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::raw

```cpp
slice<const byte> raw() const noexcept;
```

Returns the whole certificate as the bytes of its encoding, the DER [parse](parse.md) read: what a TLS handshake
sends, and what the fingerprint of a certificate is the digest of. The slice keeps the bytes alive.

## Parameters

None.

## Return value

The DER of the certificate.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    println("{} bytes", cert.raw().size());
    println("SHA-256 {}", encoding::hex::encode(crypto::sha256::of(cert.raw())));
}
```

Output:

```text
675 bytes
SHA-256 c5b24bc679408204477ffbf17a0e938f0635a31de427da87709b827dcbd345dd
```

## See also

- [raw_tbs](raw_tbs.md): the part the signature covers
- [operator==](operator_cmp.md): certificates of the same bytes
- [sgcl::crypto::x509::certificate](README.md)
