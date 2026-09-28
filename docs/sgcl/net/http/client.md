# sgcl::net::http::client

```cpp
#include "sgcl/net/http/client.h"   // or "sgcl/net/http/http.h"

namespace sgcl::net::http {
    class client;   // HTTP/1.1 and HTTP/2 with a pool of connections; a handle of one word, copies share the pool
    using dial_function = function<async::task<expected<net::connection, io::error>>(const net::url&, async::stop_token)>;
}
```

Go's `http.Client` and `http.Transport` in one. A request is sent, redirects are followed, and the response comes back with its head read and its body still on the connection: `res.text()`, `res.bytes()` or `res.body()` read it, and the end of the body gives the connection back to the pool by itself (Go asks for both the end and `Close`). The settings are public fields, as in [`io::command`](../../io/exec.md), read by each request when it starts. Over TLS it speaks HTTP/2 when the server chooses it (RFC 9113, as Go's Transport does by default): one connection per origin shared by the requests, a stream each, with nothing of the program's code changed but what `res.proto()` says.

## Rules

- **Two forms.** `get`, `head`, `post` and `send` block the calling thread: the exchange runs on the scheduler and the thread waits for it, so they are for a thread of the program, never a worker. A task writes `co_await client.async_get(url)`; its body with `co_await res.async_text()`.
- **The pool** is keyed by the origin (scheme, host, port). Idle connections are taken last in first out, at most `max_idle_per_host` of them are kept (16; Go keeps 2), and one idle for `idle_timeout` (90 s) is closed by a timer of the module's clock. A connection goes back when the body of its response was read to its end and the server did not say `Connection: close`; one whose body was not read stays out (`close()` of the response gives it back when the rest is already in the buffer, and closes it otherwise).
- **Retries.** A request on a pooled HTTP/1.1 connection that fails before a byte of the response (the server closed the connection while it was idle) is sent once more on a new one, for an idempotent method or a body held in memory (RFC 9110 §9.2.2), as Go does. Over HTTP/2 a stream the server never processed goes again by itself, up to two more times: one it refused (`REFUSED_STREAM`, RFC 9113 §8.7) for any method, one above the last stream of its GOAWAY (§6.8) or lost with its connection before the head only for an idempotent method (Go sends any method again after a GOAWAY; this version does not). A body given as a stream is never sent twice.
- **Redirects**, up to `max_redirects` (10; `too_many_redirects` past it): 301, 302 and 303 go on as GET without a body (HEAD stays HEAD, and 301 and 302 keep a GET), 307 and 308 keep the method and the body, unless the body is a stream (the 307 is then the response). `Authorization`, `Cookie`, `Proxy-Authorization` and `WWW-Authenticate` do not follow to a host that is neither the same nor under it. `res.url()` is the last URL.
- **Timeouts**: `timeout` bounds the whole exchange, the reading of the body included; `connect_timeout` the dial; `response_header_timeout` the wait for the head after the request went out. Each is `ETIMEDOUT` (`e.is_timeout()`). Over HTTP/2 they hold for the request's stream alone: past one the stream is reset (`CANCEL`) and the connection goes on carrying the other requests.
- **What is sent**: `Host` from the URL (unless the request sets one), the program's fields, a Content-Length for a body in memory or a stream of a known length, chunked for a stream without one, `Content-Length: 0` for a POST, PUT or PATCH without a body. No `Accept-Encoding` (there is no gzip yet, so servers do not compress) and no `User-Agent` unless the program sets one.
- **What cannot be sent** is refused before a connection is dialed: a method that is not a token, a name that is not one, a value with CR, LF, NUL or another control (obs-fold among them), a target or `Host` with a space or a control make `send` (and `get`, `post`, the async forms) `std::errc::invalid_argument`, the part named (`invalid header value: X-Foo`), not an exception and not a byte on the wire.
- **What is read**: the response is held to RFC 9112 as a request is on the server (a Transfer-Encoding with a Content-Length, two lengths, a coding but chunked, a broken chunk: `malformed_response`); 1xx responses before the final one are skipped (100 Continue, 103 Early Hints); a response to HEAD, a 204 and a 304 have no body; one with neither a length nor chunked lasts to the close, and its connection is not kept. A head past `max_response_header_bytes` (1 MB) is `header_too_large`.
- **https://** is over [TLS 1.3](../tls.md): the connection is made by `net::tls::connect` with the `tls` settings (the system's roots unless `tls.roots` names others), port 443 unless the URL gives one. ALPN offers `h2` first and then what `tls.alpn` holds (`http/1.1`); `http2 = false` leaves `h2` out. The server's name is the URL's host, an IP address checked against the certificate's addresses; `tls.server_name` overrides it. A certificate that does not verify fails the request with the [tls error](../tls.md) (`tls: certificate signed by unknown authority`, `certificate_reason(e)`). A `dial` function given makes the transport and TLS goes over it, as over Go's `DialContext`. The pool keeps https and http connections apart (the origin includes the scheme), and a redirect may go from one to the other by the rule above.
- **HTTP/2** comes when the server chooses `h2` by ALPN, or on `http://` by prior knowledge when `h2c` is on (the preface sent at once, no Upgrade: for a server known to speak it, such as a service behind the program's own proxy). The origin gets one connection, shared by every request to it, each on a stream of its own (RFC 9113); a second one is dialed only when the first carries as many streams as the server's `SETTINGS_MAX_CONCURRENT_STREAMS` allows. Requests that find no connection wait for the one being dialed rather than each dialing its own. A new connection to an origin starts from the limit its server already told another connection, and a first one from 100 until the server's SETTINGS come, as Go's does. A request's body goes as DATA within the server's windows. A response's body is read within ours: 1 MB a stream and 16 MB the connection. A body nobody reads stops its stream at 1 MB, and the other streams go on (Go's 4 MB and 1 GB were not taken, since memory comes first; 1 MB at 100 ms of RTT is some 80 Mb/s a stream). The response's fields are held to RFC 9113 §8.3 (`:status` alone and first, names in lower case, no fields of HTTP/1.1's connection; else `malformed_response`), a list past `max_response_header_bytes` is `header_too_large`, and 1xx responses are skipped as over HTTP/1.1. A connection without streams for `idle_timeout` is closed with GOAWAY, and so is one that is idle when `close_idle_connections()` is called. A server that chooses `http/1.1` gets HTTP/1.1 and its pool, as before.
- **Not in this version** (HTTP/2): server push (the client sends `ENABLE_PUSH = 0`), priorities (RFC 9218), `CONNECT` and Extended CONNECT (WebSocket over HTTP/2), trailers on a request, a PING to test an idle connection and a write deadline of the connection's own (Go's `ReadIdleTimeout` and `WriteByteTimeout`, off by default there too): a connection whose server has gone silent is found only by the timeouts of the requests on it.
- Any other scheme is `unsupported_scheme`; a URL that does not parse is `invalid_url`. A 4xx or a 5xx is a response, not an error.

