[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cors](README.md)

# sgcl::net::http::cors::wrap

```cpp
handler wrap(const handler& next) const;
```

The middleware around one handler, for a route of its own (Go's `cors(handler)`): what [server::use](../server/use.md)
does for every request, done for the requests of that route. A preflight reaches it only when the route takes the
`OPTIONS` method (a pattern without one, `"/api/"`): a route of `"GET /x"` answers a preflight 405 before the
middleware sees it, which [server::use](../server/use.md) does not.

## Parameters

| Parameter | Description |
|---|---|
| `next` | the handler the request goes on to |

## Return value

The handler of the middleware and `next`.

## Complexity

Constant.

## Exceptions

What the copy of a function throws.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    net::http::cors open;
    srv.route("/public/", open.wrap([](net::http::request, net::http::response_writer w) { w.write("open"); }));
    srv.route("/private/", [](net::http::request, net::http::response_writer w) { w.write("closed"); });
    for (const char* path : {"/public/a", "/private/a"}) {
        auto req = net::http::test_request("GET", path);
        req.set_header("Origin", "https://x.example");
        net::http::response_recorder rec;
        rec.serve(srv, req);
        println("{}: [{}]", rec.body(), rec.header("Access-Control-Allow-Origin"));
    }
}
```

Output:

```text
open: [*]
closed: []
```

## See also

- [server::use](../server/use.md)
- [cors](README.md)
