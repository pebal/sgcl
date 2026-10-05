[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_stream](README.md)

# sgcl::net::http::event_stream::send, async_send

```cpp
expected<void, io::error> send(const event& e) const;                                    // (1)
expected<void, io::error> send(const string& data) const;                                // (2)
async::task<expected<void, io::error>> async_send(const event& e) const noexcept;        // (3)
async::task<expected<void, io::error>> async_send(const string& data) const noexcept;    // (4)
```

Writes an event and flushes it, so that it goes out at once (a chunk over HTTP/1.1, DATA over HTTP/2): its `event:`
line when it has a type, `id:` when it has an id, `retry:` in milliseconds when it has one, a `data:` line for each
line of its data (CRLF, LF and CR all ending one, as a reader splits them), and the empty line that dispatches it.

- (1, 3) The event `e`. A type or an id that holds a line break, or an id that holds NUL, would break the stream:
  `EINVAL`, and nothing is written.
- (2, 4) An event of data alone, which a reader gives as the type `message`.
- (1–2) On the calling thread, which waits: not in a handler, which runs on a worker.
- (3–4) The same for a task: a handler writes `co_await events.async_send(e)`.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the event: type, data, id, retry |
| `data` | the data of an event of no type |

## Return value

Nothing; or the [io::error](../../../io/error/README.md): `EINVAL` for a type or an id that cannot be written, the
write's error (the client gone: the handler stops there).

## Complexity

Linear in the event.

## Exceptions

- (1–2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3–4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /events", [](net::http::request, net::http::response_writer w) -> async::task<> {
        net::http::event_stream events(w);
        net::http::event e;
        e.type = "update";
        e.id = "7";
        e.retry = 5 * second;
        e.data = "two\nlines";
        co_await events.async_send(e);
        co_await events.async_send("plain");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/events";
    net::http::client web;
    web.proxy = net::http::proxy();
    println("{}", web.get(url)->text().value().trim());
    srv.close();
}
```

Output:

```text
event: update
id: 7
retry: 5000
data: two
data: lines

data: plain
```

## See also

- [comment, async_comment](comment.md)
- [event](../event.md)
- [sgcl::net::http::event_stream](README.md)
