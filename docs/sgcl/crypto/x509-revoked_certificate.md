[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::revoked_certificate

```cpp
#include "sgcl/crypto/x509_revocation.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    struct revoked_certificate {
        slice<const byte> serial_number;
        time::datetime revocation_time;
        revocation_reason reason = revocation_reason::unspecified;
        optional<time::datetime> invalidity_date;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::x509::revoked_certificate` is one entry of a [CRL](x509-revocation_list/README.md): a certificate its
issuer revoked, Go's `x509.RevocationListEntry`. A value made of the list's bytes when
[operator\[\]](x509-revocation_list/operator_at.md) or [lookup](x509-revocation_list/lookup.md) asks for it; the list
keeps the entries themselves only as an index into its bytes.

## Member objects

| Member | Description |
|---|---|
| `serial_number` | the certificate's serial number, its INTEGER's bytes as [certificate::serial_number](x509-certificate/serial_number.md) has them: a view of the list's bytes, which it keeps alive |
| `revocation_time` | when the certificate was revoked (its revocationDate), in UTC |
| `reason` | why ([revocation_reason](x509-revocation_reason.md)): the entry's reasonCode, `unspecified` when it has none |
| `invalidity_date` | since when the key is known or suspected to be compromised (the entry's invalidityDate), in UTC; `nullopt` when the entry does not say |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto crl = crypto::x509::revocation_list::parse(io::read_file(dir + "int.crl").value()).value();
    auto revoked = crypto::x509::certificate::from_pem(io::read_text(dir + "revoked.pem")).value();

    crypto::x509::revoked_certificate entry = crl.lookup(revoked).value();
    println("{}", entry.reason == crypto::x509::revocation_reason::key_compromise);
    println("{}", entry.revocation_time <= crl.this_update());
    println("{}", entry.invalidity_date.has_value());
}
```

Output:

```text
true
true
false
```

## See also

- [revocation_list::lookup](x509-revocation_list/lookup.md), [operator\[\]](x509-revocation_list/operator_at.md): what makes it
- [sgcl::crypto::x509](x509.md)
