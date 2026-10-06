[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::client, async_client

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    expected<net::connection, io::error> client(const net::connection& transport,    // (1)
                                                const config& c);
    async::task<expected<net::connection, io::error>> async_client(                  // (2)
        net::connection transport, config c) noexcept;
}
```

Makes a TLS connection of a connection there is: the client's handshake over `transport`, and the connection once it
is done and the server's chain verified. Go's `tls.Client` followed by its `Handshake`, in one call. It is for a
protocol that starts in the clear and turns to TLS on its way (STARTTLS), or a transport other than TCP: a unix
socket, a pair in memory. There is no address to take the server's name from, so `c` names it, `server_name`, or
takes the chain unchecked with `insecure_skip_verify` ([config](config.md)).

1. On the calling thread, which waits for the transport meanwhile.
2. The same for a task, which holds no worker while it waits.

The handshake replaces the transport's deadlines with its own, `c.handshake_timeout` from the call, and removes them
when it ends. From then on the transport carries the records: the program reads and writes the connection returned,
and its `close` closes the transport. A handshake that fails closes the transport; a config refused before the
handshake leaves it as it was.

## Parameters

| Parameter | Description |
|---|---|
| `transport` | the connection the records go over, a TCP one or any other |
| `c` | the settings: the name, the roots, the groups, the cipher suites, the ALPN protocols, the timeout |

## Return value

The TLS connection over `transport`, its handshake done. Or the [io::error](../../io/error/README.md):

- `EINVAL` for a config the handshake cannot start with: no group or no cipher suite, no `server_name` without
  `insecure_skip_verify`, an ALPN protocol of 0 or more than 255 bytes;
- `ETIMEDOUT` when `c.handshake_timeout` passed (`is_timeout()`);
- an error of the category `"tls"` for an alert of either side or a chain that did not verify ([alert_of](alert_of.md),
  [certificate_reason](certificate_reason.md)); `alert::ech_required` for a `c.ech_config_list` the server did not
  take, whose `retry_configs` [ech_retry_configs](ech_retry_configs.md) gives for a second try;
- the transport's own errors (`io::errc::closed`, `ECONNRESET`).

## Complexity

One round trip of the handshake, two when the server asks for another group with a HelloRetryRequest; the
verification of the chain.

## Exceptions

- (1) `std::system_error` when the handshake has to wait and the thread of the reactor, or of the timers, which its
  first use starts, cannot be made.
- (2) None.

## Example

A protocol that starts in the clear: the client asks for TLS with a line of text, the server agrees, and both make
TLS of the TCP connection they have.

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

async::task<> serve(net::listener incoming, net::tls::config cfg) {
    auto plain = co_await incoming.async_accept();
    if (!plain) {
        co_return;
    }
    co_await plain->async_read_line();  // STARTTLS
    co_await plain->async_write(string("go ahead\n"));
    auto c = co_await net::tls::async_server(*plain, cfg);
    if (c) {
        co_await c->async_write(string("now over tls\n"));
        co_await c->async_close();
    }
}

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(serve(incoming, server_cfg));

    net::connection plain = net::tcp::connect(incoming.local_endpoint().to_string());
    plain.write("STARTTLS\n");
    println("{}", **plain.read_line());

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.server_name = "localhost";
    auto c = net::tls::client(plain, cfg);
    if (!c) {
        eprintln("{}", c.error().message());
        return 1;
    }
    println("{}", **c->read_line());
    c->close();
    serving.wait();
    incoming.close();
}
```

Output:

```text
go ahead
now over tls
```

A task makes TLS of one end of a pair in memory, the server the other end:

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

async::task<> serve(net::connection transport, net::tls::config cfg) {
    auto c = co_await net::tls::async_server(transport, cfg);
    if (c) {
        co_await c->async_write(string("hello through memory\n"));
        co_await c->async_close();
    }
}

async::task<string> fetch(net::connection transport, net::tls::config cfg) {
    auto c = co_await net::tls::async_client(transport, cfg);
    if (!c) {
        co_return c.error().message();
    }
    auto line = co_await c->async_read_line();
    co_await c->async_close();
    co_return **line;
}

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ed25519.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ed25519.key"))};
    auto [near, far] = net::connection::in_memory();
    auto serving = async::spawn(serve(far, server_cfg));

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.server_name = "localhost";
    println("{}", async::run(fetch(near, cfg)));
    serving.wait();

    auto [other, unused] = net::connection::in_memory();
    auto refused = net::tls::client(other, {});  // no name to verify
    println("{} {}", refused.error().code() == std::errc::invalid_argument, other.is_closed());
}
```

Output:

```text
hello through memory
true false
```

## See also

- [connect, async_connect](connect.md): a TCP connection and this handshake in one call
- [server, async_server](server.md): the other side
- [config](config.md): the settings; [state_of](state_of.md): what the handshake settled
- [connection::in_memory](../connection/in_memory.md): a pair of connections without sockets
- [net::tls](README.md)
