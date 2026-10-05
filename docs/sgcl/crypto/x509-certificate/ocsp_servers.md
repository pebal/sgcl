[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::ocsp_servers

```cpp
const vector<string>& ocsp_servers() const noexcept;
```

Returns the URIs of the certificate's OCSP responders: the `id-ad-ocsp` locations of its authorityInfoAccess (RFC 5280
§4.2.2.1), in their order, Go's `OCSPServer`. An OCSP request of the certificate goes to one of them
([ocsp_request](../x509-ocsp_request/README.md)); [net::tls](../../net/tls/revocation_mode.md) asks them when its config
says so. A location of another kind than a URI is passed over.

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
    println("{}", leaf.ocsp_servers());
}
```

Output:

```text
["http://127.0.0.1:47811"]
```

## See also

- [issuing_certificate_urls](issuing_certificate_urls.md): the other locations of the same extension
- [crl_distribution_points](crl_distribution_points.md): where its CRLs are
- [sgcl::crypto::x509::certificate](README.md)
