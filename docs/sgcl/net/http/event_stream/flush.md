[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_stream](README.md)

# sgcl::net::http::event_stream::flush, async_flush

```cpp
expected<void, io::error> flush() const;                                // (1)
async::task<expected<void, io::error>> async_flush() const noexcept;    // (2)
```

Sends the head now, before any event, so that a client sees the stream open (a browser's `onopen`) while the first
event is still to come.

1. On the calling thread, which waits: not in a handler.
2. The same for a task.

## Parameters

None.

## Return value

Nothing; or the write's [io::error](../../../io/error/README.md).

## Complexity

Constant.

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
    net::http::server srv;
    srv.route("GET /events", [](net::http::request, net::http::response_writer w) -> async::task<> {
        net::http::event_stream events(w);
        co_await events.async_flush();
        co_await async::sleep(100 * millisecond);  // nothing to send yet
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/events";
    net::http::client web;
    web.proxy = net::http::proxy();
    net::http::response res = web.get(url).value();  // the head, before any event
    println("{} {}", res.status(), res.header("Content-Type"));
    srv.close();
}
```

Output:

```text
200 text/event-stream
```

## See also

- [response_writer::flush](../response_writer/flush.md)
- [sgcl::net::http::event_stream](README.md)
