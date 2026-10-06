[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::group

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    enum class group : uint16_t {
        x25519_mlkem768 = 0x11EC,
        x25519 = 0x001D,
        secp256r1 = 0x0017,
        secp384r1 = 0x0018,
        secp521r1 = 0x0019,
    };
}
```

The groups of the key exchange, by their numbers on the wire (the `NamedGroup` of RFC 8446): Go's `tls.CurveID`.
A [config](config.md) lists them in order of preference, `config::groups`, by default the first four in the order of the
declaration; a [state](state.md) says which one the handshake settled on. A client sends a key share of the first
of its list, and of X25519 beside the hybrid, so that a server of either answers in one round trip; a server
that takes neither asks for one of its own with a HelloRetryRequest.

| Value | Description |
|---|---|
| `x25519_mlkem768` | X25519MLKEM768 (draft-ietf-tls-ecdhe-mlkem): the post-quantum hybrid, an ML-KEM-768 encapsulation ([mlkem](../../crypto/mlkem.md)) and an X25519 exchange together, safe while either holds; first by default, as in Go and the browsers |
| `x25519` | X25519 (RFC 7748, [x25519](../../crypto/x25519.md)); its share goes beside the hybrid's in a default ClientHello |
| `secp256r1` | ECDHE over NIST P-256 ([p256](../../crypto/p256.md)); a server of it alone costs a default client a HelloRetryRequest |
| `secp384r1` | ECDHE over NIST P-384 ([p384](../../crypto/p384.md)); the same |
| `secp521r1` | ECDHE over NIST P-521 ([p521](../../crypto/p521.md)); not in the default list, as in Chrome: taken when a config lists it |

## Example

A server that takes X25519 alone: the client's first ClientHello carries its share, and no HelloRetryRequest is
needed.

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
    server_cfg.groups = {net::tls::group::x25519};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    println("{}", cfg.groups[0] == net::tls::group::x25519_mlkem768);
    auto c = net::tls::connect("localhost:" + to_string(incoming.local_endpoint().port()), cfg);
    println("{}", net::tls::state_of(*c)->group == net::tls::group::x25519);
    println("{:#06x}", uint16_t(net::tls::group::x25519_mlkem768));
    c->close();
    incoming.close();
}
```

Output:

```text
true
true
0x11ec
```

## See also

- [config](config.md): the lists of preference; [state](state.md): the group settled
- [cipher](cipher.md): the cipher suites
- [net::tls](README.md)
