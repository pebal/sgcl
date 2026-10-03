[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](../request.md)

# sgcl::net::http::request::text, async_text

```cpp
expected<string, io::error> text() const;                                // (1)
async::task<expected<string, io::error>> async_text() const noexcept;    // (2)
```

Reads the whole body of a received request as text, Go's `io.ReadAll(r.Body)`, straight into the string it returns.
The read is bounded by the server's `max_body_bytes`: a chunked body past it is `net::errc::body_too_large` (a request
that declares a longer `Content-Length` is answered 413 by the server before the handler runs). The body is read once:
a second read, or one after [body](body.md) was read, gives what is left. A request without a body gives `""`.

1. Blocks the calling thread: the reading runs on the scheduler and the thread waits for it. For code on a thread of
   its own, never a handler, which runs on a worker.
2. Returns a task that does the same: a handler is a task and writes `co_await req.async_text()`.

## Parameters

None.

## Return value

The body, or the [error](../../../io/error.md) of its reading: `net::errc::body_too_large`,
`io::errc::unexpected_eof` for a body cut short, or the error of the connection.

## Complexity

Linear in the size of the body.

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
    srv.max_body_bytes = 16;
    srv.route("POST /notes", [](net::http::request req, net::http::response_writer w)
                                 -> async::task<> {
        auto text = co_await req.async_text();
        if (!text) {
            w.write(text.error().message() + "\n");
            co_return;
        }
        w.write("saved: " + text->to_upper() + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response saved = web.post(base + "/notes", "text/plain", "buy milk");
    print("{} {}", saved.status(), saved.text().value());
    net::http::request long_note("POST", base + "/notes");
    long_note.set_body(io::buffer(string("x").repeat(100)));  // chunked: no length to refuse
    net::http::response cut = web.send(long_note);
    print("{} {}", cut.status(), cut.text().value());
    net::http::response refused = web.post(base + "/notes", "text/plain", string("x").repeat(100));
    println("{}", refused.status());
    srv.close();
}
```

Output:

```text
200 saved: BUY MILK
200 read body: body too large
413
```

## See also

- [bytes](bytes.md): the body as bytes; [body](body.md): the body as a stream
- [max_body_bytes](../server.md#member-objects): the bound of a read
- [sgcl::net::http::request](../request.md)
