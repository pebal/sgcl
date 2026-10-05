[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::websocket::websocket

```cpp
websocket() noexcept;                     // (1)
websocket(const websocket&) = default;    // (2), implicitly declared
```

1. No connection: an operation on it is a contract violation; `operator bool` is `false`. A connection is made by
   [connect](connect.md) or [accept](accept.md).
2. The same connection: a copy shares it.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

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

    net::http::websocket none;
    net::http::websocket ws = net::http::websocket::connect(url).value();
    net::http::websocket copy = ws;
    copy.send("through the copy");
    println("{} {}", (bool)none, ws.receive()->text());
    ws.close();
    println("{}", copy.is_closed());
    srv.close();
}
```

Output:

```text
false through the copy
true
```

## See also

- [connect, async_connect](connect.md), [accept, async_accept](accept.md)
- [sgcl::net::http::websocket](README.md)
