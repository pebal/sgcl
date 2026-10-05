[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::websocket::connect, async_connect

```cpp
static expected<websocket, io::error> connect(const string& url);                                    // (1)
static expected<websocket, io::error> connect(const string& url, const options& o);                  // (2)
static async::task<expected<websocket, io::error>> async_connect(string url) noexcept;               // (3)
static async::task<expected<websocket, io::error>> async_connect(string url, options o) noexcept;    // (4)
```

A client's connection to `url`: the opening handshake of RFC 6455 §4.1 over a connection of the HTTP client of the
process ([http::download](../download.md)'s, its settings the defaults: the environment's proxy, the system's roots).
A request `GET` with `Upgrade: websocket`, a random key, version 13, the subprotocols and the fields of `o`, and
permessage-deflate offered when `o.compression` is on; the answer must be 101 with the accept of the key, a
subprotocol offered or none, an extension offered or none. Redirects are not followed. A client of the program's
settings is [client::websocket](../client/websocket.md).

- (1, 3) The default [options](../websocket-options.md): no subprotocol, no field, 32 MB a message, 30 s for the
  handshake.
- (2, 4) Those of `o`.
- (1–2) Run the exchange on the scheduler and wait for it on the calling thread: for a thread of the program, never
  a handler.
- (3–4) The same for a task.

`ws://` and `wss://` (and `http://`, `https://` as the same); `wss://` is TLS 1.3 with ALPN `http/1.1` alone.
Through an HTTP proxy the connection goes through a `CONNECT` tunnel, never as a forwarded request.

## Parameters

| Parameter | Description |
|---|---|
| `url` | `ws://` or `wss://`, the host, the port, the path and the query |
| `o` | the subprotocols, the fields, the limit, keep-alive, compression, the timeout, a stop |

## Return value

The connection; or the [io::error](../../../io/error/README.md), its operation `GET` and its path the URL:
`net::errc::invalid_url`, `net::errc::unsupported_scheme` for a URL of another scheme, the dial's error
(`ECONNREFUSED`, a TLS error, a proxy's), `net::errc::malformed_response`, `net::errc::websocket_handshake` for an
answer that is not an upgrade (the status and what is wrong with it after the URL: `(404: the status is not 101)`),
`ETIMEDOUT` past `o.handshake_timeout`, `ECANCELED` for `o.stop`.

## Complexity

The dial and one round trip.

## Exceptions

- (1–2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3–4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("/echo", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        net::http::websocket ws = (co_await net::http::websocket::async_accept(req, w)).value();
        while (auto m = co_await ws.async_receive()) {
            m->binary ? co_await ws.async_send(m->data) : co_await ws.async_send(m->text());
        }
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "ws://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/echo";

    net::http::websocket ws = net::http::websocket::connect(url).value();
    ws.send("first");
    println("{}", ws.receive()->text());
    ws.close();

    auto wrong = net::http::websocket::connect("ws://127.0.0.1:1/");
    println("{}", wrong.error().code() == std::errc::connection_refused);
    auto scheme = net::http::websocket::connect("ftp://example.com/");
    println("{}", scheme.error().code() == net::errc::unsupported_scheme);
    srv.close();
}
```

Output:

```text
first
true
true
```

## See also

- [client::websocket](../client/websocket.md): through a client of the program's settings
- [accept, async_accept](accept.md): the server's side
- [options](../websocket-options.md)
- [sgcl::net::http::websocket](README.md)
