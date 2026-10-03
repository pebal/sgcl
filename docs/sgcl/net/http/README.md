[sgcl](../../README.md) › [net](../README.md) › http

# sgcl::net::http

```cpp
#include "sgcl/net/http.h"   // namespace sgcl::net::http
```

What Go has in `net/http` for HTTP/1.1 and HTTP/2: a [client](client/README.md) with a pool of connections, a
[server](server/README.md) with the routes of Go 1.22's `ServeMux`, the messages both of them handle ([request](request/README.md),
[response](response/README.md), [response_writer](response_writer/README.md)), [headers](headers/README.md), [cookies](cookie/README.md) and the
codes of [status](status.md). It stands on the rest of [net](../README.md) (connections, [URLs](../url/README.md)), on
[async](../../async/README.md) and on [io](../../io/README.md). `https://` goes over [TLS 1.3](../tls/README.md), and
HTTP/2 (RFC 9113) comes by ALPN over TLS, or by prior knowledge on a plain port (`h2c`), under the same types and
handlers.

The idea it rests on is that an exchange is a task on the scheduler and the program chooses how to wait for it. A
handler is a plain function of `(request, response_writer)` when it never waits — a write only buffers, and the
server sends the response whole with its Content-Length when the handler returns — and a task when it reads a body
or calls another service; a client's call is the task started and waited for on a thread, or awaited in a task. The
messages are handles, so a request, a response or a writer passed into a task keeps what it needs alive, and the
bytes of the wire go through one buffer per connection, from which a head is copied once into a string whose slices
its fields are.

Errors are values, an `expected<T, io::error>` ([io::error](../../io/error/README.md)). The codes of the module are
[net::errc](../errc.md)'s: `invalid_url`, `unsupported_scheme`, `malformed_response`, `header_too_large`,
`body_too_large`, `too_many_redirects`, `server_closed`, `invalid_cookie`, `http_status`, each in an `io::error` that
names the method and the URL (`GET http://x/: connection refused`). A 4xx or a 5xx is a response, not an error.

## The rules

1. As everywhere in io and net, an operation without a prefix does its work on the calling thread and returns the
   result, and the form for a task has the prefix `async_` and returns a task: `web.get(url)` and `co_await
   web.async_get(url)`, `res.text()` and `co_await res.async_text()`, `server.serve(":8080")` and `co_await
   server.async_serve(":8080")`. The exchange itself always runs on the scheduler, so a synchronous call is the task
   started and waited for: it is for a thread of the program (`main`), never for a worker. A handler runs on a
   worker, so a handler that reads a body or waits for anything is a task and uses the `async_` forms.
2. A [request](request/README.md), a [response](response/README.md) and a [response_writer](response_writer/README.md) are handles of one
   word, a `tracked_ptr` to the message, as a [connection](../connection/README.md) is: a copy shares the message, and a
   handle passed by value into a task keeps it alive. A [client](client/README.md) and a [server](server/README.md) are a word to
   what the copies share (the client's pool, the server's routes and connections) and their settings, which are each
   copy's own. All of them hold a `tracked_ptr`, so they live where one may: on a stack, in a task, in a managed
   object; in a global or a `std` container, a [rooted](../../core/rooted/README.md) of them. [headers](headers/README.md) and
   [cookie](cookie/README.md) are values.
3. The parser is pure: bytes in, a head or a status to refuse it with out. It holds to RFC 9112 and RFC 9110 and
   refuses everything that has carried request smuggling: a Transfer-Encoding with a Content-Length, a coding other
   than `chunked`, two different lengths, whitespace before a colon, obs-fold, a bare CR, LF alone inside chunked
   framing, a length or a chunk size that is not digits or overflows; the whole list is in the
   [server's rules](server/README.md#the-head). The tests hold each rule to a vector named by its section, and a mutator
   runs over valid requests under ASan.
4. A connection's bytes go through one buffer in front of it, unmanaged memory the connection owns (8 KB, grown for a
   head that does not fit). A head is copied out into a string of its size, whose slices its fields are, one list of
   their number; a body is copied out (read whole: into a vector of its declared length, or gathered and copied
   once), or dropped where it lies when a handler left it. No slice of the buffer leaves it, and it is read only by
   the connection's reads in the task, never on the pool.
5. Not in this version: gzip and proxies. The client sends no `Accept-Encoding`.

## Functions

| Function | Header | Description |
|---|---|---|
| [download, async_download](download.md) | `download.h` | a URL saved to a file through a client of the process's; a status but 2xx is an error |
| [reason](reason.md) | `status.h` | the reason phrase of a status, `Not Found` for 404 |
| [serve, async_serve](serve.md) | `serve.h` | the files of a directory over HTTP, in one call |
| [serve_tls](serve_tls.md) | `serve.h` | one handler over https, the certificate and the key read from their files |

## Classes

| Class | Header | Description |
|---|---|---|
| [client](client/README.md) | `client.h` | HTTP/1.1 and HTTP/2 with a pool of connections, redirects and timeouts: Go's `http.Client` |
| [cookie](cookie/README.md) | `cookie.h` | a cookie, the Set-Cookie field written and read: Go's `http.Cookie` |
| [headers](headers/README.md) | `headers.h` | the fields of a head, names compared without case and kept as written |
| [request](request/README.md) | `request.h` | a request: made for a client, received by a handler |
| [response](response/README.md) | `response.h` | what a client receives: the status, the fields, the body |
| [response_writer](response_writer/README.md) | `response_writer.h` | the response a handler writes: Go's `http.ResponseWriter` |
| [server](server/README.md) | `server.h` | routes, limits, timeouts, HTTP/2 and shutdown: Go's `http.Server` and `ServeMux` in one |

## Namespaces

| Namespace | Header | Description |
|---|---|---|
| [status](status.md) | `status.h` | the status codes of the IANA registry as `int` constants, `status::ok`, `status::not_found`... |

## Objects and types

| Name | Header | Description |
|---|---|---|
| `dial_function` | `client.h` | `function<async::task<expected<net::connection, io::error>>(const net::url&, async::stop_token)>`: what makes a client's connections, its member `dial` ([client](client/README.md)) |

## See also

- [Benchmarks](benchmarks.md): the server under load, HTTP/1.1, https and HTTP/2, against Go's
- [net](../README.md): the connections and the URLs under it
- [tls](../tls/README.md): https
- [The modules](../../README.md)
