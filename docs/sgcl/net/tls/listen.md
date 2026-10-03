[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::listen, async_listen

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    expected<net::listener, io::error> listen(const string& address, const config& c);    // (1)
    async::task<expected<net::listener, io::error>> async_listen(string address,          // (2)
                                                                 config c) noexcept;
}
```

Binds `address` as [tcp::listen](../tcp/listen.md) does (`":8443"`, `"127.0.0.1:0"`) and gives a TCP listener whose
`accept` gives connections whose handshake is done: a plain [net::listener](../listener.md), with `accept`,
`async_accept`, `close` and `local_endpoint`, so that [net::http::server](../http/server.md) serves it unchanged
(https). Go's `tls.Listen` and `tls.NewListener`, but Go's `Accept` gives a connection before its handshake, which
runs at the first read; here `accept` gives it after.

1. On the calling thread.
2. The same for a task.

The listener runs an accept loop of its own: each connection's handshake ([server](server.md)) runs in a task of its
own, bounded by `c.handshake_timeout`, so that a client that is slow, or never speaks, holds no other up. A handshake
that fails or times out is dropped: its connection is closed, the alert sent, and nothing reaches `accept` (no log in
v1). The connections whose handshake is done wait for `accept` in a queue of 16. `close()` stops the accept loop,
ends the accepts in progress with `io::errc::closed` and closes the connections that were ready and not taken. The
listener lives until it is closed.

`c` needs at least one identity; its `groups`, `ciphers` and `alpn` are the server's preferences, in order, and
`roots`, `server_name` and `insecure_skip_verify` are the client's and are not read ([config](config.md)).

## Parameters

| Parameter | Description |
|---|---|
| `address` | `"host:port"` or `":port"`, as [tcp::listen](../tcp/listen.md) takes it; port 0 is a free one |
| `c` | the settings of every connection: the identities, the groups, the cipher suites, the ALPN protocols, the timeout |

## Return value

The listener, bound and accepting. Or the [io::error](../../io/error.md): `EINVAL` for a config a handshake cannot
start with (no identity, no group or no cipher suite, an ALPN protocol of 0 or more than 255 bytes), checked before
the address is bound; what [tcp::listen](../tcp/listen.md) returns for an address it cannot bind (`EADDRINUSE`).

## Complexity

The bind of [tcp::listen](../tcp/listen.md); each connection then costs a task and its handshake.

## Exceptions

- (1) `std::system_error` when the accept loop starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

A client whose protocol the server does not speak is dropped, and one that never speaks holds no other up: `accept`
gives only the connection whose handshake is done.

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

async::task<> serve(net::listener incoming) {
    for (;;) {
        auto c = co_await incoming.async_accept();
        if (!c) {
            println("accept: {}", c.error().is_closed());
            co_return;
        }
        co_await c->async_write(net::tls::state_of(*c)->alpn + "\n");
        co_await c->async_close();
    }
}

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    server_cfg.alpn = {"echo/1"};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    auto serving = async::spawn(serve(incoming));
    string address = "localhost:" + to_string(incoming.local_endpoint().port());

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.alpn = {"h2"};
    println("{}", net::tls::connect(address, cfg).has_value());

    net::connection silent = net::tcp::connect(address);  // never says a word
    cfg.alpn = {"echo/1"};
    auto c = net::tls::connect(address, cfg);
    println("{}", **c->read_line());

    incoming.close();
    serving.wait();
    silent.close();
}
```

Output:

```text
false
echo/1
accept: true
```

A task binds a listener, or learns why it cannot:

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

async::task<> start(net::tls::config cfg) {
    auto l = co_await net::tls::async_listen("127.0.0.1:0", cfg);
    if (!l) {
        println("{}", l.error().message());
        co_return;
    }
    println("listening");
    l->close();
}

int main() {
    async::run(start({}));

    net::tls::config cfg;
    cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ed25519.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ed25519.key"))};
    async::run(start(cfg));
}
```

Output:

```text
tls a server's config without an identity: Invalid argument
listening
```

## See also

- [server, async_server](server.md): the handshake each connection gets
- [net::listener](../listener.md): what it returns; [tcp::listen](../tcp/listen.md): the listener inside
- [config](config.md), [identity](identity.md): the settings and the certificate
- [net::http::server](../http/server.md): https over such a listener
- [net::tls](README.md)
