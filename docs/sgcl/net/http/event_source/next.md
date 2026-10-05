[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_source](README.md)

# sgcl::net::http::event_source::next, async_next

```cpp
expected<event, io::error> next() const;                                // (1)
async::task<expected<event, io::error>> async_next() const noexcept;    // (2)
```

The next [event](../event.md), connecting and reconnecting as a browser's `EventSource` does (WHATWG HTML §9.2.3): a
`GET` with `Accept: text/event-stream`, `Cache-Control: no-cache`, the options' fields and `Last-Event-ID` when an
event had an id; when the stream ends or the connection fails, another after the reconnection time (the stream's
`retry`, or `options::retry`), the reader of the new stream starting from the last event id. It gives up for good,
and every next after gives the same error: on a status other than 200 (204 is the server's way to tell it to stop),
a `Content-Type` other than `text/event-stream`, `options::max_reconnects` reconnections in a row with no event, an
event past the limit, the stop, or [close](close.md).

1. On the calling thread, which waits: for a thread of the program, never a handler.
2. The same for a task.

## Parameters

None.

## Return value

The event; or the [io::error](../../../io/error/README.md) it gave up with: `net::errc::http_status` (the status in
its path), `net::errc::malformed_response` (another `Content-Type`), `io::errc::unexpected_eof` (no more
reconnections), `net::errc::body_too_large`, `ECANCELED` for the stop, `io::errc::closed` after close.

## Complexity

One request a connection; the reading, linear in the stream.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

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
    o.max_reconnects = 1;
    o.retry = 50 * millisecond;
    net::http::event_source source(web, url, o);
    for (int i : range(5)) {
        auto e = source.next();
        if (!e) {
            println("{}", e.error().code() == io::errc::unexpected_eof);
            break;
        }
        println("{} {}", e->id, e->data);  // the server starts over: it does not read Last-Event-ID
    }
    srv.close();
}
```

Output:

```text
0 event 0
1 event 1
2 event 2
0 event 0
1 event 1
```

## See also

- [last_event_id](last_event_id.md), [retry](retry.md)
- [event_reader](../event_reader/README.md): a stream read once
- [sgcl::net::http::event_source](README.md)
