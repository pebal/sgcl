[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_source](README.md)

# sgcl::net::http::event_source::event_source

```cpp
event_source() noexcept;                                                                // (1)
explicit event_source(const string& url) noexcept;                                      // (2)
event_source(const string& url, const options& o) noexcept;                             // (3)
event_source(const http::client& via, const string& url, const options& o) noexcept;    // (4)
event_source(const event_source&) = default;                                            // (5), implicitly declared
```

1. No source: an operation on it is a contract violation; `operator bool` is `false`.
2. The stream at `url`, through a client of default settings (the environment's proxy, the system's roots), with the
   default [options](../event_source-options.md): 3 s between reconnections, no end to them.
3. The same with `o`.
4. Through the client `via`: its proxy, its TLS settings, its timeouts.
5. The same source: a copy shares it.

- (2–4) Nothing is sent before the first [next](next.md).

## Parameters

| Parameter | Description |
|---|---|
| `url` | the stream's URL, `http://` or `https://` |
| `o` | the fields, the reconnection time, the reconnections, the limit, a stop |
| `via` | the client the requests go through |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /events", [](net::http::request, net::http::response_writer w) -> async::task<> {
        net::http::event_stream events(w);
        for (int i : range(3)) {
            net::http::event e;
            e.type = "tick";
            e.id = to_string(i);
            e.data = "event " + to_string(i);
            co_await events.async_send(e);
        }
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/events";
    net::http::client web;
    web.proxy = net::http::proxy();

    net::http::event_source::options o;
    o.max_reconnects = 0;
    net::http::event_source source(web, url, o);
    for (int i : range(3)) {
        println("{}", source.next()->data);
    }
    srv.close();
}
```

Output:

```text
event 0
event 1
event 2
```

## See also

- [next, async_next](next.md)
- [options](../event_source-options.md)
- [sgcl::net::http::event_source](README.md)
