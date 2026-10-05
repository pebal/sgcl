[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::revocation_reason

```cpp
#include "sgcl/crypto/x509_revocation.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    enum class revocation_reason : uint8_t {
        unspecified = 0,
        key_compromise = 1,
        ca_compromise = 2,
        affiliation_changed = 3,
        superseded = 4,
        cessation_of_operation = 5,
        certificate_hold = 6,
        remove_from_crl = 8,
        privilege_withdrawn = 9,
        aa_compromise = 10
    };
}
```

Why a certificate was revoked: CRLReason (RFC 5280 §5.3.1), by its numbers (7 is not used), the reason of a
[CRL's entry](x509-revoked_certificate.md) and of an [OCSP response](x509-ocsp_single_response.md)'s RevokedInfo.
Go has them as the `ReasonCode` integers of `x509.RevocationListEntry` and the constants of `ocsp`.

| Value | Description |
|---|---|
| `unspecified` | no reason given, or the entry has none |
| `key_compromise` | the certificate's key is known or suspected to be compromised |
| `ca_compromise` | a CA's key is |
| `affiliation_changed` | the subject's name or other information changed |
| `superseded` | the certificate was replaced |
| `cessation_of_operation` | the certificate is no longer needed |
| `certificate_hold` | on hold: revoked until taken off again by `remove_from_crl` |
| `remove_from_crl` | a delta CRL's: the certificate is taken off hold, good again |
| `privilege_withdrawn` | a privilege the certificate held was withdrawn |
| `aa_compromise` | an attribute authority's key was compromised |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto crl = crypto::x509::revocation_list::parse(io::read_file(dir + "int.crl").value()).value();
    for (int i : range(crl.size())) {
        println("{}", crl[i].reason
            == crypto::x509::revocation_reason::certificate_hold ? "on hold" : "revoked");
    }
}
```

Output:

```text
revoked
on hold
```

## See also

- [revoked_certificate](x509-revoked_certificate.md), [ocsp_single_response](x509-ocsp_single_response.md): where it is
- [sgcl::crypto::x509](x509.md)
