[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [broker](README.md)

# sgcl::net::mqtt::broker::accept, async_accept

```cpp
expected<void, io::error> accept(const http::request& r, const http::response_writer& w) const;
async::task<expected<void, io::error>> async_accept(http::request r, http::response_writer w) const noexcept;
```

A WebSocket upgrade of an [http::server](../../http/server/README.md)'s request (MQTT 5 §6, subprotocol `mqtt`, binary messages) taken over and served as one connection of the broker until it ends: a route of the server is the broker's `ws://` endpoint. A request that is no upgrade is answered (400, 426) and its error returned.

`accept` waits on the calling thread; a task awaits `async_accept`.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the request |
| `w` | its response writer |

## Return value

Nothing when the connection ended; the upgrade's error; `net::errc::server_closed` after a shutdown.

## Complexity

The connection's life.

## Exceptions

- `accept`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_accept`: none.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net/mqtt.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::mqtt::broker b;
    net::http::server srv;
    srv.route("/mqtt", [b](net::http::request r, net::http::response_writer w) -> async::task<> {
        (void)co_await b.async_accept(r, w);
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("ws://", l.local_endpoint().to_string(), "/mqtt");
    net::mqtt::client c = net::mqtt::client::connect(url).value();
    c.subscribe("t");
    c.publish("t", "over a WebSocket");
    println("{}", c.receive()->text());
    c.disconnect();
    b.close();
    srv.close();
    serving.wait();
}
```

Output:

```text
over a WebSocket
```

## See also

- [http::websocket](../../http/websocket/README.md)
- [broker](README.md)
