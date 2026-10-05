[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::websocket::accept, async_accept

```cpp
static expected<websocket, io::error> accept(const request& r, const response_writer& w);                      // (1)
static expected<websocket, io::error> accept(const request& r, const response_writer& w, const options& o);    // (2)
static async::task<expected<websocket, io::error>> async_accept(request r, response_writer w) noexcept;        // (3)
static async::task<expected<websocket, io::error>> async_accept(request r, response_writer w,                  // (4)
                                                                options o) noexcept;
```

A server's connection of a request's upgrade, in its handler: the checks of RFC 6455 §4.2.1 (a `GET` of HTTP/1.1,
`Upgrade: websocket`, `Connection: upgrade`, a key of 16 bytes in base64, version 13), the `Origin` (none, the
request's own host, or one of `o.origins`), then the connection taken over from the server
([hijack](../response_writer/hijack.md)) and answered `101 Switching Protocols` with the accept of the key, the
subprotocol chosen (the first of `o.subprotocols` the client offered), permessage-deflate when `o.compression` is on
and the client offered it, and the fields of `o.headers`. The server sends nothing more on the connection and does
not close it: the connection is the handler's, which may keep it after it returns.

A request refused is answered through the writer: 405 for another method, 426 with `Sec-WebSocket-Version: 13` for
another version, 403 for an `Origin` not allowed, 400 for the rest, 505 for a request of HTTP/2 (RFC 8441 is not
served); the handler returns, the server sends the answer.

- (1, 3) The default [options](../websocket-options.md).
- (2, 4) Those of `o`.
- (1–2) Write the answer on the calling thread, which waits for it: not in a handler, which runs on a worker.
- (3–4) The same for a task: a handler writes `co_await net::http::websocket::async_accept(req, w)`.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the request the handler was given |
| `w` | its writer, nothing written to it yet |
| `o` | the subprotocols taken, the fields of the answer, the origins, the limit, keep-alive, compression, a stop |

## Return value

The connection; or the [io::error](../../../io/error/README.md): `net::errc::websocket_handshake` for a request
refused (what is wrong in its path), `EINVAL` for a field of `o.headers` that cannot be written, `io::errc::closed`
for a writer that sent its head already, the error of the 101's write.

## Complexity

The checks, linear in the fields of the request; one write.

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
    srv.route("/chat", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        net::http::websocket::options o;
        o.subprotocols = {"chat.v2", "chat.v1"};
        auto ws = co_await net::http::websocket::async_accept(req, w, o);
        if (ws) {
            co_await ws->async_send("hello " + ws->subprotocol());
            co_await ws->async_receive();  // until the client's close
        }
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "ws://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/chat";

    net::http::websocket::options co;
    co.headers.set("Origin", "https://elsewhere.example");
    auto refused = net::http::websocket::connect(url, co);
    println("{}", refused.error().path().contains("403"));

    co.headers = net::http::headers();
    co.subprotocols = {"chat.v1"};
    net::http::websocket ws = net::http::websocket::connect(url, co).value();
    println("{}", ws.receive()->text());
    ws.close();
    srv.close();
}
```

Output:

```text
true
hello chat.v1
```

## See also

- [connect, async_connect](connect.md): the client's side
- [response_writer::hijack](../response_writer/hijack.md): the takeover under it
- [options](../websocket-options.md)
- [sgcl::net::http::websocket](README.md)
