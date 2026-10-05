[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::revocation_status

```cpp
#include "sgcl/crypto/x509_revocation.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    enum class revocation_status : uint8_t {
        good,
        revoked,
        unknown
    };
}
```

What is known of a certificate's revocation: OCSP's CertStatus (RFC 6960 §4.2.1, Go's `ocsp.Good`, `ocsp.Revoked`,
`ocsp.Unknown`), which an [ocsp_single_response](x509-ocsp_single_response.md) holds; a
[CRL](x509-revocation_list/status_of.md) says `good` or `revoked`. A TLS connection's
[state](../net/tls/state.md) gives the status of its peer's leaf in this type.

| Value | Description |
|---|---|
| `good` | not revoked: the responder or the list knows the certificate and it is not revoked |
| `revoked` | revoked, for good or on hold |
| `unknown` | the responder does not know the certificate (OCSP); for a TLS connection, no source said |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "unknown.pem")).value();
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto response = crypto::x509::ocsp_response::parse(
        io::read_file(dir + "ocsp_unknown.der").value()).value();
    println("{}",
        response.verify(leaf, issuer)->status == crypto::x509::revocation_status::unknown);
}
```

Output:

```text
true
```

## See also

- [ocsp_single_response](x509-ocsp_single_response.md): the status of OCSP
- [revocation_list::status_of](x509-revocation_list/status_of.md): the status of a CRL
- [sgcl::crypto::x509](x509.md)
