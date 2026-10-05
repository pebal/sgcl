[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::websocket::close_status

```cpp
uint16_t close_status() const noexcept;
```

The code of the peer's close frame: the one it closed with, or the echo of this side's close. 1005 for a close frame
without a code; 0 while none has come.

## Parameters

None.

## Return value

The code.

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
    println("{}", ws.close_status());
    ws.close(1001, "going away");
    println("{}", ws.close_status());
    srv.close();
}
```

Output:

```text
0
1001
```

## See also

- [close_reason](close_reason.md)
- [close, async_close](close.md)
- [sgcl::net::http::websocket](README.md)
