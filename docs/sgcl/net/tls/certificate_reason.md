[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::certificate_reason

```cpp
#include "sgcl/net/tls/error.h"   // or "sgcl/net/tls.h"

namespace sgcl::net::tls {
    optional<crypto::x509::reason> certificate_reason(const io::error& e) noexcept;
}
```

Returns why the server's chain did not verify, for an error that says so: Go's `tls.CertificateVerificationError`,
with the reasons of [crypto::x509](../../crypto/x509-reason.md). The chain is verified by the
client against `config::roots`, or the system's roots, for the server's name ([config](config.md)); the alert sent
to the server follows from the reason. The text of the error is the reason's, worded as Go words it:

| Reason | Text |
|---|---|
| `expired`, `not_yet_valid` | `tls: certificate has expired or is not yet valid` |
| `unknown_authority` | `tls: certificate signed by unknown authority` |
| `hostname_mismatch` | `tls: certificate is not valid for the server name` |
| `name_constraints` | `tls: certificate is outside a name constraint of its issuer` |
| `unsupported_algorithm` | `tls: certificate of an algorithm not supported` |
| `insecure_algorithm` | `tls: certificate signed with an insecure algorithm` |
| `invalid_signature` | `tls: certificate signature does not verify` |
| `too_many_intermediates` | `tls: too many intermediate certificates` |
| `path_length` | `tls: certificate chain past a path length constraint` |
| `not_a_ca` | `tls: certificate issued by one that is not a CA` |
| `missing_cert_sign` | `tls: certificate issued by a CA not allowed to sign certificates` |
| `incompatible_usage` | `tls: certificate specifies an incompatible key usage` |
| `unhandled_critical_extension` | `tls: certificate with an unhandled critical extension` |
| `too_many_constraints` | `tls: certificate chain with too many name constraints` |

## Parameters

| Parameter | Description |
|---|---|
| `e` | the error |

## Return value

The reason, or `nullopt` for an error of another kind: an alert of either side, an error of another category.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    string address = "localhost:" + to_string(incoming.local_endpoint().port());

    auto untrusted = net::tls::connect(address);  // the system's roots
    auto reason = net::tls::certificate_reason(untrusted.error());
    println("{}", reason == crypto::x509::reason::unknown_authority);

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.server_name = "example.com";
    auto misnamed = net::tls::connect(address, cfg);
    reason = net::tls::certificate_reason(misnamed.error());
    println("{}", reason == crypto::x509::reason::hostname_mismatch);
    println("{}", misnamed.error().code().message());
    incoming.close();
}
```

Output:

```text
true
true
tls: certificate is not valid for the server name
```

## See also

- [crypto::x509](../../crypto/x509.md): the verification and its reasons
- [alert_of](alert_of.md), [is_remote](is_remote.md): the other failures of a handshake
- [config](config.md): the roots and the name
- [net::tls](README.md)
