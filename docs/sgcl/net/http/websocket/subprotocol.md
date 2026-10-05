[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::websocket::subprotocol

```cpp
string subprotocol() const noexcept;
```

The subprotocol of the connection (`Sec-WebSocket-Protocol`): on a client's, the one the server chose of those it
offered; on a server's, the one it chose. Empty for none.

## Parameters

None.

## Return value

The subprotocol, or an empty string.

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
        net::http::websocket::options o;
        o.subprotocols = {"chat.v1"};
        net::http::websocket ws = (co_await net::http::websocket::async_accept(req, w, o)).value();
        co_await ws.async_receive();
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "ws://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/ws";

    net::http::websocket::options o;
    o.subprotocols = {"chat.v2", "chat.v1"};
    net::http::websocket ws = net::http::websocket::connect(url, o).value();
    println("{}", ws.subprotocol());
    ws.close();
    srv.close();
}
```

Output:

```text
chat.v1
```

## See also

- [options](../websocket-options.md): `subprotocols`
- [sgcl::net::http::websocket](README.md)
