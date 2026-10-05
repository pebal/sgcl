[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::client_auth

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    enum class client_auth : uint8_t {
        none,
        request,
        require,
    };
}
```

Whether a server asks the client for a certificate (mTLS, RFC 8446 §4.3.2): `config::client_auth`, Go's
`tls.ClientAuthType`. A server that asks sends a `CertificateRequest` naming the signature schemes it takes and the
authorities of its `config::client_roots`; a chain the client sends is verified against those roots (the system's
when the config has none) for client authentication, the extended key usage `clientAuth`, and its
`CertificateVerify` checked under the leaf's key. The chain is the server's [state](state.md)'s
`peer_certificates`, and an [http::request](../http/request/README.md)'s through [tls](../http/request/tls.md). A
resumed session asks nothing again: it keeps the chain of the handshake that made it. A client sends the first of
its config's `identities` that the request takes, or none.

| Value | Description |
|---|---|
| `none` | no certificate asked for, the default; a client's identities are not sent (Go's `NoClientCert`) |
| `request` | asked for: a chain the client sends must verify, and a client without one is taken, its `peer_certificates` empty (Go's `VerifyClientCertIfGiven`) |
| `require` | asked for and required: a client without one is refused with `certificate_required`, one whose chain does not verify with `unknown_ca`, `certificate_expired` or `bad_certificate` (Go's `RequireAndVerifyClientCert`) |

A value of none of these is refused by the server's functions with `EINVAL`, as a config they cannot start with.

## Example

A server that requires a certificate of the test CA: a client with one is let in and named, a client without one
learns it at its first read, since its own handshake was done before the server read its empty `Certificate`.

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
    server_cfg.client_auth = net::tls::client_auth::require;
    server_cfg.client_roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    string address = "localhost:" + to_string(incoming.local_endpoint().port());

    net::tls::config cfg;
    cfg.roots = server_cfg.client_roots;
    cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/client_ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/client_ecdsa.key"))};
    auto c = net::tls::connect(address, cfg);
    net::connection served = incoming.accept().value();
    println("{}", net::tls::state_of(served)->peer_certificates[0].subject().common_name());
    served.write("welcome\n");
    println("{}", c->read_line()->value());
    c->close();
    served.close();

    cfg.identities = {};
    auto refused = net::tls::connect(address, cfg);
    println("{}", refused->read_line().error().message());
    incoming.close();
}
```

Sample output:

```text
sgcl test client ecdsa
welcome
read tls 127.0.0.1:50000->127.0.0.1:50001: remote error: tls: certificate required
```

## See also

- [config](config.md): `client_auth`, `client_roots` and the client's `identities`
- [identity](identity/README.md): a client's certificate and key
- [state](state.md): the client's chain on the server's side
- [alert](alert.md): `certificate_required`
- [net::tls](README.md)
