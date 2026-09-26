# sgcl::net::http::client

```cpp
#include "sgcl/net/http/client.h"   // or "sgcl/net/http/http.h"

namespace sgcl::net::http {
    class client;   // HTTP/1.1 with a pool of connections; a handle of one word, copies share the pool
    using dial_function = function<async::task<expected<net::connection, io::error>>(const net::url&, async::stop_token)>;
}
```

Go's `http.Client` and `http.Transport` in one. A request is sent, redirects are followed, and the response comes back with its head read and its body still on the connection: `res.text()`, `res.bytes()` or `res.body()` read it, and the end of the body gives the connection back to the pool by itself (Go asks for both the end and `Close`). The settings are public fields, as in [`io::command`](../../io/exec.md), read by each request when it starts.

## Rules

- **Two forms.** `get`, `head`, `post` and `send` block the calling thread: the exchange runs on the scheduler and the thread waits for it, so they are for a thread of the program, never a worker. A task writes `co_await client.async_get(url)`; its body with `co_await res->async_text()`.
- **The pool** is keyed by the origin (scheme, host, port). Idle connections are taken last in first out, at most `max_idle_per_host` of them are kept (16; Go keeps 2), and one idle for `idle_timeout` (90 s) is closed by a timer of the module's clock. A connection goes back when the body of its response was read to its end and the server did not say `Connection: close`; one whose body was not read stays out (`close()` of the response gives it back when the rest is already in the buffer, and closes it otherwise).
- **One retry.** A request on a pooled connection that fails before a byte of the response (the server closed the connection while it was idle) is sent once more on a new one, for an idempotent method or a body held in memory (RFC 9110 §9.2.2), as Go does; a body given as a stream is never sent twice.
- **Redirects**, up to `max_redirects` (10; `too_many_redirects` past it): 301, 302 and 303 go on as GET without a body (HEAD stays HEAD, and 301 and 302 keep a GET), 307 and 308 keep the method and the body, unless the body is a stream (the 307 is then the response). `Authorization`, `Cookie`, `Proxy-Authorization` and `WWW-Authenticate` do not follow to a host that is neither the same nor under it. `res.url()` is the last URL.
- **Timeouts**: `timeout` bounds the whole exchange, the reading of the body included; `connect_timeout` the dial; `response_header_timeout` the wait for the head after the request went out. Each is `ETIMEDOUT` (`e.is_timeout()`).
- **What is sent**: `Host` from the URL (unless the request sets one), the program's fields (a value's CR, LF and NUL made spaces), a Content-Length for a body in memory or a stream of a known length, chunked for a stream without one, `Content-Length: 0` for a POST, PUT or PATCH without a body. No `Accept-Encoding` (there is no gzip yet, so servers do not compress) and no `User-Agent` unless the program sets one.
- **What is read**: the response is held to RFC 9112 as a request is on the server (a Transfer-Encoding with a Content-Length, two lengths, a coding but chunked, a broken chunk: `malformed_response`); 1xx responses before the final one are skipped (100 Continue, 103 Early Hints); a response to HEAD, a 204 and a 304 have no body; one with neither a length nor chunked lasts to the close, and its connection is not kept. A head past `max_response_header_bytes` (1 MB) is `header_too_large`.
- **https://** is `unsupported_scheme` until TLS, the next stage of the module; a URL that does not parse is `invalid_url`. A 4xx or a 5xx is a response, not an error.

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
dial_function dial;                                   // tcp::connect by default
```

## Example

```cpp
#include "sgcl/net/http/http.h"
#include <iostream>

using namespace sgcl;

async::task<> fetch(net::http::client client, string base) {
    auto res = co_await client.async_get(base + "/old");
    if (!res) {
        std::cerr << res.error().message() << '\n';
        co_return;
    }
    auto body = co_await res->async_text();
    std::cout << res->status() << ' ' << res->url().path() << ' ' << *body;

    net::http::request req("PUT", base + "/items/7");
    req.set_header("Content-Type", "text/plain").set_body("seven");
    auto put = co_await client.async_send(req);
    std::cout << put->status() << ' ' << *co_await put->async_text();
}

int main() {
    net::http::server server;
    server.route("GET /old", [](net::http::request, net::http::response_writer w) { w.redirect("/new"); });
    server.route("GET /new", [](net::http::request, net::http::response_writer w) { w.write("moved here\n"); });
    server.route("PUT /items/{id}", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        auto body = co_await req.async_text();
        w.set_status(net::http::status::created);
        w.write("item " + req.path_value("id") + " = " + *body + "\n");
    });
    auto listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(server.async_serve(*listener));

    net::http::client client;
    client.timeout = std::chrono::seconds(5);
    auto base = "http://127.0.0.1:" + to_string(listener->local_endpoint().port());
    async::spawn(fetch(client, base)).wait();

    server.close();
    serving.wait();
}
```

Output:

```text
200 /new moved here
201 item 7 = seven
```

## See also

- [request](request.md), [response](response.md): what is sent and what comes back; [server](server.md): the other side
- [url](../url.md): how the URL is read; [connection](../connection.md): what `dial` returns
- `tests/net/http/client.cpp` (a scripted server in memory: the pool, the retry, every framing, redirects, errors), `tests/net/http/go.cpp` (against Go's server)
