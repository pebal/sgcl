[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [websocket](websocket/README.md) › message

# sgcl::net::http::websocket::message

```cpp
#include "sgcl/net/http/websocket.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class websocket {
    public:
        struct message {
            bool binary = false;
            vector<byte> data;

            string text() const noexcept;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::websocket::message` is a message [receive](websocket/receive.md) gave: its frames put together,
decompressed when it came compressed. A text message's bytes are UTF-8, checked as they came; `text()` gives them as
a string. A value: its vector is a handle, and a copy shares the bytes.

## Member objects

| Member | Description |
|---|---|
| `binary` | a message of bytes (opcode 2); `false` for text (opcode 1) |
| `data` | the message's bytes |

## Member functions

| Function | Description |
|---|---|
| `text` | `string text() const noexcept;` the bytes as a string (a copy of them) |

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
        co_await ws.async_send("zażółć");
        co_await ws.async_send(vector<byte>{byte(0xCA), byte(0xFE)});
        co_await ws.async_receive();
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "ws://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/ws";

    net::http::websocket ws = net::http::websocket::connect(url).value();
    net::http::websocket::message text = ws.receive().value();
    net::http::websocket::message bytes = ws.receive().value();
    println("{} {} {}", text.binary, text.data.size(), text.text());
    println("{} {}", bytes.binary, bytes.data.size());
    ws.close();
    srv.close();
}
```

Output:

```text
false 10 zażółć
true 2
```

## See also

- [receive, async_receive](websocket/receive.md): what gives it
- [websocket](websocket/README.md)
