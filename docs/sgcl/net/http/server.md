# sgcl::net::http::server

```cpp
#include "sgcl/net/http/server.h"   // or "sgcl/net/http/http.h"

namespace sgcl::net::http {
    class server;   // HTTP/1.1 and HTTP/2 with routes; a handle of one word, copies share the routes and the connections
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
- **Fields a handler cannot send** (a name that is not a token, a value with CR, LF, NUL or another control: a user's text that would split the response) are answered the same way: nothing of that head is sent, a 500 of the server's own goes instead, `on_error` names the field, and a flush before it returns `std::errc::invalid_argument`.
- **Shutdown**: `shutdown` closes the listeners and the idle connections, lets each active one finish its response (which says `Connection: close`), and returns when every connection has ended; `close` ends everything at once, the reads and writes in progress with `io::errc::closed`, and stops every request's `stop()` token. `serve` then returns `server_closed`, as does a `serve` after either.
- **https** is the same server over TLS: `srv.serve_tls(":8443", tls)` (Go's `ListenAndServeTLS`), or `srv.serve(net::tls::listen(":8443", tls))` for a listener of the program's own, whose accept gives connections after their handshake (each handshake in a task of its own within `tls.handshake_timeout`, one that fails dropped), with the certificate and key of a [`net::tls::identity`](../tls.md) in `tls.identities`. Nothing of the server changes: routes, limits, timeouts and shutdown are the same. `serve_tls` completes the config's ALPN list as Go does: `"h2"` added at the end when `http2` is on and the list has none (taken out when it is off), `"http/1.1"` added when missing, the order given kept; the server's order is the preference, so `{"http/1.1"}` given stays HTTP/1.1 for a client that offers both. A listener of one's own keeps the ALPN it was made with: `tls.alpn = {"h2", "http/1.1"}` there for HTTP/2.
- **HTTP/2** (RFC 9113, HPACK RFC 7541) serves the same handlers on the same routes; see [HTTP/2](#http2) below.
- **The access log.** `access_log()` writes a record of every exchange through the [default logger](../../slog/logger.md), buffered (a batch per worker); `access_log(log)` through a logger of one's own, as it is given (`slog::options{.out = file, .json = true, .buffered = true}` for JSON in batches: [slog](../../slog/logger.md)). One record when the response is finished: `method`, `path`, `proto`, `status`, `bytes` (of the body: 0 for HEAD, 204, 304), `duration` (from the request's first byte; over HTTP/2 from the handler's start), `remote`, `user_agent` (`""` for none), and `request_id` when the request has an `X-Request-ID`; at `info`, at `error` for a 5xx. Nothing managed per request: the attributes are views of the request, the remote address is written into the line. A request refused before its handler (400, 413, 417, 431 over HTTP/1.1) and a hijacked connection are not logged.

  ```text
  time=2026-09-28T14:05:01.123+02:00 level=INFO msg=request method=GET path=/hello proto=HTTP/1.1 status=200 bytes=5 duration=84.5µs remote=127.0.0.1:52811 user_agent=curl/8.7.1
  ```
- **Two forms.** `serve` and `shutdown` block the thread (main's: `server.serve(":8080")` is Go's `ListenAndServe`); a task writes `co_await server.async_serve(...)`, `co_await server.async_shutdown()`.

## HTTP/2

- **When.** Over TLS a connection is HTTP/2 when its handshake agreed on `"h2"` by ALPN (`http2`, on by default; `serve_tls` puts `"h2"` in the list, a listener of one's own names it itself). On a plain port it is HTTP/2 when `h2c` is on and the connection begins with the client's preface (HTTP/2 by prior knowledge, RFC 9113 §3.3; the Upgrade of HTTP/1.1 is not offered, RFC 9113 dropped it); anything else on that port is HTTP/1.1, as before. Because h2c shares the port with HTTP/1.1, bytes that are not the preface are an HTTP/1.1 request (answered 400), not a GOAWAY: the one case of h2spec's where the plain port differs (over TLS it passes).
- **The same handler.** A request comes to the same route and handler as over HTTP/1.1, with `request::proto()` `"HTTP/2.0"` (`"HTTP/1.1"` or `"HTTP/1.0"` otherwise; the client's `response::proto()` likewise). The response is the same [response_writer](response_writer.md): written whole, it goes as HEADERS (with its Content-Length) and DATA with END_STREAM; `async_flush` sends HEADERS and DATA without END_STREAM, and the rest follows. The fields of a connection (Connection, Keep-Alive, Proxy-Connection, Transfer-Encoding, Upgrade) are not sent, names go in lower case. `hijack` gives `std::errc::operation_not_supported`: a stream is not a connection (Go has no Hijacker in HTTP/2). The request's trailers are `request::trailers()` after its body; CONNECT is 405 in this version, and nothing is pushed.
- **Streams.** A connection carries up to `max_concurrent_streams` requests at once, each its handler in a task of its own; one past them is refused (REFUSED_STREAM, the client retries it). A request whose declared body is at most 16 KB, and which does not ask `Expect: 100-continue`, has its handler started when the first bytes of its body come rather than with its HEADERS (Go starts it with the HEADERS); the handler sees the same request. Flow control is per stream and per connection: the server announces windows of 1 MB and gives them back as the handler reads the body, and sends within the client's.
- **Limits.** `max_header_bytes` is the SETTINGS_MAX_HEADER_LIST_SIZE announced: a request's fields past it are answered 431 on its stream and the connection lives; a field block whose bytes pass max(2 × that, 64 KB) ends the connection before it is decoded (CVE-2023-45288). A malformed request (RFC 9113 §8.1.1, §8.2, §8.3.1: an upper-case name, a pseudo-field unknown, repeated or after a regular one, a field of a connection, TE other than trailers, :method, :scheme or :path missing, a Content-Length its DATA does not match, a pseudo-field in the trailers) is its stream's PROTOCOL_ERROR. Against rapid reset (CVE-2023-44487) at most `max_concurrent_streams` handlers run, more than four times that waiting ends the connection (Go's rule), and RST_STREAM from the client are held to 1000 at once and 33 a second after (nghttp2's). Floods of control frames (10 000 unanswered), of empty frames (1000 in a row) end the connection with ENHANCE_YOUR_CALM.
- **Timeouts.** `idle_timeout` is for a connection without streams. `read_timeout` and `write_timeout` hold each stream from its request's head: a request whose body has not come whole by `read_timeout` is reset (RST_STREAM CANCEL: its handler's reads fail), a response not whole by `write_timeout` is reset (INTERNAL_ERROR); the connection lives. Go, at its ReadTimeout, only closes the handler's body and sends nothing. A stream that waits for window longer than 30 s is reset too.
- **Shutdown.** `shutdown` sends GOAWAY as Go does (first with the largest identifier, then, after a PING's answer, the last stream seen), lets the streams already begun finish, and closes each connection when its last stream has; an idle connection is closed at once. `close` ends them all.
- **Conformance.** h2spec 2.6.0 against this server: 146 of 146 over TLS, 145 of 146 on the plain port (the preface case above).

## Members

### Routes and the log

```cpp
server();

