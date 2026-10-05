[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::ocsp_single_response

```cpp
#include "sgcl/crypto/x509_revocation.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    struct ocsp_single_response {
        hash_id hash = hash_id::sha1;
        vector<byte> issuer_name_hash;
        vector<byte> issuer_key_hash;
        vector<byte> serial_number;
        revocation_status status = revocation_status::unknown;
        time::datetime this_update;
        optional<time::datetime> next_update;
        optional<time::datetime> revocation_time;
        revocation_reason reason = revocation_reason::unspecified;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::x509::ocsp_single_response` is one SingleResponse of an [OCSP response](x509-ocsp_response/README.md)
(RFC 6960 §4.2.1): the status of one certificate, named by its CertID, and the times the status is current for. Go's
`ocsp.Response` holds one such status itself; a response may hold several, each a value of this type in
[responses](x509-ocsp_response/responses.md), and [verify](x509-ocsp_response/verify.md) gives the one of a certificate,
verified.

## Member objects

| Member | Description |
|---|---|
| `hash` | the hash of the CertID ([hash_id](hash_id.md)) |
| `issuer_name_hash` | the hash of the DER of the certificate's issuer name |
| `issuer_key_hash` | the hash of the issuer's public key bits |
| `serial_number` | the certificate's serial number, its INTEGER's bytes |
| `status` | the status ([revocation_status](x509-revocation_status.md)): `good`, `revoked`, `unknown` (the responder does not know the certificate) |
| `this_update` | when the status was known to be right, in UTC |
| `next_update` | when newer information will be there, in UTC; `nullopt` when the response has none (always fresh at the responder) |
| `revocation_time` | `revoked`: when the certificate was revoked; `nullopt` otherwise |
| `reason` | `revoked`: why ([revocation_reason](x509-revocation_reason.md)), `unspecified` when the response does not say |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto response = crypto::x509::ocsp_response::parse(
        io::read_file(dir + "ocsp_good.der").value()).value();

    crypto::x509::ocsp_single_response single = response.verify(leaf, issuer).value();
    println("{}", single.status == crypto::x509::revocation_status::good);
    println("{}", single.serial_number == leaf.serial_number());
    println("{}", single.revocation_time.has_value());
}
```

Output:

```text
true
true
false
```

## See also

- [ocsp_response::verify](x509-ocsp_response/verify.md): the one of a certificate, verified
- [ocsp_response::responses](x509-ocsp_response/responses.md): all of them
- [sgcl::crypto::x509](x509.md)
