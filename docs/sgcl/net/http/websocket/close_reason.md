[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::websocket::close_reason

```cpp
string close_reason() const noexcept;
```

The reason of the peer's close frame, UTF-8 checked; empty for none, and while no close has come.

## Parameters

None.

## Return value

The reason.

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
        co_await ws.async_close(1013, "busy, try later");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "ws://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/ws";

    net::http::websocket ws = net::http::websocket::connect(url).value();
    auto m = ws.receive();  // the server's close, answered
    println("{}", ws.close_reason());
    srv.close();
}
```

Output:

```text
busy, try later
```

## See also

- [close_status](close_status.md)
- [sgcl::net::http::websocket](README.md)
