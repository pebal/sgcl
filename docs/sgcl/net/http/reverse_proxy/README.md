[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::reverse_proxy

```cpp
#include "sgcl/net/http/reverse_proxy.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class reverse_proxy {
    public:
        enum class balancing : uint8_t;
        struct backend;
        struct options;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::http::reverse_proxy` is a handler of a [server](../server/README.md) that passes each request on to a backend
and its response back, Go's `httputil.ReverseProxy`: `server.route("/", net::http::reverse_proxy(url))`. The
outgoing request is the incoming one sent to the backend's URL, the backend's path joined before the request's and
the queries joined, through an [http::client](../client/README.md) — its TLS settings, HTTP/2, proxy and timeouts —
and the response comes back as it arrives: the head when the backend's comes, the body as it is read, flushed at once
for a stream of events or a body of unknown length, the trailers after it. An upgrade (WebSocket, any
`Upgrade` of HTTP/1.1) becomes a pipe of bytes between the two connections once the backend answers 101.

Several backends are balanced: in turn, by the fewest requests in progress, or at random
([balancing](../reverse_proxy-balancing.md)); a backend that fails is skipped for a while and, with a health path,
one that fails its checks; an idempotent request whose send failed goes again to another. Beyond Go's
`ReverseProxy`, which has one `Director` and no balancing, these are nginx's `upstream` in a type.

## Rules

- The hop-by-hop fields are not passed on, either way (RFC 9110 §7.6.1): Connection and the fields it names,
  Keep-Alive, Proxy-Connection, Proxy-Authorization, Proxy-Authenticate, TE (but `TE: trailers` when the client
  sent it), Trailer on a request, Transfer-Encoding, Upgrade (but in an upgrade). Expect is not passed on: the
  server answers `100 Continue` itself when the proxy reads the body.
- X-Forwarded-For gets the client's address appended, X-Forwarded-Host and X-Forwarded-Proto are replaced by the
  request's Host and scheme (`x_forwarded`, on by default); Forwarded (RFC 7239) and Via (RFC 9110 §7.6.3) are
  appended when the [options](../reverse_proxy-options.md) ask. A Forwarded that is not RFC 7239's is replaced, not
  appended to. The Host is the backend's unless `preserve_host`.
- The request's body is streamed to the backend as it comes, with its length or chunked; the response's body
  back as it is read. A 1xx of the backend (103 Early Hints) is passed on before the response
  ([send_informational](../response_writer/send_informational.md)); 100 Continue is the server's own.
- A backend that cannot be reached, or fails before its head, is answered 502 Bad Gateway, 504 Gateway Timeout for a
  timeout of the client's (`timeout`, `response_header_timeout`), or as `error_handler` says. A backend that fails
  half-way through a body breaks the response off: the connection ends (HTTP/1.1) or the stream is reset (HTTP/2),
  so the client never takes a cut body for a whole one; with nothing sent yet, the answer is 502.
- A client gone ends the backend's exchange: the request's [stop](../request/stop.md) (the server closed, the
  HTTP/2 connection lost) cancels it, and a write to the client that fails gives the backend's response up.
- The server's `max_body_bytes` bounds a proxied body too (32 MB by default): a proxy of uploads sets it to zero.
- A proxy is a handle of one word: a copy shares its backends and their counts, and a move is the copy.

## Member types

| Type | Definition |
|---|---|
| [balancing](../reverse_proxy-balancing.md) | how the backend of a request is chosen |
| [backend](../reverse_proxy-backend.md) | a backend and its counts |
| [options](../reverse_proxy-options.md) | the hooks, the client, flushing, the fields, balancing, health, retries |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](reverse_proxy.md) | constructs a proxy to one backend or several |
| `operator=` | makes the handle refer to another proxy |
| [operator()](operator_call.md) | the handler: passes a request on and its response back |
| [backends](backends.md) | the backends with their counts |
| [close](close.md) | stops the health checks, closes the idle connections |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::test_server backend([](net::http::request r, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write("backend saw " + r.method() + " " + r.url().request_target() + " from "
                + r.header("X-Forwarded-For") + "\n");
    });
    net::http::server front;
    front.route("/", net::http::reverse_proxy(backend.url() + "/api"));
    net::http::test_server proxy(front);

    net::http::response r = proxy.client().get(proxy.url() + "/users?id=7");
    print("{} {} | {}", r.status(), r.header("Content-Type"), r.text().value());
}
```

Output:

```text
200 text/plain | backend saw GET /api/users?id=7 from 127.0.0.1
```

## See also

- [server](../server/README.md): what runs it
- [client](../client/README.md): how it reaches the backends
- [websocket](../websocket/README.md): passed through as bytes
