# sgcl::net::http::server

```cpp
#include "sgcl/net/http/server.h"   // or "sgcl/net/http/http.h"

namespace sgcl::net::http {
    class server;   // HTTP/1.1 with routes; a handle of one word, copies share the routes and the connections
}
```

Go's `http.Server` and `ServeMux` in one. Routes are registered with a handler each, `serve` listens and runs one task for each connection, and `shutdown` or `close` ends it. A handler is a function of `(request, response_writer)` that returns `void`, for one that never waits (a write only buffers), or `async::task<>`, for one that reads the body or calls another service; the server tells them apart by the type.

## Rules

- **Patterns** are Go 1.22's: `"[METHOD ][HOST]/PATH"`. A segment is a literal, `{name}` (one segment, not empty), `{name...}` (the rest of the path, last) or `{$}` (the end of a path ending in `/`, last); a path ending in `/` matches everything under it. `GET` matches HEAD too. Of the patterns that match a request the most specific wins; a pattern that would conflict with one already registered (both match some request and neither is more specific) is `invalid_argument` at `route` (a broken program, a panic in Go), except that a pattern with a host wins over one without. The tests hold the table to Go's own `ServeMux` (`tools/route_oracle.go`), conflicts included. Routes are registered before `serve`.
- **What the route does not match**: a path with an empty segment inside is redirected (307) to the one without; a subtree named without its slash (`/images` for `/images/`) is redirected there (307), as Go does; a path some pattern matches for other methods is 405 with `Allow`; anything else goes to `not_found` (404 text/plain by default). The path is the one the URL parser normalized (`.` and `..` resolved, [url](../url.md)), and each segment is unescaped before it is compared: `{id}` of `/posts/a%20b` is `a b`.
- **The head** is read under `read_header_timeout` (10 s; Go's is none), counted from its first byte, and a connection kept alive waits `idle_timeout` (120 s) for that byte. Past `max_header_bytes` (32 KB; Go 1 MB) it is 431. The head is held to RFC 9112 and RFC 9110 by `detail/parser.h`, and refused with its status and the connection closed:
  - the request line: one SP between the parts, a method that is a token, a target of ASCII without controls in one of the four forms (asterisk for OPTIONS only, authority for CONNECT only), `HTTP/1.0` or `HTTP/1.1` exactly (another version is 505);
  - the fields: a bare CR anywhere, whitespace between a name and its colon, whitespace before the first field, obs-fold, a name that is not a token, NUL or a control but HTAB in a value, all 400; a bare LF ends a line, as RFC 9112 §2.2 allows;
  - the framing: Transfer-Encoding with Content-Length is 400; a coding other than exactly `chunked` 501, `chunked` twice 400, Transfer-Encoding in HTTP/1.0 400; a Content-Length that is not `1*DIGIT`, past 2^63 − 1, or lengths that disagree (fields or list items) 400;
  - chunked: a size of hex digits only, bounded; extensions by their grammar, the line at most 4 KB; strictly CRLF after the size line and after the data; a framing field in the trailers (Content-Length, Transfer-Encoding, Host, Trailer) 400;
  - Host: none in HTTP/1.1, or two, or one that is not an authority, 400;
  - `Expect: 100-continue` sends `100 Continue` when the handler first reads the body (not at all when it does not); any other expectation is 417.
- **The body** is read by the handler, each read bounded by `max_body_bytes` (32 MB; 0 for none): a Content-Length past it is 413 before the handler runs, and a body that grows past it while read fails the read with `body_too_large`; a handler that wrote nothing then gets a 413 from the server. What the handler leaves unread is read after the response, up to 256 KB (Go's bound); past that, or a length that says more, the response says `Connection: close` and the connection ends, the writing half first and the rest read and dropped for half a second, so that the client reads the response before the close.
- **The response** is the [response_writer](response_writer.md)'s: sent whole after the handler with its Content-Length, or flushed and chunked; `Connection: close` when the request asked for it, for HTTP/1.0 without keep-alive, during a shutdown, or after an error. Pipelined requests are served in turn.
- **A handler that throws** gets a 500 when nothing was sent yet, `on_error` hears of it (a line on stderr by default), and the connection ends; the server goes on.
- **Shutdown**: `shutdown` closes the listeners and the idle connections, lets each active one finish its response (which says `Connection: close`), and returns when every connection has ended; `close` ends everything at once, the reads and writes in progress with `io::errc::closed`, and stops every request's `stop()` token. `serve` then returns `server_closed`, as does a `serve` after either.
- **Two forms.** `serve` and `shutdown` block the thread (main's: `server.serve(":8080")` is Go's `ListenAndServe`); a task writes `co_await server.async_serve(...)`, `co_await server.async_shutdown()`.

## Members

```cpp
server();

template<class Handler> server& route(const string& pattern, Handler handler);   // void or async::task<> of (request, response_writer)
template<class Handler> server& not_found(Handler handler);

expected<void, io::error> serve(const string& address) const;       // ":8080": until shutdown or close, then server_closed
async::task<expected<void, io::error>> async_serve(const string& address) const;
expected<void, io::error> serve(const net::listener& l) const;
async::task<expected<void, io::error>> async_serve(const net::listener& l) const;
void shutdown() const;
async::task<> async_shutdown() const;
void close() const;

duration read_header_timeout = 10s;
duration read_timeout = duration::zero();       // the head and the body; zero: none
duration write_timeout = duration::zero();      // the response, from the end of the head
duration idle_timeout = 120s;
size_t max_header_bytes = 32 * 1024;
uint64_t max_body_bytes = 32 << 20;
function<void(const string&)> on_error;
```

## Example

```cpp
#include "sgcl/core/range.h"
#include "sgcl/net/http/http.h"
#include <iostream>

using namespace sgcl;

int main() {
    net::http::server server;
    server.route("GET /users/{id}", [](net::http::request req, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write("user " + req.path_value("id") + "\n");
    });
    server.route("POST /echo", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        auto body = co_await req.async_text();
        if (!body) {
            w.error(net::http::status::bad_request);
            co_return;
        }
        w.write(*body);
    });
    server.route("GET /events", [](net::http::request, net::http::response_writer w) -> async::task<> {
        w.set_header("Content-Type", "text/event-stream");
        for (int i : range(3)) {
            w.write("data: " + to_string(i) + "\n\n");
            if (!co_await w.async_flush()) {
                co_return;   // the client went away
            }
        }
    });
    auto listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(server.async_serve(*listener));

    net::http::client client;
    auto base = "http://127.0.0.1:" + to_string(listener->local_endpoint().port());
    std::cout << *client.get(base + "/users/42")->text();
    std::cout << *client.post(base + "/echo", "text/plain", "echoed\n")->text();
    auto events = client.get(base + "/events");
    std::cout << events->header("Transfer-Encoding") << ": " << events->text()->size() << " bytes\n";
    auto wrong = client.post(base + "/users/42", "text/plain", "");
    std::cout << wrong->status() << ' ' << wrong->header("Allow") << '\n';
    (void)wrong->text();

    server.shutdown();
    std::cout << (serving.wait().error().code() == net::errc::server_closed) << '\n';
}
```

Output:

```text
user 42
echoed
chunked: 27 bytes
405 GET, HEAD
1
```

## See also

- [request](request.md), [response_writer](response_writer.md): what a handler gets; [client](client.md): the other side
- [async timeout](../../async/timeout.md): a limit on `async_shutdown`
- `tools/route_oracle.go` (the routes against Go's `ServeMux`: `go run tools/route_oracle.go > tests/net/http/route_oracle.h`), `tests/net/http/parser.cpp` (every rule of the head, by section, and the smuggling payloads), `tests/net/http/server.cpp` (limits, timeouts on the manual clock, shutdown, close, hijack), `tests/net/http/go.cpp` (Go's client against this server)
