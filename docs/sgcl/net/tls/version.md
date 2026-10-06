[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::version

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    enum class version : uint16_t {
        tls12 = 0x0303,
        tls13 = 0x0304,
    };
}
```

The versions of TLS, by their numbers on the wire: Go's `tls.VersionTLS12` and `tls.VersionTLS13`. A
[config](config.md) bounds what a client offers with `min_version` and `max_version`, by default both: its
ClientHello offers 1.3 and 1.2 together, and a server without 1.3 answers in 1.2 (RFC 5246), as many load balancers
still do. A [state](state.md) says which one the handshake settled on. The server speaks 1.3 alone.

The client's TLS 1.2 is the modern part of it: an ephemeral ECDHE key exchange (X25519, P-256, P-384, P-521) signed by the
server's verified certificate, the AEAD suites alone ([cipher](cipher.md)), the extended master secret of RFC 7627,
which it requires (a server without it is refused with `handshake_failure`), RFC 5746's empty renegotiation_info and
no renegotiation, ALPN, client certificates. A server of 1.3 that a client offering 1.3 finds answering in 1.2 says
so in its random (RFC 8446 §4.1.3), and the client refuses the handshake with `illegal_parameter`: something between
has stripped 1.3 from the hello. A client with a [session_cache](session_cache/README.md) resumes 1.2 sessions by the
server's ticket or session id, in the abbreviated handshake of RFC 5246 §7.3.

| Value | Description |
|---|---|
| `tls12` | TLS 1.2 (RFC 5246), a client's for servers without 1.3; `config::min_version`'s default |
| `tls13` | TLS 1.3 (RFC 8446); `config::max_version`'s default, and the server's only one |

## Example

A client that requires 1.3 and one that stops at 1.2, against the namespace's server, which speaks 1.3 alone.

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

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.min_version = net::tls::version::tls13;
    auto c = net::tls::connect(address, cfg);
    println("{}", net::tls::state_of(*c)->version == net::tls::version::tls13);
    c->close();

    cfg.min_version = cfg.max_version = net::tls::version::tls12;
    println("{}", net::tls::alert_of(net::tls::connect(address, cfg).error()).has_value());
    incoming.close();
}
```

Output:

```text
true
true
```

## See also

- [config](config.md): `min_version`, `max_version`; [state](state.md): the version settled
- [cipher](cipher.md): the suites of each version
- [net::tls](README.md)
