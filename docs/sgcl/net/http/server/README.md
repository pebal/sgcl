[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::server

```cpp
#include "sgcl/net/http/server.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class server;
}
```

`net::http::server` is Go's `http.Server` and `ServeMux` in one: routes are registered with a handler each
([route](route.md), the patterns of Go 1.22), [serve](serve.md) listens and runs one task for each
connection, and [shutdown](shutdown.md) or [close](close.md) ends it. A handler is a function of
`(request, response_writer)` that returns `void`, for one that never waits (a [write](../response_writer/write.md) only
buffers), or `async::task<>`, for one that reads the body or calls another service; the server tells the two apart
by the type. The same handlers serve HTTP/1.1 and HTTP/2, on a plain port and over TLS 1.3
([serve_tls](serve_tls.md), or a listener of [tls::listen](../../tls/listen.md)).

A server is a word and its settings: a `tracked_ptr` to the routes and the connections, which the copies share, as
the copies of a [connection](../../connection/README.md) share it, and the [settings](#member-objects) — the limits, the
timeouts, the HTTP/2 switches, `on_error`, the access log — as fields of the handle itself, so that each copy has its
own. They are read when `serve` is called, for the connections that `serve` accepts. Against Go, the head has a
timeout and a smaller limit by default (10 s and 32 KB, where Go has none and 1 MB), a body has a limit (32 MB, Go's
`http.MaxBytesReader` made the default), and no Content-Type is guessed.

## Rules

- A server holds a `tracked_ptr`, so it lives where one may: on a stack, in a task, in a managed object; in a global
  or a `std` container, a [rooted](../../../core/rooted/README.md) of it ([The rules](../../../core/README.md#the-rules), 1).
- The routes, [not_found](not_found.md) and [access_log](access_log.md) are set before `serve`, the
  settings too: what changes after `serve` was called does not reach its connections.
- `serve`, `serve_tls` and `shutdown` block the calling thread (main's: `server.serve(":8080")` is Go's
  `ListenAndServe`), the connections served on the scheduler meanwhile; a task writes `co_await
  server.async_serve(...)` and `co_await server.async_shutdown()`. A handler runs on a worker: one that waits uses
  the `async_` forms.
- A handler that throws gets a 500 when nothing was sent yet, `on_error` hears of it (a line on stderr by default),
  and the connection ends; the server goes on.
- Fields a handler cannot send (a name that is not a token, a value with CR, LF, NUL or another control: a user's
  text that would split the response) are answered the same way: nothing of that head is sent, a 500 of the
  server's own goes instead, `on_error` names the field, and a flush before it returns `std::errc::invalid_argument`
  ([response_writer](../response_writer/README.md)).
- Errors are values, `expected<void, io::error>` ([io::error](../../../io/error/README.md)): `serve` returns
  [errc](../../errc.md)`::server_closed` after `shutdown` or `close`, as Go's `ErrServerClosed`.

### The head

The head is read under `read_header_timeout` (10 s), counted from its first byte, and a connection kept alive waits
`idle_timeout` (120 s) for that byte. Past `max_header_bytes` (32 KB) it is 431. The head is held to RFC 9112 and
RFC 9110 and refused with its status, the connection closed:

- the request line: one SP between the parts, a method that is a token, a target of ASCII without controls in one
  of the four forms (asterisk for OPTIONS only, authority for CONNECT only), `HTTP/1.0` or `HTTP/1.1` exactly
  (another version is 505);
- the fields: a bare CR anywhere, whitespace between a name and its colon, whitespace before the first field,
  obs-fold, a name that is not a token, NUL or a control but HTAB in a value, all 400; a bare LF ends a line, as
  RFC 9112 §2.2 allows;
- the framing: Transfer-Encoding with Content-Length is 400; a coding other than exactly `chunked` 501, `chunked`
  twice 400, Transfer-Encoding in HTTP/1.0 400; a Content-Length that is not `1*DIGIT`, past 2^63 − 1, or lengths
  that disagree (fields or list items) 400;
- chunked: a size of hex digits only, bounded; extensions by their grammar, the line at most 4 KB; strictly CRLF
  after the size line and after the data; a framing field in the trailers (Content-Length, Transfer-Encoding, Host,
  Trailer) 400;
- Host: none in HTTP/1.1, or two, or one that is not an authority, 400;
- `Expect: 100-continue` sends `100 Continue` when the handler first reads the body (not at all when it does not);
  any other expectation is 417.

### The body and the response

- The body is read by the handler ([request](../request/README.md)), each read bounded by `max_body_bytes` (32 MB; 0 for
  none): a Content-Length past it is 413 before the handler runs, and a body that grows past it while read fails the
  read with `body_too_large`; a handler that wrote nothing then gets a 413 from the server.
- What the handler leaves unread is read after the response, up to 256 KB (Go's bound); past that, or a length that
  says more, the response says `Connection: close` and the connection ends, the writing half first and the rest read
  and dropped for half a second, so that the client reads the response before the close.
- The response is the [response_writer](../response_writer/README.md)'s: sent whole after the handler with its
  Content-Length, or flushed and chunked; `Connection: close` when the request asked for it, for HTTP/1.0 without
  keep-alive, during a shutdown, or after an error. Pipelined requests are served in turn.

### HTTP/2

- **When.** Over TLS a connection is HTTP/2 when its handshake agreed on `"h2"` by ALPN (`http2`, on by default;
  [serve_tls](serve_tls.md) puts `"h2"` in the list, a listener of one's own names it itself). On a plain
  port it is HTTP/2 when `h2c` is on and the connection begins with the client's preface (HTTP/2 by prior
  knowledge, RFC 9113 §3.3; the Upgrade of HTTP/1.1 is not offered, RFC 9113 dropped it); anything else on that port
  is HTTP/1.1, as before. Because h2c shares the port with HTTP/1.1, bytes that are not the preface are an HTTP/1.1
  request (answered 400), not a GOAWAY: the one case of h2spec's where the plain port differs (over TLS it passes).
- **The same handler.** A request comes to the same route and handler as over HTTP/1.1, with
  [request::proto](../request/proto.md) `"HTTP/2.0"` (`"HTTP/1.1"` or `"HTTP/1.0"` otherwise; the client's
  [response::proto](../response/proto.md) likewise). The response is the same [response_writer](../response_writer/README.md):
  written whole, it goes as HEADERS (with its Content-Length) and DATA with END_STREAM; `async_flush` sends HEADERS
  and DATA without END_STREAM, and the rest follows. The fields of a connection (Connection, Keep-Alive,
  Proxy-Connection, Transfer-Encoding, Upgrade) are not sent, names go in lower case.
  [hijack](../response_writer/hijack.md) gives `std::errc::operation_not_supported`: a stream is not a connection (Go
  has no Hijacker in HTTP/2). The request's trailers are [request::trailers](../request/trailers.md) after its body;
  CONNECT is 405 in this version, and nothing is pushed.
- **Streams.** A connection carries up to `max_concurrent_streams` requests at once, each its handler in a task of
  its own; one past them is refused (REFUSED_STREAM, the client retries it). A request whose declared body is at most
  16 KB, and which does not ask `Expect: 100-continue`, has its handler started when the first bytes of its body come
  rather than with its HEADERS (Go starts it with the HEADERS); the handler sees the same request. Flow control is per
  stream and per connection: the server announces windows of 1 MB and gives them back as the handler reads the body,
  and sends within the client's.
- **Limits.** `max_header_bytes` is the SETTINGS_MAX_HEADER_LIST_SIZE announced: a request's fields past it are
  answered 431 on its stream and the connection lives; a field block whose bytes pass max(2 × that, 64 KB) ends the
  connection before it is decoded (CVE-2023-45288). A malformed request (RFC 9113 §8.1.1, §8.2, §8.3.1: an
  upper-case name, a pseudo-field unknown, repeated or after a regular one, a field of a connection, TE other than
  trailers, :method, :scheme or :path missing, a Content-Length its DATA does not match, a pseudo-field in the
  trailers) is its stream's PROTOCOL_ERROR. Against rapid reset (CVE-2023-44487) at most `max_concurrent_streams`
  handlers run, more than four times that waiting ends the connection (Go's rule), and RST_STREAM from the client are
  held to 1000 at once and 33 a second after (nghttp2's). Floods of control frames (10 000 unanswered), of empty
  frames (1000 in a row) end the connection with ENHANCE_YOUR_CALM.
- **Timeouts.** `idle_timeout` is for a connection without streams. `read_timeout` and `write_timeout` hold each
  stream from its request's head: a request whose body has not come whole by `read_timeout` is reset (RST_STREAM
  CANCEL: its handler's reads fail), a response not whole by `write_timeout` is reset (INTERNAL_ERROR); the
  connection lives. Go, at its ReadTimeout, only closes the handler's body and sends nothing. A stream that waits for
  window longer than 30 s is reset too.
- **Conformance.** h2spec 2.6.0 against this server: 146 of 146 over TLS, 145 of 146 on the plain port (the preface
  case above).

## Member objects

The settings, read when [serve](serve.md) is called; a zero timeout is none.

| Member | Description |
|---|---|
| `duration read_header_timeout` | the whole head, from its first byte (Slowloris); `10s` by default, where Go has none |
| `duration read_timeout` | the head and the body; over HTTP/2 the body of each stream from its head. Zero, the default: none |
| `duration write_timeout` | the response, from the end of the head; over HTTP/2 each stream's. Zero, the default: none |
| `duration idle_timeout` | how long a kept connection waits for the next request's first byte, an HTTP/2 connection without streams; `120s` by default |
| `size_t max_header_bytes` | the limit of a head, 431 past it (a head of exactly this size is read); over HTTP/2 the SETTINGS_MAX_HEADER_LIST_SIZE announced. 32 KB by default (Go: 1 MB), zero taken as 1 |
| `uint64_t max_body_bytes` | the limit of a body, 413 past it; zero: none. 32 MB by default |
| `function<void(const string&)> on_error` | told of a handler's exception, a field it could not send, a file of the body that could not be read, an accept's failure; a line on stderr when empty, the default |
| `bool http2` | HTTP/2 (RFC 9113, HPACK RFC 7541) for a connection whose TLS agreed on `"h2"` by ALPN; `true` by default |
| `bool h2c` | HTTP/2 by prior knowledge on a plain port, beside HTTP/1.1 (tests, a network of one's own); `false` by default |
| `uint32_t max_concurrent_streams` | HTTP/2: the streams a client may have open, the handlers a connection runs; 250 by default, as Go's, zero taken as 1 |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](server.md) | constructs a server without routes |
| `operator=` | copies another server: the routes and the connections shared, the settings copied; a move is the copy, so a moved-from server is the same server |

#### Routes

| Function | Description |
|---|---|
| [route](route.md) | registers a handler for a pattern |
| [not_found](not_found.md) | registers the handler of what no pattern matches |
| [access_log](access_log.md) | a record of every exchange through a logger |

#### Serving

| Function | Description |
|---|---|
| [serve, async_serve](serve.md) | serves the connections of an address or a listener until the server stops |
| [serve_tls, async_serve_tls](serve_tls.md) | serves over TLS on an address |

#### Stopping

| Function | Description |
|---|---|
| [shutdown, async_shutdown](shutdown.md) | stops gracefully: the responses in progress finish |
| [close](close.md) | stops at once |

## Examples

Routes with a path value, a handler that reads the body, a stream, and the 405 of a path no route of the method
matches; a client of the same program asks:

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /users/{id}", [](net::http::request req, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write("user " + req.path_value("id") + "\n");
    });
    srv.route("POST /echo", [](net::http::request req,
                               net::http::response_writer w) -> async::task<> {
        auto body = co_await req.async_text();
        if (!body) {
            w.error(net::http::status::bad_request);
            co_return;
        }
        // `*`: write takes a string and bytes alike, and the expected converts to both
        w.write(*body);
    });
    srv.route("GET /events", [](net::http::request, net::http::response_writer w) -> async::task<> {
        w.set_header("Content-Type", "text/event-stream");
        for (int i : range(3)) {
            w.write("data: " + to_string(i) + "\n\n");
            if (!co_await w.async_flush()) {
                co_return;  // the client went away
            }
        }
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));

    net::http::client web;
    auto base = "http://127.0.0.1:" + to_string(incoming.local_endpoint().port());
    net::http::response user = web.get(base + "/users/42");
    string text = user.text();
    print(text);
    net::http::response echo = web.post(base + "/echo", "text/plain", "echoed\n");
    string echoed = echo.text();
    print(echoed);
    net::http::response events = web.get(base + "/events");
    string stream = events.text();
    println("{}: {} bytes", events.header("Transfer-Encoding"), stream.size());
    net::http::response wrong = web.post(base + "/users/42", "text/plain", "");
    println("{} {}", wrong.status(), wrong.header("Allow"));
    wrong.text();

    srv.shutdown();
    println(serving.wait().error().code() == net::errc::server_closed);
}
```

Output:

```text
user 42
echoed
chunked: 27 bytes
405 GET, HEAD
true
```

HTTP/2 by prior knowledge beside HTTP/1.1 on one plain port, and the hijack an HTTP/2 stream refuses:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.h2c = true;
    srv.route("GET /hello", [](net::http::request req, net::http::response_writer w) {
        w.write("hello over " + req.proto() + "\n");
    });
    srv.route("GET /upgrade", [](net::http::request, net::http::response_writer w) {
        auto taken = w.hijack();
        w.write(taken ? "hijacked\n" : "no hijack: " + taken.error().message() + "\n");
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));
    auto base = "http://127.0.0.1:" + to_string(incoming.local_endpoint().port());

    net::http::client h2;
    h2.h2c = true;  // the client's HTTP/2 by prior knowledge
    net::http::response hello = h2.get(base + "/hello");
    string text = hello.text();
    print(hello.proto() + ": " + text);
    net::http::response upgrade = h2.get(base + "/upgrade");
    string refused = upgrade.text();
    print(refused);

    net::http::client h1;
    net::http::response old = h1.get(base + "/hello");
    string text1 = old.text();
    print(old.proto() + ": " + text1);

    srv.shutdown();
    println(serving.wait().error().code() == net::errc::server_closed);
}
```

Output:

```text
HTTP/2.0: hello over HTTP/2.0
no hijack: hijack HTTP/2 response: Operation not supported on socket
HTTP/1.1: hello over HTTP/1.1
true
```

## See also

- [request](../request/README.md), [response_writer](../response_writer/README.md): what a handler gets
- [client](../client/README.md): the other side
- [serve](../serve.md), [serve_tls](../serve_tls.md): a directory, or one handler over https, in one call
- [tls](../../tls/README.md): the listener of https, the identity
- [Benchmarks](../benchmarks.md): the server under load against Go's
- `tests/net/http/parser.cpp` (every rule of the head, by section, and the smuggling payloads),
  `tests/net/http/server.cpp` (limits, timeouts on the manual clock, shutdown, close, hijack), `tests/net/http/go.cpp`
  (Go's client against this server), `tests/net/http/https.cpp` (this server over TLS against this module's client
  and curl)
- `tests/net/http/h2_server.cpp` (HTTP/2 against Go's client, `tools/h2_oracle.go`, and curl: h2c and TLS, 100
  streams on one connection, bodies both ways, flushes, trailers, the stream timeouts, shutdown),
  `tests/net/http/h2_spec.cpp` (h2spec), `tests/net/http/h2_connection.cpp` (the connection machine by RFC section,
  the attacks)
