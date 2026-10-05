[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::crl_distribution_points

```cpp
const vector<string>& crl_distribution_points() const noexcept;
```

Returns the URIs of the certificate's CRL distribution points (RFC 5280 §4.2.1.13): the URIs of each point's full
name, in their order, Go's `CRLDistributionPoints`. The CRL that says whether the certificate is revoked is fetched
from one of them ([revocation_list](../x509-revocation_list/README.md)); a point named relative to its issuer, the
reasons and the cRLIssuer of a point are read for their syntax only.

## Parameters

None.

## Return value

The URIs; empty when the certificate has no cRLDistributionPoints.

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
    println("{}", leaf.crl_distribution_points());
}
```

Output:

```text
["http://127.0.0.1:47812/int.crl"]
```

## See also

- [ocsp_servers](ocsp_servers.md): the responders that answer the same question one certificate at a time
- [revocation_list::status_of](../x509-revocation_list/status_of.md): a CRL read
- [sgcl::crypto::x509::certificate](README.md)
