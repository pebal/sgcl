[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::websocket::is_closed

```cpp
bool is_closed() const noexcept;
```

Whether the connection under it is closed: by this side's close, by the peer's, by a failure, a stop or keep-alive.

## Parameters

None.

## Return value

Whether it is closed.

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

    net::http::websocket ws = net::http::websocket::connect(url).value();
    println("{}", ws.is_closed());
    ws.close();
    println("{}", ws.is_closed());
    srv.close();
}
```

Output:

```text
false
true
```

## See also

- [close, async_close](close.md)
- [sgcl::net::http::websocket](README.md)
