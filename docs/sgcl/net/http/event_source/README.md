[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::event_source

```cpp
#include "sgcl/net/http/events.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class event_source {
    public:
        struct options;   // the fields, the reconnection time, the reconnections, the limit, a stop
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::event_source` is a client of a stream of Server-Sent Events that reconnects, as a browser's
`EventSource` does (the WHATWG HTML Standard, §9.2.2-§9.2.3): [next](next.md) gives the next [event](../event.md),
connecting at the first and again whenever the stream ends or the connection fails, after the reconnection time the
stream set, with `Last-Event-ID` the id of the last event it gave, so that a server can resume where it was. It goes
through a [client](../client/README.md): one of default settings, or one given, with its proxy and TLS settings.

A handle of one word: copies are the same source. Nothing is sent before the first next.

## Rules

- **The request**: `GET` with `Accept: text/event-stream`, `Cache-Control: no-cache`, the fields of
  [options](../event_source-options.md), `Last-Event-ID` once an event had an id.
- **Reconnection**: when the stream ends or its connection fails, after the stream's last `retry`, or
  `options::retry` (3 s) while it gave none; the new stream's reader starts from the last event id. A reconnection
  that gives no event counts against `options::max_reconnects` (no end by default); an event clears the count.
- **Giving up**, for good, as `EventSource` fails the connection: a status other than 200 (204 No Content is how a
  server tells it to stop), a `Content-Type` other than `text/event-stream`, the reconnections spent, an event past
  `options::max_event_bytes`, the stop, [close](close.md). Every next after it gives the same error.
- `next` blocks a thread of the program; a task awaits `async_next`.

## Member types

| Type | Definition |
|---|---|
| [options](../event_source-options.md) | the fields, the reconnection time, the reconnections, the limit, a stop |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](event_source.md) | a source of a URL, through a client of default settings or one given |
| [next, async_next](next.md) | the next event, reconnecting as needed |
| [last_event_id](last_event_id.md) | the id of the last event |
| [retry](retry.md) | the reconnection time now |
| [close](close.md) | closes the stream, no more reconnections |
| [operator bool](operator_bool.md) | whether there is a source |

## Example

A server that resumes after `Last-Event-ID`, a stream at a time:

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /feed", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        int from = req.header("Last-Event-ID").empty() ? 1 : parse<int>(req.header("Last-Event-ID")).value() + 1;
        net::http::event_stream events(w);
        net::http::event e;
        e.retry = 20 * millisecond;
        e.id = to_string(from);
        e.data = "item " + to_string(from);
        co_await events.async_send(e);  // one a stream, then the end
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/feed";

    net::http::event_source feed(url);
    for (int i : range(3)) {
        println("{}", feed.next()->data);
    }
    feed.close();
    srv.close();
}
```

Output:

```text
item 1
item 2
item 3
```

## See also

- [event_reader](../event_reader/README.md): a stream read once
- [event_stream](../event_stream/README.md): the server's side
- [client](../client/README.md): what it goes through
- The WHATWG HTML Standard §9.2.3; `tests/net/http/events.cpp` (reconnection against this module's server and Go's)