template<class Handler> server& route(const string& pattern, Handler handler);   // void or async::task<> of (request, response_writer)
template<class Handler> server& not_found(Handler handler);
server& access_log(const slog::logger& log);     // a record per exchange through log, as given
server& access_log();                            // through the default logger, buffered
```

A handler for each pattern, and one for what no pattern matches (404 text/plain unless given), registered before `serve`; the access log, through the default logger or one of the program's.

### Serving

```cpp
expected<void, io::error> serve(const string& address) const;       // ":8080": until shutdown or close, then server_closed
async::task<expected<void, io::error>> async_serve(const string& address) const;
expected<void, io::error> serve(const net::listener& l) const;
async::task<expected<void, io::error>> async_serve(const net::listener& l) const;
expected<void, io::error> serve_tls(const string& address, const net::tls::config& c) const;   // ALPN completed (h2, http/1.1), as Go's ListenAndServeTLS
async::task<expected<void, io::error>> async_serve_tls(const string& address, const net::tls::config& c) const;
```

The server on an address it listens on, on a listener of the program's (TCP or TLS), or over TLS on an address; each runs until `shutdown` or `close` and then returns `server_closed`.

### Stopping

```cpp
void shutdown() const;
async::task<> async_shutdown() const;
void close() const;
```

`shutdown` lets the active connections finish their responses and returns when all have ended; `close` ends everything at once.

### Settings

```cpp
duration read_header_timeout = 10s;
duration read_timeout = duration::zero();       // the head and the body; zero: none
duration write_timeout = duration::zero();      // the response, from the end of the head
duration idle_timeout = 120s;
size_t max_header_bytes = 32 * 1024;
uint64_t max_body_bytes = 32 << 20;
function<void(const string&)> on_error;
bool http2 = true;                       // HTTP/2 when TLS agreed on "h2"
bool h2c = false;                        // HTTP/2 by prior knowledge on a plain port, beside HTTP/1.1
uint32_t max_concurrent_streams = 250;   // HTTP/2: streams a client may have open, handlers a connection runs (Go: 250)
```

The limits and timeouts of the [rules](#rules) with their defaults (a zero timeout is none), the handler of the server's errors (a line on stderr unless set), and [HTTP/2](#http2): by ALPN over TLS unless `http2` is off, by prior knowledge on a plain port when `h2c` is on.

## Examples

### Routes

```cpp
#include "sgcl/async/async.h"
#include "sgcl/core/core.h"
#include "sgcl/io/io.h"
#include "sgcl/net/http/http.h"
#include "sgcl/net/net.h"

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

