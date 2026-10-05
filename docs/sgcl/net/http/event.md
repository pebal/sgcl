[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::event

```cpp
#include "sgcl/net/http/events.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    struct event {
        string type;
        string data;
        string id;
        optional<duration> retry;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::event` is an event of a stream of Server-Sent Events (the WHATWG HTML Standard, §9.2): what a
server's [event_stream](event_stream/README.md) sends, and what an [event_reader](event_reader/README.md) and an
[event_source](event_source/README.md) give. A plain struct of values, set by name.

## Member objects

| Member | Description |
|---|---|
| `type` | the `event` field; sent only when not empty, given as `message` when the stream gave none |
| `data` | the data: its lines joined by LF when read, each line a `data:` line when sent |
| `id` | the `id` field when sent (one line, no NUL); when read, the stream's last event id at the event |
| `retry` | the reconnection time, `retry:` in milliseconds when sent; when read, the event's own `retry` field; none by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::event_reader events(io::reader(make_tracked<io::buffer>("id: 5\nretry: 250\ndata: a\ndata: b\n\n")));
    net::http::event e = events.next().value().value();
    println("{} {} {} {}", e.type, e.id, e.retry->to_string(), e.data.replace("\n", "|"));
}
```

Output:

```text
message 5 250ms a|b
```

## See also

- [event_stream](event_stream/README.md): what sends one
- [event_reader](event_reader/README.md), [event_source](event_source/README.md): what give one
