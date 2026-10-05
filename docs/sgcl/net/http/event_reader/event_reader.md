[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_reader](README.md)

# sgcl::net::http::event_reader::event_reader

```cpp
event_reader() noexcept;                                                                                     // (1)
explicit event_reader(const io::reader& body, size_t max_event_bytes = default_max_event_bytes) noexcept;    // (2)
explicit event_reader(const response& r, size_t max_event_bytes = default_max_event_bytes) noexcept;         // (3)
event_reader(const event_reader&) = default;                                                                 // (4), implicitly declared
```

1. No stream: an operation on it is a contract violation; `operator bool` is `false`.
2. The events of `body`, read as they come.
3. The events of the response's body, as (2) of `r.body()`.
4. The same reader: a copy shares it.

- (2–3) An event, or a line, of more than `max_event_bytes` (1 MB by default) is `net::errc::body_too_large`.

## Parameters

| Parameter | Description |
|---|---|
| `body` | the stream of events |
| `r` | a response whose body is the stream |
| `max_event_bytes` | the most bytes of an event's data, and of a line |

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
        println("{} {} {}", (*e)->type, (*e)->id, (*e)->data);
    }
    srv.close();
}
```

Output:

```text
tick 0 event 0
tick 1 event 1
tick 2 event 2
```

## See also

- [next, async_next](next.md)
- [sgcl::net::http::event_reader](README.md)
