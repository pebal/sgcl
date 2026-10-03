[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::server, async_server

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    expected<net::connection, io::error> server(const net::connection& transport,    // (1)
                                                const config& c);
    async::task<expected<net::connection, io::error>> async_server(                  // (2)
        net::connection transport, config c) noexcept;
}
```

Makes a TLS connection of a connection there is, the server's side: the handshake over `transport`, and the
connection once the client's `Finished` is verified. Go's `tls.Server` followed by its `Handshake`, in one call. It
is for a connection accepted from a listener of TCP or of another transport, or one that turns to TLS on its way;
[listen](listen.md) does it for every connection of a TCP listener.

1. On the calling thread, which waits for the transport meanwhile.
2. The same for a task, which holds no worker while it waits.

`c` needs at least one identity; its `groups`, `ciphers` and `alpn` are the server's preferences, in order, and
`roots`, `server_name` and `insecure_skip_verify` are the client's and are not read ([config](config.md)). What the
server chooses of what the client offers is in [the rules](README.md#the-rules) of the namespace: the suite, the
group (a HelloRetryRequest when the client sent no share of one the server takes), the identity by the name the
client sent, the ALPN protocol.

The handshake replaces the transport's deadlines with its own, `c.handshake_timeout` from the call, and removes them
when it ends. From then on the transport carries the records: the program reads and writes the connection returned,
and its `close` closes the transport. A handshake that fails closes the transport, after the alert it sends; a
config refused before the handshake leaves it as it was.

## Parameters

| Parameter | Description |
|---|---|
| `transport` | the connection the records go over, a TCP one or any other |
| `c` | the settings: the identities, the groups, the cipher suites, the ALPN protocols, the timeout |

## Return value

The TLS connection over `transport`, its handshake done. Or the [io::error](../../io/error/README.md):

- `EINVAL` for a config the handshake cannot start with: no identity, no group or no cipher suite, an ALPN protocol
  of 0 or more than 255 bytes;
- `ETIMEDOUT` when `c.handshake_timeout` passed (`is_timeout()`): a client slow, or one that never speaks;
- an error of the category `"tls"` for an alert of either side ([alert_of](alert_of.md)): `handshake_failure` for
  no common suite, group or signature scheme, `no_application_protocol` for no common ALPN protocol,
  `protocol_version` for a client without TLS 1.3;
- the transport's own errors (`io::errc::closed`, `ECONNRESET`).

## Complexity

One round trip of the handshake, two with a HelloRetryRequest; one signature of the identity's key.

## Exceptions

- (1) `std::system_error` when the handshake has to wait and the thread of the reactor, or of the timers, which its
  first use starts, cannot be made.
- (2) None.

## Example

A server on the calling thread, over a connection a TCP listener accepted, and a client in a task:

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

async::task<string> fetch(string address, net::tls::config cfg) {
    auto c = co_await net::tls::async_connect(address, cfg);
    if (!c) {
        co_return c.error().message();
    }
    auto line = co_await c->async_read_line();
    co_await c->async_close();
    co_return **line;
}

int main() {
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.alpn = {"h2", "http/1.1"};
    auto fetching = async::spawn(
        fetch("localhost:" + to_string(incoming.local_endpoint().port()), cfg));

    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/rsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/rsa.key"))};
    server_cfg.alpn = {"http/1.1"};
    net::connection accepted = incoming.accept();
    auto c = net::tls::server(accepted, server_cfg);
    if (!c) {
        eprintln("{}", c.error().message());
        return 1;
    }
    auto s = net::tls::state_of(*c);
    println("{} {}", s->server_name, s->alpn);
    c->write("welcome\n");
    c->close();
    println("{}", fetching.wait());
    incoming.close();
}
```

Output:

```text
localhost http/1.1
welcome
```

A server in a task refuses a client whose protocols it does not speak; both sides see the alert:

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

async::task<> serve(net::connection transport, net::tls::config cfg) {
    auto c = co_await net::tls::async_server(transport, cfg);
    if (!c) {
        println("server: {}", c.error().code().message());
    }
}

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    server_cfg.alpn = {"echo/1"};
    auto [near, far] = net::connection::in_memory();
    auto serving = async::spawn(serve(far, server_cfg));

    net::tls::config cfg;
    cfg.insecure_skip_verify = true;
    cfg.alpn = {"h2"};
    auto c = net::tls::client(near, cfg);
    serving.wait();
    auto alert = net::tls::alert_of(c.error());
    println("client: {} {}", alert == net::tls::alert::no_application_protocol,
            net::tls::is_remote(c.error()));
    println("{} {}", far.is_closed(), near.is_closed());
}
```

Output:

```text
server: tls: no application protocol
client: true true
true true
```

## See also

- [listen, async_listen](listen.md): a TCP listener that runs this handshake for each connection
- [client, async_client](client.md): the other side
- [config](config.md), [identity](identity/README.md): the settings and the certificate
- [net::tls](README.md)
