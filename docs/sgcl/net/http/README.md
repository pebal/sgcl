# sgcl::net::http

What Go has in `net/http` for HTTP/1.1: a client with a pool of connections, a server with the routes of Go 1.22's `ServeMux`, the messages both of them handle, headers, statuses and cookies. `#include "sgcl/net/http/http.h"` brings it in; it stands on the rest of [net](../README.md) (connections, [URLs](../url.md)), [async](../../async/README.md) and [io](../../io/README.md). This is stage 1c of the module; TLS, `https://` and HTTP/2 come with the next stage, after `crypto`, as another kind of connection under the same types.

## The names

Everything is in `sgcl::net::http`: [`client`](client.md), [`server`](server.md), [`request`](request.md), [`response`](response.md), [`response_writer`](response_writer.md), [`headers`](headers.md), [`cookie`](cookie.md) and the codes of [`status`](status.md). The errors are the module's, [`net::errc`](../error.md): `invalid_url`, `unsupported_scheme`, `malformed_response`, `header_too_large`, `body_too_large`, `too_many_redirects`, `server_closed`, each in an `io::error` that names the method and the URL (`GET http://x/: connection refused`).

## Waiting

As everywhere in io and net, an operation without a prefix does its work on the calling thread and returns the result, and the form for a task has the prefix `async_` and returns a task: `client.get(url)` and `co_await client.async_get(url)`, `res.text()` and `co_await res.async_text()`, `server.serve(":8080")` and `co_await server.async_serve(":8080")`. The exchange itself always runs on the scheduler, so a synchronous call is the task started and waited for: it is for a thread of the program (`main`), never for a worker. A handler runs on a worker, so a handler that reads a body or waits for anything is a task and uses the `async_` forms.

## Handles

A `client`, a `server`, a `request`, a `response` and a `response_writer` are handles of one word, as a [`net::connection`](../connection.md) is: a copy shares what is inside (the client's pool, the server's routes and connections), and a handle passed by value into a task keeps its object alive. `headers` and `cookie` are values.

## The wire

The parser (`detail/parser.h`) is pure: bytes in, a head or a status to refuse it with out. It holds to RFC 9112 and RFC 9110 and refuses everything that has carried request smuggling: a Transfer-Encoding with a Content-Length, a coding other than `chunked`, two different lengths, whitespace before a colon, obs-fold, a bare CR, LF alone inside chunked framing, a length or a chunk size that is not digits or overflows; the list is on the [server](server.md#rules) page. The tests hold each rule to a vector named by its section, and a mutator runs over valid requests under ASan.

## Pages

| page | header | what it is |
|---|---|---|
| [client](client.md) | `sgcl/net/http/client.h` | `http::client`: `get`, `head`, `post`, `send`, the pool, redirects, timeouts, `dial` |
| [server](server.md) | `sgcl/net/http/server.h` | `http::server`: `route` (Go 1.22 patterns), `serve`, `shutdown`, `close`, the limits and timeouts |
| [request](request.md) | `sgcl/net/http/request.h` | `http::request`: built by a client, received by a handler |
| [response](response.md) | `sgcl/net/http/response.h` | `http::response`: what a client receives |
| [response_writer](response_writer.md) | `sgcl/net/http/response_writer.h` | `http::response_writer`: what a handler writes; `flush`, `error`, `redirect`, `hijack` |
| [headers](headers.md) | `sgcl/net/http/headers.h` | `http::headers`: the fields of a head |
| [cookie](cookie.md) | `sgcl/net/http/cookie.h` | `http::cookie`: Set-Cookie written and read |
| [status](status.md) | `sgcl/net/http/status.h` | `http::status::ok`…, `http::reason(code)` |

## Example

```cpp
#include "sgcl/net/http/http.h"
#include <iostream>

using namespace sgcl;

int main() {
    net::http::server server;
    server.route("GET /hello/{name}", [](net::http::request req, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write("hello, " + req.path_value("name") + "\n");
    });
    auto listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(server.async_serve(*listener));

    net::http::client client;
    auto url = "http://127.0.0.1:" + to_string(listener->local_endpoint().port()) + "/hello/world";
    auto res = client.get(url);
    if (res) {
        std::cout << res->status() << ' ' << res->header("Content-Type") << '\n';
        std::cout << *res->text();
    }

    server.close();
    auto served = serving.wait();
    std::cout << (served.error().code() == net::errc::server_closed ? "server closed" : "?") << '\n';
}
```

Output:

```text
200 text/plain
hello, world
server closed
```

## SGCL and Go

| Go | sgcl::net::http | note |
|---|---|---|
| `http.Get(u)`, `Client.Do(r)` | `client.get(u)`, `client.send(r)`, and `co_await client.async_get(u)` | the pool in the client; the end of a body gives the connection back, with no `Close` |
| `http.Client{Timeout}`, `Transport{ResponseHeaderTimeout, IdleConnTimeout, MaxIdleConnsPerHost}` | `timeout`, `response_header_timeout`, `idle_timeout`, `max_idle_per_host` | 16 idle a host by default, Go 2 |
| `CheckRedirect`, `ErrUseLastResponse` | `max_redirects` | 301/302/303 as GET, 307/308 keep the method; credentials do not go to another host |
| `Transport.DialContext` | `client.dial` | a unix socket, a connection in memory; TLS in the next stage |
| `io.ReadAll(resp.Body)` | `res.text()`, `res.bytes()`, `res.body()` | |
| `resp.StatusCode`, `resp.Header.Get` | `res.status()`, `res.header(n)` | names compared without case, kept as written |
| `http.ServeMux`, `HandleFunc("GET /x/{id}")`, `r.PathValue` | `server.route("GET /x/{id}", h)`, `req.path_value("id")` | the same syntax and precedence, checked against Go's own mux |
| `http.ResponseWriter`, `WriteHeader`, `Write`, `Flusher` | `response_writer`, `set_status`, `write`, `async_flush` | `write` only buffers: an exact Content-Length unless flushed |
| `http.Error`, `http.Redirect`, `Hijacker` | `w.error(code)`, `w.redirect(u)`, `w.hijack()` | |
| `ListenAndServe`, `Serve`, `Shutdown`, `Close` | `serve(":8080")`, `serve(listener)`, `shutdown()`, `close()` | `async::timeout(s.async_shutdown(), 10s)` for a context's deadline |
| `ReadHeaderTimeout`, `ReadTimeout`, `WriteTimeout`, `IdleTimeout`, `MaxHeaderBytes` | the fields of the same names | 10 s and 32 KB by default (Go: none and 1 MB) |
| `http.MaxBytesReader` | `server.max_body_bytes` | 32 MB by default, 413 |
| `r.Context()` | `req.stop()` | stopped by `close()` and by a failed write |
| `http.Cookie`, `SetCookie` | `cookie`, `w.set_cookie(c)`, `req.cookie(name)` | no jar; `Expires` by RFC 6265's cookie-date |
| `http.TimeFormat`, `http.ParseTime` | `headers.set_date(n, t)`, `headers.date(n)` | `time::datetime`, the `time` module's `time::http` |
| `https`, HTTP/2, gzip, proxies | — | the next stage; the client sends no Accept-Encoding |
