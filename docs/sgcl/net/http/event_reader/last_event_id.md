[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_reader](README.md)

# sgcl::net::http::event_reader::last_event_id

```cpp
string last_event_id() const noexcept;
```

The stream's last event id: the value of the last `id` field read (empty after an `id` without a value), kept across
events. What a reconnection sends as `Last-Event-ID`.

## Parameters

None.

## Return value

The last event id; empty when the stream gave none.

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

    net::http::event_reader events(web.get(url).value());
    while (auto e = events.next()) {
        if (!*e) {
            break;
        }
    }
    println("{}", events.last_event_id());
    srv.close();
}
```

Output:

```text
2
```

## See also

- [next, async_next](next.md)
- [event_source](../event_source/README.md): the reconnection
- [sgcl::net::http::event_reader](README.md)
