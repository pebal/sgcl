[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_reader](README.md)

# sgcl::net::http::event_reader::next, async_next

```cpp
expected<optional<event>, io::error> next() const;                                // (1)
async::task<expected<optional<event>, io::error>> async_next() const noexcept;    // (2)
```

The next [event](../event.md) of the stream, as the WHATWG HTML Standard interprets one (§9.2.6): its lines read up to
the empty line that dispatches it (each line ending in CRLF, LF or CR alone), a byte order mark at the start dropped,
comments (`:`) dropped, `event`, `data`, `id` and `retry` taken and other fields ignored, one space after a colon
dropped, an event of no data not dispatched. The type is `message` when the stream gave none; the id is the last
event id there was (an `id` field is kept across events, and one holding NUL ignored); a `retry` of ASCII digits is
the event's retry and the reader's [retry](retry.md). At the stream's end `nullopt`, an event it cut dropped.

1. Reads on the calling thread, which waits: for a thread of the program, never a handler.
2. The same for a task.

## Parameters

None.

## Return value

The event; `nullopt` at the end of the stream; or the [io::error](../../../io/error/README.md): `net::errc::body_too_large`
for an event or a line past the limit, the stream's error. An error is kept: every next gives it.

## Complexity

Linear in the bytes read.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    string stream = "event: add\r\nid: 1\r\ndata: 73857293\r\n\r\n"
                    ": a comment\n"
                    "data: two\ndata: lines\n\n"
                    "data: cut by the end";
    net::http::event_reader events(io::reader(make_tracked<io::buffer>(stream)));
    while (auto e = events.next()) {
        if (!*e) {
            println("end");
            break;
        }
        println("{} {} [{}]", (*e)->type, (*e)->id, (*e)->data);
    }
}
```

Output:

```text
add 1 [73857293]
message 1 [two
lines]
end
```

## See also

- [event](../event.md)
- [last_event_id](last_event_id.md), [retry](retry.md)
- [sgcl::net::http::event_reader](README.md)
