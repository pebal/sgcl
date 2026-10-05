[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_stream](README.md)

# sgcl::net::http::event_stream::event_stream

```cpp
explicit event_stream(const response_writer& w) noexcept;    // (1)
event_stream(const event_stream&) = default;                 // (2), implicitly declared
```

1. A stream over the response of `w`: its fields set (`Content-Type: text/event-stream`, `Cache-Control: no-cache`,
   `X-Accel-Buffering: no`, which tells a proxy of nginx's kind to hold nothing back), nothing sent yet. The head goes
   with the first event, or at once by [flush](flush.md).
2. The same stream: a copy shares the writer.

## Parameters

| Parameter | Description |
|---|---|
| `w` | the writer of the handler, nothing sent through it yet |

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
    srv.route("GET /events", [](net::http::request, net::http::response_writer w) -> async::task<> {
        net::http::event_stream events(w);
        co_await events.async_send("hello");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/events";
    net::http::client web;
    web.proxy = net::http::proxy();
    println("{}", web.get(url)->text().value().trim());
    srv.close();
}
```

Output:

```text
data: hello
```

## See also

- [send, async_send](send.md)
- [response_writer](../response_writer/README.md)
- [sgcl::net::http::event_stream](README.md)
