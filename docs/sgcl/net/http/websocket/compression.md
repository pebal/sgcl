[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::websocket::compression

```cpp
bool compression() const noexcept;
```

Whether permessage-deflate (RFC 7692) was agreed: both sides had `options::compression` on, and the server took the
client's offer. The messages are then compressed and decompressed on their own.

## Parameters

None.

## Return value

Whether the messages go compressed.

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

    net::http::websocket::options o;
    o.compression = true;
    net::http::websocket ws = net::http::websocket::connect(url, o).value();
    println("{}", ws.compression());  // the server did not take it
    ws.close();
    srv.close();
}
```

Output:

```text
false
```

## See also

- [options](../websocket-options.md): `compression`
- [sgcl::net::http::websocket](README.md)
