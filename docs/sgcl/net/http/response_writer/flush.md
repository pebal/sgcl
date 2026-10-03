[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](README.md)

# sgcl::net::http::response_writer::flush, async_flush

```cpp
expected<void, io::error> flush() const;                                // (1)
async::task<expected<void, io::error>> async_flush() const noexcept;    // (2)
```

Sends now what is buffered, Go's `Flusher.Flush`: the head, if it has not gone, then the body written so far. From
the first flush on the body goes chunked (to an HTTP/1.0 client, to the end of the connection), unless the handler
set a Content-Length before it, which is kept then; a body that does not match it ends the connection after the
response. Over HTTP/2 a flush sends HEADERS and DATA without END_STREAM, and the rest follows. Streaming — a large
body, server-sent events — is a write and a flush in turn.

1. Blocks the calling thread until the bytes are sent: for a response written from one of the program's threads,
   never from a worker.
2. The same for a task: a handler runs on a worker and writes `co_await w.async_flush()`.

## Parameters

None.

## Return value

Nothing, or the error:

- the writer's first error, returned by this flush and every flush after it: a field that cannot be sent (a name
  that is not a token, a value with CR, LF, NUL or another control: `std::errc::invalid_argument`, the field named),
  nothing of the head sent; a read of a [written](write.md) file that failed;
- the connection's error when the client went away; the request's [stop](../request/stop.md) is stopped then;
- `io::errc::closed` after a [hijack](hijack.md), and after the response has ended: a writer a task kept past its
  handler, once the server has sent the response, sends nothing more.

## Complexity

Linear in what is buffered.

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
        w.set_header("Content-Type", "text/event-stream");
        for (int i : range(3)) {
            w.write("data: " + to_string(i) + "\n\n");
            if (!co_await w.async_flush()) {
                co_return;  // the client went away
            }
        }
    });
    srv.route("GET /bad", [](net::http::request, net::http::response_writer w) -> async::task<> {
        w.set_header("X-Note", "line\r\nInjected: yes");
        auto sent = co_await w.async_flush();
        w.write(sent ? "sent\n" : "unsent\n");
    });
    srv.on_error = [](const string&) {};
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));
    auto base = "http://127.0.0.1:" + to_string(incoming.local_endpoint().port());

    net::http::client web;
    net::http::response events = web.get(base + "/events");
    string stream = events.text();
    println("{}: {} bytes", events.header("Transfer-Encoding"), stream.size());
    net::http::response bad = web.get(base + "/bad");
    bad.text();
    println("{} {}", bad.status(), bad.headers().contains("Injected"));
    srv.shutdown();
}
```

Output:

```text
chunked: 27 bytes
500 false
```

## See also

- [write](write.md): what is buffered
- [header_sent](header_sent.md): whether the head has gone
- [sgcl::net::http::response_writer](README.md)