## Members

```cpp
client();

expected<response, io::error> send(const request& req) const;
async::task<expected<response, io::error>> async_send(const request& req) const;
expected<response, io::error> get(const string& url) const;
async::task<expected<response, io::error>> async_get(const string& url) const;
expected<response, io::error> head(const string& url) const;
async::task<expected<response, io::error>> async_head(const string& url) const;
expected<response, io::error> post(const string& url, const string& content_type, const string& body) const;
async::task<expected<response, io::error>> async_post(const string& url, const string& content_type, const string& body) const;

void close_idle_connections() const;     // the pool's idle connections closed now

duration timeout = duration::zero();                  // the whole exchange; zero: none
duration connect_timeout = 30s;
duration response_header_timeout = duration::zero();
duration idle_timeout = 90s;
size_t max_idle_per_host = 16;
int max_redirects = 10;
size_t max_response_header_bytes = 1 << 20;
dial_function dial;                                   // tcp::connect by default; for https the transport under TLS
net::tls::config tls;                                 // https: roots, groups, ciphers, handshake_timeout...; ALPN http/1.1
bool http2 = true;                                    // https: h2 offered first by ALPN
bool h2c = false;                                     // http://: HTTP/2 by prior knowledge
```

## Example

```cpp
#include "sgcl/net/http/http.h"
#include "sgcl/io/print.h"

using namespace sgcl;

async::task<> fetch(net::http::client web, string base) {
    net::http::response res = co_await web.async_get(base + "/old");
    string body = co_await res.async_text();
    print("{} {} {}", res.status(), res.url().path(), body);

    net::http::request req("PUT", base + "/items/7");
    req.set_header("Content-Type", "text/plain").set_body("seven");
    net::http::response put = co_await web.async_send(req);
    string reply = co_await put.async_text();
    print("{} {}", put.status(), reply);
}

int main() {
    net::http::server srv;
    srv.route("GET /old", [](net::http::request, net::http::response_writer w) { w.redirect("/new"); });
    srv.route("GET /new", [](net::http::request, net::http::response_writer w) { w.write("moved here\n"); });
    srv.route("PUT /items/{id}", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        string body = co_await req.async_text();
        w.set_status(net::http::status::created);
        w.write("item " + req.path_value("id") + " = " + body + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));

    net::http::client web;
    web.timeout = std::chrono::seconds(5);
    auto base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    async::spawn(fetch(web, base)).wait();

    srv.close();
    serving.wait();
}
```

Output:

```text
200 /new moved here
201 item 7 = seven
```

### https

A server of the same program over TLS, with the test certificate of the tree (`tests/net/tls_testdata`: a CA of its own and a leaf for localhost and 127.0.0.1), and two clients: one that trusts that CA, and one with the system's roots, which do not hold it. Run from the root of the tree.

