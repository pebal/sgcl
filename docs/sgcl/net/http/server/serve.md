[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [server](../server.md)

# sgcl::net::http::server::serve, async_serve

```cpp
expected<void, io::error> serve(const string& address) const;                                 // (1)
async::task<expected<void, io::error>> async_serve(const string& address) const noexcept;     // (2)
expected<void, io::error> serve(const net::listener& l) const;                                // (3)
async::task<expected<void, io::error>> async_serve(const net::listener& l) const noexcept;    // (4)
```

Serves until [shutdown](shutdown.md) or [close](close.md): accepts each connection and runs it in a task of its own
on the scheduler, by the [rules](../server.md#rules) of the server and with its settings as they are at the call.

- (1–2) Listens on `address` as [tcp::listen](../../tcp/listen.md) does (`":8080"` every address of both families,
  `"127.0.0.1:8080"` one), Go's `ListenAndServe`.
- (3–4) Serves the connections of a listener the program made: of [tcp::listen](../../tcp/listen.md), of
  [unix_domain::listen](../../unix_domain/listen.md), or of [tls::listen](../../tls/listen.md), whose accept gives
  connections after their handshake (each handshake in a task of its own within the config's `handshake_timeout`,
  one that fails dropped), Go's `Serve`. Nothing of the server changes over TLS: routes, limits, timeouts and
  shutdown are the same. A TLS listener keeps the ALPN it was made with: `alpn = {"h2", "http/1.1"}` in its config
  for HTTP/2, where [serve_tls](serve_tls.md) completes the list by itself. The listener is closed by `shutdown` and
  `close`.

(1) and (3) block the calling thread, a thread of the program's (main's), never a worker; (2) and (4) are for a
task.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the address to listen on, `"host:port"`; no host is every address, port 0 a free one |
| `l` | the listener whose connections are served |

## Return value

Never a value. [errc](../../errc.md)`::server_closed` once the server was shut down or closed, and at once for a
call after either (the listener given is closed then); the error of the listen (an address in use, an address that
is not one); the error of an accept that failed, which `on_error` is told of too.

## Complexity

A task for each connection, for as long as it lives.

## Exceptions

- (1), (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4) None.

What a handler throws is its connection's, told to `on_error`.

## Example

The server on an address, until the program is stopped:

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /hello/{name}", [](net::http::request req, net::http::response_writer w) {
        w.write("hello, " + req.path_value("name") + "\n");
    });
    auto served = srv.serve(":8080");
    println("{}", served.error().message());
}
```

A request to it:

```text
$ curl http://localhost:8080/hello/world
hello, world
```

A listener of TLS with the test certificate of the tree (`tests/net/tls_testdata`: a leaf for localhost and
127.0.0.1, signed by a CA of its own), and a client that trusts that CA, by name and by address:

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::config tls;
    tls.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    tls.alpn = {"http/1.1"};

    net::http::server srv;
    srv.route("GET /{name}", [](net::http::request req, net::http::response_writer w) {
        w.write("hello, " + req.path_value("name") + " over " + req.proto() + "\n");
    });
    net::listener incoming = net::tls::listen("127.0.0.1:0", tls);
    auto serving = async::spawn(srv.async_serve(incoming));

    net::http::client web;
    web.tls.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    auto port = to_string(incoming.local_endpoint().port());
    net::http::response by_name = web.get("https://localhost:" + port + "/name");
    string one = by_name.text();
    print(one);
    // checked against the certificate's addresses
    net::http::response by_address = web.get("https://127.0.0.1:" + port + "/address");
    string two = by_address.text();
    print(two);

    srv.shutdown();
    println(serving.wait().error().code() == net::errc::server_closed);
}
```

Output:

```text
hello, name over HTTP/1.1
hello, address over HTTP/1.1
true
```

The same listener with `"h2"` in its ALPN: a client that offers it gets HTTP/2, one that does not HTTP/1.1:

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::config tls;
    tls.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    tls.alpn = {"h2", "http/1.1"};

    net::http::server srv;
    srv.route("POST /count", [](net::http::request req,
                                net::http::response_writer w) -> async::task<> {
        auto body = co_await req.async_bytes();
        w.write(req.proto() + ": " + to_string(body ? body->size() : 0) + " bytes\n");
    });
    net::listener incoming = net::tls::listen("127.0.0.1:0", tls);
    auto serving = async::spawn(srv.async_serve(incoming));

    net::http::client web;  // offers h2 first
    web.tls.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    auto url = "https://localhost:" + to_string(incoming.local_endpoint().port()) + "/count";
    net::http::response counted =
        web.post(url, "application/octet-stream", string(std::string(100000, 'x')));
    string text = counted.text();
    print(text);

    net::http::client old;
    old.http2 = false;
    old.tls.roots = web.tls.roots;
    net::http::response counted1 = old.post(url, "text/plain", "abc");
    string text1 = counted1.text();
    print(text1);

    srv.shutdown();
    println(serving.wait().error().code() == net::errc::server_closed);
}
```

Output:

```text
HTTP/2.0: 100000 bytes
HTTP/1.1: 3 bytes
true
```

## See also

- [serve_tls](serve_tls.md): over TLS on an address, the ALPN completed
- [shutdown](shutdown.md), [close](close.md): what ends it
- [serve](../serve.md): the files of a directory in one call
- [sgcl::net::http::server](../server.md)
