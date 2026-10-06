[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [server](README.md)

# sgcl::net::http::server::use

```cpp
template<class M>
server& use(M middleware);
```

Adds a middleware around every request of the server ([middleware](../middleware.md)): around the routing, as Go's
`handler = cors(mux)`, so that a request of no route — the 404, the 405 and the redirects of the router — goes through it
too. The first given is the outermost; each passes the request on to the next, the last to the router. Read when
[serve](serve.md) is called, as the server's fields are.

The middleware is one of:

- a middleware of the library: [cors](../cors/README.md), [recovery](../recovery/README.md),
  [request_log](../request_log/README.md), [body_limit](../body_limit/README.md), [rate_limit](../rate_limit/README.md),
  [sessions](../sessions/README.md), [csrf](../csrf/README.md), [basic_auth](../basic_auth/README.md),
  [digest_auth](../digest_auth/README.md), [compression](../compression/README.md);
- a filter, a function `(request&, response_writer&) -> bool`: `true` passes the request on, `false` ends it with
  what the filter wrote;
- an around, a function `(request, response_writer, const handler& next) -> async::task<>`, which passes the request
  on with `co_await next(r, w)` ([handler](../handler/README.md)).

Anything else does not compile.

## Parameters

| Parameter | Description |
|---|---|
| `middleware` | a middleware of the library, a filter, or an around |

## Return value

`*this`.

## Complexity

Constant here. Each request then runs each middleware: a filter or a middleware of the library is a call, an around a
task.

## Exceptions

What the copy of `middleware` throws.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use([](net::http::request req, net::http::response_writer w,
               const net::http::handler& next) -> async::task<> {
        println("in {}", req.url().path());
        co_await next(req, w);
        println("out {}", w.status());
    });
    srv.route("GET /a", [](net::http::request, net::http::response_writer w) { w.write("a"); });
    net::http::response_recorder found, missing;
    found.serve(srv, net::http::test_request("GET", "/a"));
    missing.serve(srv, net::http::test_request("GET", "/b"));
}
```

Output:

```text
in /a
out 200
in /b
out 404
```

## See also

- [middleware](../middleware.md): the shape of a middleware
- [route](route.md)
- [server](README.md)