### A file server

The files of a directory, in one call:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/net/http/http.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("public");
    (void)io::write_file("public/hello.txt", "hello from a file\n");
    (void)io::write_file("secret.txt", "not for the web\n");
    auto served = net::http::serve(":8080", "public");
    println(served.error().message());
}
```

A request to it:

```text
$ curl http://localhost:8080/hello.txt
hello from a file
$ curl http://localhost:8080/none.txt
Not Found
$ curl http://localhost:8080/..%2fsecret.txt
Not Found
```

A path value is unescaped, as Go's `PathValue` is: `%2f` in a segment is a `/` in the value and `..%2f` a way out of the directory, which the URL's normalization does not see (it resolves `..` segments, not what `%2f` makes). A handler of the program's that serves files does what `serve` does: the name through [`io::path::under`](../../io/path.md), which refuses one that leaves the directory (`io::errc::insecure_path`), and a 404 for it, as for a file that is not there.

### https in one call

The certificate and the key read from their files (here the test certificate of the tree), h2 and http/1.1 by ALPN:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/net/http/http.h"

using namespace sgcl;

int main() {
    auto served = net::http::serve_tls(":8443", "tests/net/tls_testdata/ecdsa.pem",
                                       "tests/net/tls_testdata/ecdsa.key",
                                       [](net::http::request req, net::http::response_writer w) {
                                           w.write("hello over " + req.proto() + "\n");
                                       });
    println(served.error().message());
}
```

A request to it:

```text
$ curl -s --cacert tests/net/tls_testdata/ca.pem https://localhost:8443/hello
hello over HTTP/2.0
$ curl -s --http1.1 --cacert tests/net/tls_testdata/ca.pem https://localhost:8443/hello
hello over HTTP/1.1
```

### https

The server over TLS with the test certificate of the tree (`tests/net/tls_testdata`: a leaf for localhost and 127.0.0.1, signed by a CA of its own), and a client that trusts that CA, by name and by address. Run from the root of the tree.

