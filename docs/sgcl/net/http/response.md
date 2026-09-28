# sgcl::net::http::response

```cpp
#include "sgcl/net/http/response.h"   // or "sgcl/net/http/http.h"

namespace sgcl::net::http {
    class response;   // what a client receives; a handle of one word
}
```

A response as [`client`](client.md) returns it: the status and the head read, the body still on the connection. A 4xx or a 5xx is a response and not an error, as in Go: `ok()` tells a 2xx.

## Rules

- **The body** is read once, with `text()`, `bytes()` or the stream `body()`, and its end gives the connection back to the client's pool, with no close. In a task, `co_await res.async_text()`; `text()` blocks the thread.
- **`close()`** gives the body up without waiting: when the rest of it is already in the connection's buffer it is dropped and the connection goes back to the pool, otherwise the connection is closed. A response neither read nor closed keeps its connection out of the pool until the collector finds it. The stream `body()` has no close of its own (`has_close()` is `false`, and its `close()` does nothing): the body is given up by the response's `close()`, not by Go's `resp.Body.Close()`.
- A body cut short is `io::errc::unexpected_eof`, a chunked framing broken `malformed_response`, a total `timeout` of the client passing while it is read `ETIMEDOUT`.
- `url()` is the URL the response came from, the last of the redirects; `trailers()` holds the trailer fields of a chunked body once it has been read.
- **`proto()`** is the protocol the response came over, as Go's `resp.Proto`: `"HTTP/2.0"`, `"HTTP/1.1"` or `"HTTP/1.0"`. Over HTTP/2 the body is the stream's DATA, its window given back to the server as it is read. `close()` of a body not read to its end resets the stream (`CANCEL`) and leaves the connection to the other requests. The trailers are the fields the server sent after the body, and a stream the server resets while its body is read is `std::errc::connection_reset`.

## Members

```cpp
int status() const noexcept;
string proto() const;                    // "HTTP/1.1", "HTTP/1.0" or "HTTP/2.0": the protocol it came over (Go's resp.Proto)
bool ok() const noexcept;                          // 200 to 299
string header(const string& name) const;           // "" when there is none
const http::headers& headers() const noexcept;
optional<uint64_t> content_length() const noexcept;
net::url url() const;
expected<string, io::error> text() const;
async::task<expected<string, io::error>> async_text() const;
expected<vector<byte>, io::error> bytes() const;
async::task<expected<vector<byte>, io::error>> async_bytes() const;
io::reader body() const;
http::headers trailers() const;
void close() const;
```

## Example

```cpp
#include "sgcl/net/http/http.h"
#include "sgcl/io/print.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /report", [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write("three lines\nof a report\nend\n");
    });
    srv.route("GET /old", [](net::http::request, net::http::response_writer w) { w.redirect("/report"); });
    srv.route("GET /big", [](net::http::request, net::http::response_writer w) { w.write(string(100000, 'x')); });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    auto base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response res = web.get(base + "/old");
    string body = res.text();
    println("{} {} {} {}", res.status(), res.ok(), res.url().path(), res.proto());
    println("{} {}", res.header("content-type"), res.content_length().value_or(0));
    print(body);

    net::http::response missing = web.get(base + "/nothing");     // a 404 is a response, not an error
    println("{} {}", missing.status(), missing.ok());
    missing.close();

    net::http::response big = web.get(base + "/big");
    println("{}", big.content_length().value_or(0));
    big.close();                                                  // unread: given up without waiting

    srv.close();
    serving.wait();
}
```

Output:

```text
200 true /report HTTP/1.1
text/plain 28
three lines
of a report
end
404 false
100000
```

## See also

- [client](client.md): where it comes from; [headers](headers.md), [cookie](cookie.md) (`cookie::parse` of a `Set-Cookie`)
