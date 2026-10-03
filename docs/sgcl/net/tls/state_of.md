[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::state_of

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    optional<state> state_of(const net::connection& c) noexcept;
}
```

Returns what the handshake of `c` settled: the cipher suite, the group, the server's name, the ALPN protocol and the
peer's chain ([state](state.md)), Go's `Conn.ConnectionState`. It is there for a connection of this namespace — one
from [connect](connect.md), [client](client.md), [server](server.md) or the `accept` of a listener of
[listen](listen.md) — whose handshake is done before the program has it, so the state is complete from the start.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the connection |

## Return value

A copy of the state, or `nullopt` for a connection without TLS (a TCP one, a pair in memory) and for an empty handle.

## Complexity

Constant, and linear in the length of the peer's chain, whose certificates are copied as handles.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

async::task<> serve(net::listener incoming) {
    auto c = co_await incoming.async_accept();
    if (c) {
        auto s = net::tls::state_of(*c);
        println("server: name {}, {} certificates of the client", s->server_name,
                s->peer_certificates.size());
        co_await c->async_close();
    }
}

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    auto serving = async::spawn(serve(incoming));

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    auto c = net::tls::connect("localhost:" + to_string(incoming.local_endpoint().port()), cfg);
    serving.wait();
    auto s = net::tls::state_of(*c);
    println("client: name {}, the leaf for {}", s->server_name,
            s->peer_certificates[0].dns_names()[0]);
    c->close();
    incoming.close();

    auto [near, far] = net::connection::in_memory();
    println("{}", net::tls::state_of(near).has_value());
}
```

Output:

```text
server: name localhost, 0 certificates of the client
client: name localhost, the leaf for localhost
false
```

## See also

- [state](state.md): what it gives
- [config](config.md): what the handshake was offered
- [net::tls](README.md)
