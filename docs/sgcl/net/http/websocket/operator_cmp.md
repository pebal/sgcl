[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::operator==, operator!= (sgcl::net::http::websocket)

```cpp
friend bool operator==(const websocket& a, const websocket& b) noexcept;
```

Whether two handles are the same connection: copies of one handle are, two connections never are, and two handles
without a connection are. `!=` is its negation, written by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles |

## Return value

Whether they hold the same connection.

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
    srv.route("/ws", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        net::http::websocket ws = (co_await net::http::websocket::async_accept(req, w)).value();
        co_await ws.async_receive();
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "ws://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/ws";

    net::http::websocket a = net::http::websocket::connect(url).value();
    net::http::websocket b = net::http::websocket::connect(url).value();
    net::http::websocket copy = a;
    println("{} {}", a == copy, a != b);
    a.close();
    b.close();
    srv.close();
}
```

Output:

```text
true true
```

## See also

- [sgcl::net::http::websocket](README.md)
