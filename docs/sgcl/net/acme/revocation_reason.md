[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::revocation_reason

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
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
        aa_compromise = 10,
    };
}
```

The reasons of a revocation (RFC 5280 §5.3.1), the codes [revoke](client/revoke.md) sends (RFC 8555 §7.6); a CA takes
some of them (Let's Encrypt: unspecified, key_compromise, superseded, cessation_of_operation) and refuses the rest
with `errc::bad_revocation_reason`. Go's `acme.CRLReasonCode`.

| Value | Description |
|---|---|
| `unspecified` | no reason given (none sent) |
| `key_compromise` | the certificate's private key was compromised |
| `ca_compromise` | the CA's key was compromised |
| `affiliation_changed` | the subject's name or affiliation changed |
| `superseded` | the certificate was replaced |
| `cessation_of_operation` | the names are no longer in use |
| `certificate_hold` | suspended |
| `remove_from_crl` | off a CRL |
| `privilege_withdrawn` | a privilege of the certificate withdrawn |
| `aa_compromise` | an attribute authority compromised |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    println("{}", int(net::acme::revocation_reason::superseded));
}
```

Output:

```text
4
```

## See also

- [client::revoke](client/revoke.md)
- [net::acme](README.md)
