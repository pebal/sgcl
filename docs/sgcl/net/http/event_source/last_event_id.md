[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_source](README.md)

# sgcl::net::http::event_source::last_event_id

```cpp
string last_event_id() const noexcept;
```

The id of the last event it gave: what a reconnection sends as `Last-Event-ID`; empty while none had one.

## Parameters

None.

## Return value

The last event id.

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

    net::http::event_source source(web, url, net::http::event_source::options());
    source.next();
    source.next();
    println("{}", source.last_event_id());
    source.close();
    srv.close();
}
```

Output:

```text
1
```

## See also

- [next, async_next](next.md)
- [sgcl::net::http::event_source](README.md)
