[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::config

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    struct config {
        string server_name;
        optional<crypto::x509::certificate_pool> roots;
        vector<tls::identity> identities;
        vector<tls::group> groups = {group::x25519_mlkem768, group::x25519, group::secp256r1,
                                     group::secp384r1};
        vector<tls::cipher> ciphers = {cipher::aes_128_gcm_sha256, cipher::chacha20_poly1305_sha256,
                                       cipher::aes_256_gcm_sha384};
        vector<string> alpn;
        bool insecure_skip_verify = false;
        duration handshake_timeout = 10 * second;
    };
}
```

`net::tls::config` is the settings of a connection, Go's `tls.Config`: a plain value, copied into the connection, so
that a change after the call reaches none made before. One type serves both sides, each reading its own members: a
client the name, the roots and `insecure_skip_verify`; a server the identities; both the groups, the cipher suites,
the ALPN protocols and the timeout, a server's lists being its preferences, in order. The defaults are Go's and the
browsers' ([the rules](README.md#the-rules)); a default config is a client that verifies the server against the
system's roots for the host it dials. A designated initializer names what differs:
`net::tls::connect(a, {.alpn = {"h2"}})`.

## Rules

- A config the handshake cannot start with is refused with `EINVAL` before a record is sent: one without a group or
  a cipher suite; a client's without a name to verify (no `server_name`, no `insecure_skip_verify`, and an address
  without a host, or [client](client.md), which has no address); a server's without an identity; one with an ALPN
  protocol of 0 or more than 255 bytes.
- It holds tracked pointers (the strings, the vectors, the pool, the identities): it lives on a stack, in a task, in
  a managed object, and in a global under a [rooted](../../core/rooted/README.md). Copies of it share the identities and
  their keys, which are never copied.

## Member objects

| Member | Description |
|---|---|
| `server_name` | the client's: the name sent as SNI (never an IP address) and checked against the leaf's names, or its IP addresses for an address (Go's `ServerName`); empty, the default, is the host of the address [connect](connect.md) was given |
| `roots` | the client's: the certificates the server's chain must lead to, a [crypto::x509::certificate_pool](../../crypto/x509.md) (Go's `RootCAs`); `nullopt`, the default, is the system's |
| `identities` | the server's: its certificate chains with their keys ([identity](identity/README.md), Go's `Certificates`), at least one; the first whose leaf is for the name the client sent is chosen, else the first; empty by default |
| `groups` | the key exchange groups, in order of preference ([group](group.md), Go's `CurvePreferences`); a client sends a key share of the first, and of X25519 beside the hybrid when the list has it; by default X25519MLKEM768, X25519, P-256, P-384 |
| `ciphers` | the cipher suites, in order of preference ([cipher](cipher.md)); Go's TLS 1.3 suites are not configurable; by default AES-128-GCM, ChaCha20-Poly1305, AES-256-GCM |
| `alpn` | the application protocols (RFC 7301), in order of preference (Go's `NextProtos`): a client offers them, a server takes the first of its list the client offers and refuses a client that offers some and none of them; empty by default, none offered and none answered |
| `insecure_skip_verify` | the client's: the server's chain taken unchecked (Go's `InsecureSkipVerify`); for tests only: with it, any machine in the path can read and change the traffic; `false` by default |
| `handshake_timeout` | how long the handshake may take: for [connect](connect.md), the lookup, the TCP connect and the handshake together; for [listen](listen.md), each connection's handshake. It ends them with `ETIMEDOUT`: zero or less at once, before anything is sent; `duration::max()` is no limit; 10 seconds by default |

## Example

A client that narrows the lists to a group and a cipher suite the server does not prefer: the server asks for the
group with a HelloRetryRequest, and both settle on them. A config without a group is refused.

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
    cfg.groups = {net::tls::group::secp384r1};
    cfg.ciphers = {net::tls::cipher::chacha20_poly1305_sha256};
    auto c = net::tls::connect(address, cfg);
    auto s = net::tls::state_of(*c);
    println("{} {}", s->group == net::tls::group::secp384r1,
            s->cipher == net::tls::cipher::chacha20_poly1305_sha256);
    c->close();

    cfg.groups = {};
    println("{}", net::tls::connect(address, cfg).error().message());
    incoming.close();
}
```

Output:

```text
true true
tls a config without cipher suites or groups: Invalid argument
```

## See also

- [connect](connect.md), [client](client.md), [listen](listen.md), [server](server.md): the functions that take it
- [identity](identity/README.md): a server's certificate and key
- [state](state.md): what the handshake settled of it
- [net::tls](README.md)
