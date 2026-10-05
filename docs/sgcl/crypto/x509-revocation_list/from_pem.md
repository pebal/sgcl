[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::from_pem

```cpp
static expected<revocation_list, error> from_pem(const string& text) noexcept;
```

Reads the first `X509 CRL` block of a PEM text (RFC 7468), whatever is around it, as [certificate::from_pem](../x509-certificate/from_pem.md) reads its `CERTIFICATE`, and [parse](parse.md)s its bytes.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the PEM text |

## Return value

The list, or `errc::malformed` for a text with no `X509 CRL` block, or for its bytes as [parse](parse.md) refuses them.

## Complexity

Linear in the size of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto crl = crypto::x509::revocation_list::parse(io::read_file(dir + "int.crl").value()).value();
    string base64 = encoding::base64::standard.encode(crl.raw());
    string pem = "-----BEGIN X509 CRL-----\n" + base64 + "\n-----END X509 CRL-----\n";
    println("{}", crypto::x509::revocation_list::from_pem(pem)->size());
}
```

Output:

```text
2
```

## See also

- [parse](parse.md): a list in DER
- [sgcl::crypto::x509::revocation_list](README.md)
