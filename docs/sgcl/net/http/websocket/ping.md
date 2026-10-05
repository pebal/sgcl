[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::websocket::ping, async_ping

```cpp
expected<void, io::error> ping(const slice<const byte>& data = {}) const;                                // (1)
async::task<expected<void, io::error>> async_ping(const slice<const byte>& data = {}) const noexcept;    // (2)
```

Sends a ping with up to 125 bytes; the peer's pong is read by [receive](receive.md), which takes it on its way and
gives no message for it. Pings of the peer are answered by receive on their own; `options::ping_interval` sends them
on a timer.

1. Writes on the calling thread, which waits: for a thread of the program, never a handler.
2. The same for a task.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the ping's payload, at most 125 bytes |

## Return value

Nothing; or the [io::error](../../../io/error/README.md): `EINVAL` for more than 125 bytes (nothing sent), the
send's errors.

## Complexity

Constant.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

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
    println("{}", ws.ping().has_value());
    println("{}", ws.ping(vector<byte>(126)).error().code() == std::errc::invalid_argument);
    ws.send("after the ping");
    println("{}", ws.receive()->text());  // the pong taken on the way
    ws.close();
    srv.close();
}
```

Output:

```text
true
true
after the ping
```

## See also

- [options](../websocket-options.md): `ping_interval`
- [sgcl::net::http::websocket](README.md)