```cpp
#include "sgcl/net/http/http.h"
#include "sgcl/io/print.h"

using namespace sgcl;

int main() {
    net::tls::config server_tls;
    server_tls.identities = {net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"), io::read_text("tests/net/tls_testdata/ecdsa.key"))};
    net::http::server srv;
    srv.route("GET /hello", [](net::http::request, net::http::response_writer w) {
        w.write("hello over TLS\n");
    });
    net::listener listener = net::tls::listen("127.0.0.1:0", server_tls);
    auto serving = async::spawn(srv.async_serve(listener));

    net::http::client web;
    web.tls.roots = crypto::x509::certificate_pool::from_pem(io::read_text("tests/net/tls_testdata/ca.pem"));
    auto base = "https://localhost:" + to_string(listener.local_endpoint().port());
    net::http::response res = web.get(base + "/hello");
    string text = res.text();
    print(text);

    net::http::client strict;                  // the system's roots: the test CA is not among them
    auto refused = strict.get(base + "/hello");
    println("{}", refused.error().message());

    srv.close();
    serving.wait();
}
```

Output (the port is the system's choice):

```text
hello over TLS
GET https://localhost:65008/hello: tls: certificate signed by unknown authority
```

### HTTP/2

The same server over TLS with `h2` in its ALPN list, and three requests sent at once: three streams of one connection. A request whose deadline passes resets its stream alone, and the next request goes on the same connection. Then a client with `http2` off gets HTTP/1.1 from the same server. Run from the root of the tree.

```cpp
#include "sgcl/net/http/http.h"
#include "sgcl/io/print.h"

using namespace sgcl;

async::task<string> fetch(net::http::client web, string url) {
    net::http::response res = co_await web.async_get(url);
    string body = co_await res.async_text();
    co_return res.proto() + ": " + body;
}

int main() {
    net::tls::config server_tls;
    server_tls.identities = {net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"), io::read_text("tests/net/tls_testdata/ecdsa.key"))};
    server_tls.alpn = {"h2", "http/1.1"};
    net::http::server srv;
    srv.route("GET /items/{id}", [](net::http::request req, net::http::response_writer w) {
        w.write("item " + req.path_value("id") + "\n");
    });
    srv.route("GET /slow", [](net::http::request, net::http::response_writer w) -> async::task<> {
        co_await async::after(std::chrono::seconds(1));
        w.write("late\n");
    });
    net::listener listener = net::tls::listen("127.0.0.1:0", server_tls);
    auto serving = async::spawn(srv.async_serve(listener));

    net::http::client web;
    web.tls.roots = crypto::x509::certificate_pool::from_pem(io::read_text("tests/net/tls_testdata/ca.pem"));
    auto base = "https://localhost:" + to_string(listener.local_endpoint().port());
    auto first = async::spawn(fetch(web, base + "/items/1"));
    auto second = async::spawn(fetch(web, base + "/items/2"));
    auto third = async::spawn(fetch(web, base + "/items/3"));
    print(first.wait());
    print(second.wait());
    print(third.wait());

    web.timeout = std::chrono::milliseconds(200);
    auto slow = web.get(base + "/slow");
    println("{}", slow.error().is_timeout());
    web.timeout = std::chrono::seconds(5);
    print(fetch(web, base + "/items/4").wait());

    net::http::client older = web;
    older.http2 = false;
    print(fetch(older, base + "/items/5").wait());

    srv.close();
    serving.wait();
}
```

Output:

```text
HTTP/2.0: item 1
HTTP/2.0: item 2
HTTP/2.0: item 3
true
HTTP/2.0: item 4
HTTP/1.1: item 5
```

Over plain TCP a server with `h2c` on answers HTTP/2 by prior knowledge and HTTP/1.1 on the same port. A client with `h2c` on sends `http://` as HTTP/2:

```cpp
#include "sgcl/net/http/http.h"
#include "sgcl/io/print.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.h2c = true;
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        w.write("asked over " + req.proto() + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    auto url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/";

    net::http::client multiplexed;
    multiplexed.h2c = true;
    net::http::response res = multiplexed.get(url);
    string body = res.text();
    print("{}: {}", res.proto(), body);

    net::http::client plain;
    net::http::response old = plain.get(url);
    string text = old.text();
    print("{}: {}", old.proto(), text);

    srv.close();
    serving.wait();
}
```

Output:

```text
HTTP/2.0: asked over HTTP/2.0
HTTP/1.1: asked over HTTP/1.1
```

## See also

- [request](request.md), [response](response.md): what is sent and what comes back; [server](server.md): the other side
- [url](../url.md): how the URL is read; [connection](../connection.md): what `dial` returns
- [tls](../tls.md): the connection under https and its errors
- `tests/net/http/client.cpp` (a scripted server in memory: the pool, the retry, every framing, redirects, errors), `tests/net/http/go.cpp` (against Go's server), `tests/net/http/https.cpp` (https: against this module's server, Go's net/http over TLS and curl), `tests/net/http/h2_client.cpp` (HTTP/2 against Go's server by `tools/h2_oracle.go`, and a scripted server for REFUSED_STREAM, GOAWAY and a connection cut), `tests/net/http/h2_client_connection.cpp` (the client's side of the HTTP/2 machine, frame by frame, by the sections of RFC 9113)
