[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::state

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    struct state {
        tls::version version = version::tls13;
        tls::cipher cipher = cipher::aes_128_gcm_sha256;
        tls::group group = group::x25519;
        string server_name;
        string alpn;
        crypto::x509::chain peer_certificates;
        bool resumed = false;
        optional<crypto::x509::revocation_status> revocation;
        tls::revocation_source revocation_source = revocation_source::none;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`net::tls::state` is what the handshake of a connection settled: the version, the cipher suite, the group, the
name, the ALPN protocol, the peer's chain, whether a session was resumed, and the revocation status of the peer's
leaf when the config asked for its check. Go's `tls.ConnectionState`, its
`Version`, `CipherSuite`, `CurveID`, `ServerName`, `NegotiatedProtocol`, `PeerCertificates` and `DidResume`. [state_of](state_of.md) gives it for a connection of the namespace; it
is a value, a copy of what the connection keeps.

## Member objects

| Member | Description |
|---|---|
| `version` | the version ([version](version.md)): `tls13`, or `tls12` for a client whose server has no 1.3; a server's is always `tls13` |
| `cipher` | the cipher suite ([cipher](cipher.md)), of the version |
| `group` | the group of the key exchange ([group](group.md)), the one the server asked for with a HelloRetryRequest when there was one; over 1.2 the curve of the server's key exchange, and for a resumed 1.2 session, which makes none, the curve of the handshake that made it |
| `server_name` | a client's: the name the chain was checked for, `config::server_name` or the host dialed; a server's: the name the client sent (SNI), empty when it sent none, as a client of an IP address does |
| `alpn` | the application protocol both chose (RFC 7301); empty when none was |
| `peer_certificates` | the peer's chain as it was sent, the leaf first, a [crypto::x509::chain](../../crypto/x509.md): a client's, the server's; a server's, the client's when it asked for one ([client_auth](client_auth.md)) and got it, else empty. A resumed session's is the chain of the handshake that made it |
| `resumed` | whether the handshake resumed a session of a ticket, with no certificate sent (Go's `DidResume`): over 1.2 the abbreviated handshake, by a ticket or a session id, with no key exchange either; `false` for a full handshake |
| `revocation` | the revocation status of the peer's leaf as `config::revocation` checked it ([revocation_mode](revocation_mode.md)): `good`, or `unknown` when no source said (a revoked one ends the connection); the certificates above it were checked too. `nullopt` when nothing was checked: `revocation_mode::off`, a resumed session, a chain not verified |
| `revocation_source` | where the leaf's status came from ([revocation_source](revocation_source.md)): the peer's staple, an OCSP responder, a CRL; `none` when it is `unknown` or not checked |

The defaults of `version`, `cipher` and `group` stand only in a state made by the program; one from
[state_of](state_of.md) holds what was settled.

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
        net::tls::identity(io::read_text("tests/net/tls_testdata/rsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/rsa.key"))};
    server_cfg.alpn = {"http/1.1"};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.alpn = {"h2", "http/1.1"};
    auto c = net::tls::connect("127.0.0.1:" + to_string(incoming.local_endpoint().port()), cfg);
    net::tls::state s = net::tls::state_of(*c).value();
    println("{}", s.cipher == net::tls::cipher::aes_128_gcm_sha256);
    println("{}", s.group == net::tls::group::x25519_mlkem768);
    println("{} {}", s.server_name, s.alpn);
    println("{} {}", s.peer_certificates.size(), s.peer_certificates[0].dns_names());
    c->close();
    incoming.close();
}
```

Output:

```text
true
true
127.0.0.1 http/1.1
1 ["localhost"]
```

## See also

- [state_of](state_of.md): the function that gives it
- [config](config.md): what the handshake was offered
- [crypto::x509](../../crypto/x509.md): the certificates of the chain
- [net::tls](README.md)
