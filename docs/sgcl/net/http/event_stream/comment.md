[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_stream](README.md)

# sgcl::net::http::event_stream::comment, async_comment

```cpp
expected<void, io::error> comment(const string& text) const;                                // (1)
async::task<expected<void, io::error>> async_comment(const string& text) const noexcept;    // (2)
```

Writes a comment, a line `: text` for each line of the text, and flushes it. A reader drops it: it is a heartbeat
that keeps the connection, and the proxies on its way, from closing it as idle while no event comes.

1. On the calling thread, which waits: not in a handler.
2. The same for a task.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the comment's text; empty for a bare `:` |

## Return value

Nothing; or the write's [io::error](../../../io/error/README.md).

## Complexity

Linear in the text.

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
        co_await events.async_comment("still here");
        co_await events.async_send("done");
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
: still here

data: done
```

## See also

- [send, async_send](send.md)
- [sgcl::net::http::event_stream](README.md)