```cpp
#include "sgcl/async/async.h"
#include "sgcl/crypto/crypto.h"
#include "sgcl/io/io.h"
#include "sgcl/net/http/http.h"
#include "sgcl/net/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::config tls;
    tls.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    tls.alpn = {"http/1.1"};

    net::http::server srv;
    srv.route("GET /{name}", [](net::http::request req, net::http::response_writer w) {
        w.write("hello, " + req.path_value("name") + "\n");
    });
    // accept() gives connections after their handshake
    net::listener incoming = net::tls::listen("127.0.0.1:0", tls);
    auto serving = async::spawn(srv.async_serve(incoming));

    net::http::client web;
    web.tls.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    auto port = to_string(incoming.local_endpoint().port());
    net::http::response by_name = web.get("https://localhost:" + port + "/name");
    string one = by_name.text();
    print(one);
    // checked against the certificate's addresses
    net::http::response by_address = web.get("https://127.0.0.1:" + port + "/address");
    string two = by_address.text();
    print(two);

    srv.shutdown();
    println(serving.wait().error().code() == net::errc::server_closed);
}
```

Output:

```text
hello, name
hello, address
true
```

### HTTP/2 by prior knowledge

```cpp
#include "sgcl/async/async.h"
#include "sgcl/io/io.h"
#include "sgcl/net/http/http.h"
#include "sgcl/net/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.h2c = true;  // HTTP/2 by prior knowledge beside HTTP/1.1 on the same port
    srv.route("GET /hello", [](net::http::request req, net::http::response_writer w) {
        w.write("hello over " + req.proto() + "\n");
    });
    srv.route("GET /upgrade", [](net::http::request, net::http::response_writer w) {
        auto taken = w.hijack();  // an HTTP/2 stream is not a connection
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

    net::http::client h1;  // the same port, HTTP/1.1
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

### HTTP/2 over TLS

Over TLS with a listener of one's own, its ALPN given (run from the root of the tree, for the test certificate):

```cpp
#include "sgcl/async/async.h"
#include "sgcl/crypto/crypto.h"
#include "sgcl/io/io.h"
#include "sgcl/net/http/http.h"
#include "sgcl/net/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::config tls;
    tls.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    // a listener of one's own: its ALPN is given here (serve_tls completes it by itself)
    tls.alpn = {"h2", "http/1.1"};

    net::http::server srv;
    srv.route("POST /count", [](net::http::request req,
                                net::http::response_writer w) -> async::task<> {
        auto body = co_await req.async_bytes();
        w.write(req.proto() + ": " + to_string(body ? body->size() : 0) + " bytes\n");
    });
    net::listener incoming = net::tls::listen("127.0.0.1:0", tls);
    auto serving = async::spawn(srv.async_serve(incoming));

    net::http::client web;  // offers h2 first (client::http2)
    web.tls.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    auto url = "https://localhost:" + to_string(incoming.local_endpoint().port()) + "/count";
    net::http::response counted =
        web.post(url, "application/octet-stream", string(std::string(100000, 'x')));
    string text = counted.text();
    print(text);

    net::http::client old;
    old.http2 = false;  // HTTP/1.1 only: the server answers it as before
    old.tls.roots = web.tls.roots;
    net::http::response counted1 = old.post(url, "text/plain", "abc");
    string text1 = counted1.text();
    print(text1);

    srv.shutdown();
    println(serving.wait().error().code() == net::errc::server_closed);
}
```

Output:

```text
HTTP/2.0: 100000 bytes
HTTP/1.1: 3 bytes
true
```

## See also

- [request](request.md), [response_writer](response_writer.md): what a handler gets; [client](client.md): the other side
- [async timeout](../../async/timeout.md): a limit on `async_shutdown`; [tls](../tls.md): the listener of https, the identity
- `tests/net/http/https.cpp`: this server over TLS against this module's client and curl
- `tests/net/http/h2_server.cpp` (HTTP/2 against Go's client, `tools/h2_oracle.go`, and curl: h2c and TLS, 100 streams on one connection, bodies both ways, flushes, trailers, the stream timeouts, shutdown), `tests/net/http/h2_spec.cpp` (h2spec), `tests/net/http/h2_connection.cpp` (the connection machine by RFC section, the attacks)
- `tools/route_oracle.go` (the routes against Go's `ServeMux`: `go run tools/route_oracle.go > tests/net/http/route_oracle.h`), `tests/net/http/parser.cpp` (every rule of the head, by section, and the smuggling payloads), `tests/net/http/server.cpp` (limits, timeouts on the manual clock, shutdown, close, hijack), `tests/net/http/go.cpp` (Go's client against this server)
