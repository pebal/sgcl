[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [server](../server.md)

# sgcl::net::http::server::serve_tls, async_serve_tls

```cpp
/*(1)*/ expected<void, io::error> serve_tls(const string& address, const net::tls::config& c) const;
/*(2)*/ async::task<expected<void, io::error>> async_serve_tls(const string& address,
                                                              const net::tls::config& c) const noexcept;
```

Listens over TLS 1.3 on `address` with the config `c` and serves until [shutdown](shutdown.md) or [close](close.md),
Go's `ListenAndServeTLS`: [serve](serve.md) of a listener of [tls::listen](../../tls/listen.md), with the certificate
and key of the [identities](../../tls/identity.md) in `c.identities`.

The config's ALPN list is completed as Go completes `NextProtos`: `"h2"` added at the end when the server's `http2`
is on and the list has none (taken out when it is off), `"http/1.1"` added when missing, the protocols already there
kept in their order. The server's order is the preference (the first of its list the client offers), so
`{"http/1.1"}` given stays HTTP/1.1 for a client that offers both. The config itself is not changed.

1. Blocks the calling thread, a thread of the program's (main's), never a worker.
2. The same for a task.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the address to listen on, `"host:port"`; no host is every address |
| `c` | the server's TLS: its identities, groups, cipher suites, ALPN, handshake timeout ([tls::config](../../tls/config.md)) |

## Return value

Never a value. [errc](../../errc.md)`::server_closed` once the server was shut down or closed; the error of the
listen, a config refused among them (no identity: `EINVAL`, before anything listens); the error of an accept that
failed.

## Complexity

A task for each connection and its handshake, for as long as it lives.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

What a handler throws is its connection's, told to `on_error`.

## Example

The test certificate of the tree (run from its root), `h2` and `http/1.1` by ALPN:

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::config tls;
    tls.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    net::http::server srv;
    srv.route("GET /hello", [](net::http::request req, net::http::response_writer w) {
        w.write("hello over " + req.proto() + "\n");
    });
    auto served = srv.serve_tls(":8443", tls);
    println("{}", served.error().message());
}
```

A request to it:

```text
$ curl -s --cacert tests/net/tls_testdata/ca.pem https://localhost:8443/hello
hello over HTTP/2.0
$ curl -s --http1.1 --cacert tests/net/tls_testdata/ca.pem https://localhost:8443/hello
hello over HTTP/1.1
```

## See also

- [serve_tls](../serve_tls.md): one handler over https in one call, the certificate and key read from their files
- [serve](serve.md): a TLS listener of the program's, which keeps its own ALPN
- [sgcl::net::http::server](../server.md)
