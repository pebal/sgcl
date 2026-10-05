[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::event_reader

```cpp
#include "sgcl/net/http/events.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class event_reader;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::event_reader` reads the events of a stream of Server-Sent Events as they come, over a response's
body or any reader: [next](next.md) gives the next [event](../event.md), parsed as the WHATWG HTML Standard interprets
the event stream (§9.2.6). Go's standard library has no reader of it; a browser's `EventSource` is a reader with
reconnection, which is [event_source](../event_source/README.md) here.

A handle of one word: copies are the same reader. It holds 16 KB of the stream at a time and the event being read,
bounded by `max_event_bytes`.

## Rules

- **Lines** end in CRLF, LF or CR alone, a CR at the end of what came waiting to see whether an LF follows; a byte
  order mark at the start of the stream is dropped.
- **Fields**: `event` (the type of the next event), `data` (a line of it), `id` (the last event id, kept across
  events; one with NUL ignored), `retry` (the reconnection time in milliseconds, ASCII digits alone); any other field is
  ignored, a line that starts with `:` is a comment, a line without a colon is a field with an empty value, and one
  space after the colon is dropped.
- **Dispatch**: an empty line ends an event; one with no data is not given, its type dropped. The type is `message`
  when none was given; the id the last event id there was. The bytes are given as they came: the standard's decoding
  of invalid UTF-8 into U+FFFD is the program's to do.
- **The end**: `nullopt`; an event the end cut is dropped. An event or a line past `max_event_bytes` (1 MB by default)
  is `net::errc::body_too_large`; an error is kept.

## Member objects

| Name | Description |
|---|---|
| `default_max_event_bytes` | `static constexpr size_t default_max_event_bytes = size_t(1) << 20;` the default limit of an event and of a line |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](event_reader.md) | a reader of a body, of a response, or none |
| [next, async_next](next.md) | the next event |
| [last_event_id](last_event_id.md) | the stream's last event id |
| [retry](retry.md) | the reconnection time the stream set |
| [operator bool](operator_bool.md) | whether there is a stream |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    string stream = "\xEF\xBB\xBF" "data: YHOO\ndata: +2\ndata: 10\n\nevent: add\rdata: 73857293\r\r";
    net::http::event_reader events(io::reader(make_tracked<io::buffer>(stream)));
    while (auto e = events.next()) {
        if (!*e) {
            break;
        }
        println("{}: {}", (*e)->type, (*e)->data);
    }
}
```

Output:

```text
message: YHOO
+2
10
add: 73857293
```

## See also

- [event](../event.md): what it gives
- [event_source](../event_source/README.md): a reader that reconnects
- [event_stream](../event_stream/README.md): the server's side
- The WHATWG HTML Standard §9.2.6; `tests/net/http/events.cpp`, `tests/net/http/fuzz/http_events_fuzz.cpp`
