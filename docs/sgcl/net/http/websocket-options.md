[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [websocket](websocket/README.md) › options

# sgcl::net::http::websocket::options

```cpp
#include "sgcl/net/http/websocket.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class websocket {
    public:
        struct options {
            vector<string> subprotocols;
            http::headers headers;
            vector<string> origins;
            size_t max_message_bytes = size_t(32) << 20;
            duration ping_interval = duration::zero();
            duration handshake_timeout = 30 * second;
            bool compression = false;
            async::stop_token stop;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::websocket::options` is how a WebSocket connection is made and kept, on either side: what a client
offers and a server takes, the fields of the handshake, the origins a server lets in, the limit of a message,
keep-alive, compression and a stop. A plain struct, its fields set by name; the forms of
[connect](websocket/connect.md) and [accept](websocket/accept.md) without it take its defaults.

## Member objects

| Member | Description |
|---|---|
| `subprotocols` | a client's offer (`Sec-WebSocket-Protocol`), in its order; a server's own, its preference first: it takes the first of them the client offered. Empty by default: none |
| `headers` | a client's fields of the request (`Origin`, `Authorization`, `Cookie`); a server's of the 101. The handshake's own fields are not taken from it. Empty by default |
| `origins` | a server's: the hosts of `Origin` it takes beside the request's own (`"app.example"`, or the origin whole, `"https://app.example"`), `"*"` for every one; a request without `Origin` is taken. Empty by default: the request's own host alone, as Go's gorilla checks it |
| `max_message_bytes` | the most bytes of a message received (after decompression), past which the connection fails with close 1009 and `net::errc::body_too_large`; 32 MB by default |
| `ping_interval` | a ping each interval, the connection closed (`ETIMEDOUT`) when nothing was heard for two; zero, the default, is none |
| `handshake_timeout` | a client's dial and handshake together, `ETIMEDOUT` past it; zero or less: none; 30 s by default |
| `compression` | permessage-deflate (RFC 7692): offered by a client, taken by a server when a client offers it; `false` by default |
| `stop` | closes the connection with 1001 (going away) when stopped, the waits ending with `ECANCELED`; none by default |

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
        net::http::websocket::options o;
        o.compression = true;
        o.headers.set("X-Server", "sgcl");
        net::http::websocket ws = (co_await net::http::websocket::async_accept(req, w, o)).value();
        while (auto m = co_await ws.async_receive()) {
            co_await ws.async_send(m->text());
        }
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "ws://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/ws";

    net::http::websocket::options o;
    o.compression = true;
    o.max_message_bytes = 1 << 20;
    o.headers.set("Authorization", "Bearer token");
    net::http::websocket ws = net::http::websocket::connect(url, o).value();
    ws.send(string(100000, 'z'));
    println("{} {}", ws.compression(), ws.receive()->data.size());
    ws.close();
    srv.close();
}
```

Output:

```text
true 100000
```

## See also

- [connect, async_connect](websocket/connect.md), [accept, async_accept](websocket/accept.md): what take them
- [websocket](websocket/README.md)
