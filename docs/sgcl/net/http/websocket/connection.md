[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::websocket::connection

```cpp
net::connection connection() const noexcept;
```

The connection the WebSocket goes over: its endpoints, its TLS state ([tls::state_of](../../tls/state_of.md)). The
WebSocket reads and writes it; a program that reads or writes it too breaks the framing, and its deadlines end the
WebSocket's waits as they end any.

## Parameters

None.

## Return value

The connection.

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
    println("{}", ws.connection().remote_endpoint().port() == listener.local_endpoint().port());
    ws.close();
    srv.close();
}
```

Output:

```text
true
```

## See also

- [connection](../../connection/README.md)
- [sgcl::net::http::websocket](README.md)
