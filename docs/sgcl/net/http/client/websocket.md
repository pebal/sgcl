[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [client](README.md)

# sgcl::net::http::client::websocket, async_websocket

```cpp
expected<http::websocket, io::error> websocket(const string& url) const;                         // (1)
expected<http::websocket, io::error> websocket(const string& url,                                // (2)
                                               const http::websocket::options& o) const;
async::task<expected<http::websocket, io::error>> async_websocket(string url) const noexcept;    // (3)
async::task<expected<http::websocket, io::error>> async_websocket(                               // (4)
    string url, http::websocket::options o) const noexcept;
```

A [WebSocket](../websocket/README.md) to `url` (`ws://` or `wss://`) through this client: its [proxy](../proxy/README.md)
(an HTTP proxy as a `CONNECT` tunnel, never a forwarded request; SOCKS5 as a tunnel), its `tls` settings (the roots,
the groups, `insecure_skip_verify`) with ALPN `http/1.1` alone, its `dial` function, its `connect_timeout` for the
dial and its `max_response_header_bytes` for the answer's head. The handshake is
[websocket::connect](../websocket/connect.md)'s; the connection is not the pool's.

- (1, 3) The default [options](../websocket-options.md).
- (2, 4) Those of `o`.
- (1–2) Run the exchange on the scheduler and wait for it on the calling thread: for a thread of the program, never a
  handler.
- (3–4) The same for a task.

## Parameters

| Parameter | Description |
|---|---|
| `url` | `ws://` or `wss://` |
| `o` | the subprotocols, the fields, the limit, keep-alive, compression, the timeout, a stop |

## Return value

The connection; or the [io::error](../../../io/error/README.md) of [websocket::connect](../websocket/connect.md#return-value),
a proxy's named as the client names it (`GET ws://… (through proxy http://127.0.0.1:3128, which answered 403
Forbidden)`).

## Complexity

The dial, the proxy's and TLS's round trips, and the handshake's.

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
    srv.route("/ws", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        net::http::websocket ws = (co_await net::http::websocket::async_accept(req, w)).value();
        co_await ws.async_send("through the proxy");
        co_await ws.async_receive();
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "ws://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/ws";

    net::http::client web;
    web.proxy = net::http::proxy("http://127.0.0.1:3128");
    net::http::websocket ws = web.websocket(url).value();
    println("{}", ws.receive()->text());
    ws.close();
    srv.close();
}
```

Output:

```text
through the proxy
```

## See also

- [websocket](../websocket/README.md): the connection
- [proxy](../proxy/README.md): the proxies it goes through
- [sgcl::net::http::client](README.md)
