[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [identity](README.md)

# sgcl::net::tls::identity::set_ocsp_staple

```cpp
expected<void, io::error> set_ocsp_staple(const slice<const byte>& ocsp_response) const noexcept;
```

Sets the OCSP response (RFC 6960, DER) a server staples to the identity's leaf (RFC 6066 `status_request`, in the
leaf's `CertificateEntry` of TLS 1.3, RFC 8446 §4.4.2.1): Go's `Certificate.OCSPStaple`, a client of the server that
asks for a status receiving it with the chain and needing no responder of its own. The response is checked against
the leaf first: successful, holding a status of the leaf, and, when the chain holds the leaf's issuer, verified under
it as [crypto::x509::ocsp_response::verify](../../../crypto/x509-ocsp_response/verify.md) verifies (the signer, the
CertID, the times). Empty bytes clear the staple.

The staple belongs to the identity, so every copy of it, a config's among them, staples the response from the next
handshake on. A server whose config has `ocsp_stapling` fetches the response itself and replaces it as it refreshes;
the program's then lasts until the first refresh. A staple past its `nextUpdate` is not sent.

## Parameters

| Parameter | Description |
|---|---|
| `ocsp_response` | the OCSP response's DER, as a responder answers it (`application/ocsp-response`); empty bytes clear the staple |

## Return value

Nothing, or the error, an `io::error` of op `"identity"`: `crypto::errc::malformed` for bytes that are not an OCSP
response, `crypto::errc::verification` for one that is not successful, holds no status of the leaf, or does not verify
under its issuer. The staple is unchanged then.

## Complexity

Linear in the size of the response, and the signatures of its verification.

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
    string dir = "tests/crypto/data/revocation/";
    net::tls::identity good(io::read_text(dir + "good.pem").value() +
                                io::read_text(dir + "int.pem").value(),
                            crypto::read_secret(dir + "good.key"));
    println("{}", good.set_ocsp_staple(io::read_file(dir + "ocsp_good.der").value()).has_value());
    // the answer for another certificate
    auto wrong = good.set_ocsp_staple(io::read_file(dir + "ocsp_revoked.der").value());
    println("{}", wrong.error().code() == crypto::errc::verification);

    net::tls::config server_cfg;
    server_cfg.identities = {good};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(io::read_text(dir + "root.pem"));
    cfg.revocation = net::tls::revocation_mode::staple_only;
    auto c = net::tls::connect("localhost:" + to_string(incoming.local_endpoint().port()), cfg);
    auto s = net::tls::state_of(*c);
    println("{}", s->revocation == crypto::x509::revocation_status::good);
    println("{}", s->revocation_source == net::tls::revocation_source::staple);
    c->close();
    incoming.close();
}
```

Output:

```text
true
true
true
true
```

## See also

- [ocsp_staple](ocsp_staple.md): the staple there is
- [config](../config.md): `ocsp_stapling`, a server that fetches its staples; `revocation`, a client that checks them
- [crypto::x509::ocsp_response](../../../crypto/x509-ocsp_response/README.md): the response
- [sgcl::net::tls::identity](README.md)
