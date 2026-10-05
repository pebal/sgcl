[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::revocation_mode

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    enum class revocation_mode : uint8_t {
        off,
        staple_only,
        soft_fail,
        hard_fail,
    };
}
```

Whether and how a connection checks the peer's chain for revocation once it has verified: `config::revocation`.
Each certificate of the chain but the root is checked, its issuer the next one: the leaf by the OCSP response the
peer stapled to it (RFC 6066, RFC 8446 §4.4.2.1), then by the CRLs of `config::crls`, then — `soft_fail` and
`hard_fail` — by the OCSP responders of its authority information access and the CRLs of its distribution points,
fetched over HTTP and kept in a [revocation_cache](revocation_cache/README.md); the certificates above it the same
way but the staple. A certificate found revoked ends the connection with `certificate_revoked` in every mode but
`off`; a staple that does not verify ends it with `bad_certificate_status_response`, and so does a leaf of OCSP
Must-Staple (RFC 7633) without one. What the check found is the [state](state.md)'s `revocation` and
`revocation_source`. Go does no revocation check (`off` is its behaviour and the default); Java's
`PKIXRevocationChecker` and .NET's `X509RevocationMode.Online` do what `soft_fail` and `hard_fail` do.

| Value | Description |
|---|---|
| `off` | not checked, the default: no `status_request` sent, nothing fetched, the state's `revocation` `nullopt` |
| `staple_only` | the staple when the peer sends one, Must-Staple enforced, the config's CRLs; nothing online, and a status not known is taken |
| `soft_fail` | the staple, the config's CRLs, then OCSP online and the CRLs of the distribution points; a status that cannot be had (a responder down, no source) is taken, as the browsers take it |
| `hard_fail` | the same, and every certificate but the root must be known good: one whose status cannot be had ends the connection with `certificate_unknown`, the error's reason `revocation_unknown` |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    // a leaf the CA revoked, and the CA's answer for it, which its server staples
    string dir = "tests/crypto/data/revocation/";
    net::tls::identity revoked(io::read_text(dir + "revoked.pem").value() +
                                   io::read_text(dir + "int.pem").value(),
                               crypto::read_secret(dir + "revoked.key"));
    revoked.set_ocsp_staple(io::read_file(dir + "ocsp_revoked.der").value());
    net::tls::config server_cfg;
    server_cfg.identities = {revoked};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    string address = "localhost:" + to_string(incoming.local_endpoint().port());

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(io::read_text(dir + "root.pem"));
    for (auto mode : {net::tls::revocation_mode::off, net::tls::revocation_mode::staple_only}) {
        cfg.revocation = mode;
        auto c = net::tls::connect(address, cfg);
        println("{}", c ? string("connected") : c.error().message());
    }
    incoming.close();
}
```

Sample output:

```text
connected
handshake tls tcp 127.0.0.1:52144->127.0.0.1:52143: tls: certificate is revoked
```

## See also

- [config](config.md): `revocation` and the settings of the check
- [state](state.md): `revocation`, `revocation_source`
- [revocation_source](revocation_source.md): where a status came from
- [identity::set_ocsp_staple](identity/set_ocsp_staple.md): a server's staple
- [crypto::x509::ocsp_response](../../crypto/x509-ocsp_response/README.md),
  [crypto::x509::revocation_list](../../crypto/x509-revocation_list/README.md): what is checked
- [net::tls](README.md)
