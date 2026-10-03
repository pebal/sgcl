[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::cipher

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    enum class cipher : uint16_t {
        aes_128_gcm_sha256 = 0x1301,
        aes_256_gcm_sha384 = 0x1302,
        chacha20_poly1305_sha256 = 0x1303,
    };
}
```

The cipher suites of TLS 1.3, by their numbers on the wire (RFC 8446 §B.4): the AEAD that seals the records and the
hash of the key schedule. A [config](config.md) lists them in order of preference, `config::ciphers`, by default
AES-128-GCM, ChaCha20-Poly1305, AES-256-GCM; a [state](state.md) says which one the handshake settled on, the first
of the server's list the client offers. Go fixes its TLS 1.3 suites; here a config narrows and reorders them.

| Value | Description |
|---|---|
| `aes_128_gcm_sha256` | `TLS_AES_128_GCM_SHA256`: AES-128 in GCM ([aes_gcm](../../crypto/aes_gcm/README.md)) and SHA-256; first by default |
| `aes_256_gcm_sha384` | `TLS_AES_256_GCM_SHA384`: AES-256 in GCM and SHA-384; last by default |
| `chacha20_poly1305_sha256` | `TLS_CHACHA20_POLY1305_SHA256`: ChaCha20-Poly1305 ([chacha20_poly1305](../../crypto/chacha20_poly1305/README.md)) and SHA-256; second by default |

## Example

The server's preference decides among the suites both have:

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
    server_cfg.ciphers = {net::tls::cipher::aes_256_gcm_sha384,
                          net::tls::cipher::aes_128_gcm_sha256};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    auto c = net::tls::connect("localhost:" + to_string(incoming.local_endpoint().port()), cfg);
    println("{}", net::tls::state_of(*c)->cipher == net::tls::cipher::aes_256_gcm_sha384);
    c->close();
    incoming.close();
}
```

Output:

```text
true
```

## See also

- [config](config.md): the lists of preference; [state](state.md): the suite settled
- [group](group.md): the key exchange groups
- [net::tls](README.md)
