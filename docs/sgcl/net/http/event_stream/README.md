[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::event_stream

```cpp
#include "sgcl/net/http/events.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class event_stream;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::event_stream` is a server's stream of Server-Sent Events (the event stream of the WHATWG HTML
Standard, §9.2) over the response of a handler: the fields of `text/event-stream` set on the writer, each
[event](../event.md) written and flushed as it is sent, so that it goes out at once, a chunk over HTTP/1.1 and DATA
over HTTP/2. Go writes one by hand, the fields set and `http.Flusher` called after each event; here the stream writes
the text of the standard's format and flushes it.

A handle of one word over the [response_writer](../response_writer/README.md): copies are the same stream. The handler
that sends is a task, and the response stays open for as long as it runs; a send that fails (the client gone) is the
handler's signal to stop.

## Rules

- **The head**: `Content-Type: text/event-stream`, `Cache-Control: no-cache` and `X-Accel-Buffering: no` (a proxy of
  nginx's kind then holds nothing back), set by the constructor and sent with the first event or by
  [flush](flush.md).
- **An event's text**: `event:` when it has a type, `id:` when it has an id, `retry:` in milliseconds when it has one,
  a `data:` line for each line of its data (CRLF, LF and CR all ending one), the empty line. A type or an id with a
  line break, or an id with NUL, is refused (`EINVAL`), nothing written.
- **Comments** ([comment](comment.md)) are dropped by readers: a heartbeat for the connection and the proxies on its
  way, which close a connection idle too long.
- The blocking forms are for a thread of the program, never a handler: a handler awaits the `async_` forms.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](event_stream.md) | a stream over a handler's writer |
| [send, async_send](send.md) | writes and flushes an event |
| [comment, async_comment](comment.md) | writes and flushes a comment |
| [flush, async_flush](flush.md) | sends the head before any event |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /clock", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        net::http::event_stream events(w);
        for (int tick = 1; tick <= 3; ++tick) {
            net::http::event e;
            e.type = "tick";
            e.id = to_string(tick);
            e.data = "tick " + to_string(tick);
            if (!co_await events.async_send(e)) {
                co_return;  // the client went
            }
            co_await async::sleep(10 * millisecond);
        }
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/clock";

    net::http::client web;
    net::http::event_reader events(web.get(url).value());
    while (auto e = events.next()) {
        if (!*e) {
            break;
        }
        println("{} {}", (*e)->id, (*e)->data);
    }
    srv.close();
}
```

Output:

```text
1 tick 1
2 tick 2
3 tick 3
```

## See also

- [event](../event.md): what is sent
- [event_reader](../event_reader/README.md), [event_source](../event_source/README.md): the client's side
- [response_writer](../response_writer/README.md): what it writes through
- The WHATWG HTML Standard §9.2; `tests/net/http/events.cpp` (curl `-N` and Go's net/http reading it)
