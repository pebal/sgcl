[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::issuing_certificate_urls

```cpp
const vector<string>& issuing_certificate_urls() const noexcept;
```

Returns the URIs where the certificate's issuer's certificate is published: the `id-ad-caIssuers` locations of its
authorityInfoAccess (RFC 5280 §4.2.2.1), in their order, Go's `IssuingCertificateURL`. A server that sends an
incomplete chain leaves a client this way to the missing certificate.

## Parameters

None.

## Return value

The URIs; empty when the certificate names none.

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
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    println("{}", leaf.issuing_certificate_urls());
}
```

Output:

```text
["http://127.0.0.1:47812/int.cer"]
```

## See also

- [ocsp_servers](ocsp_servers.md): the other locations of the same extension
- [sgcl::crypto::x509::certificate](README.md)
