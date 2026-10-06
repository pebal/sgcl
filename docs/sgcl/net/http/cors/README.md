[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::cors

```cpp
#include "sgcl/net/http/middleware.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class cors;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

Cross-origin requests by the Fetch standard's CORS protocol (§3.2), a [middleware](../middleware.md): what lets a script
of one origin read the answers of a server of another. Go's standard library has none (rs/cors and gorilla/handlers are
the usual packages). A request without `Origin` is not a CORS request and passes as it is. A preflight — `OPTIONS` with
`Origin` and `Access-Control-Request-Method` — is answered by the middleware, never by a route: 204 with the
permissions (`Access-Control-Allow-Origin`, `-Allow-Methods`, `-Allow-Headers`, `-Allow-Credentials`, `-Max-Age`)
when the origin, the method and the headers are allowed, 204 with none when not, which the browser takes as a refusal.
An actual request from an allowed origin gets `Access-Control-Allow-Origin` and `-Expose-Headers`; from another origin
it is still served, and the browser keeps the answer from the script.

## Rules

- `Access-Control-Allow-Origin` is `*` for any origin without credentials; with credentials, or a list of origins, it is
  the request's origin echoed, with `Vary: Origin` so that a cache keys the answer by it (the standard forbids `*`
  with credentials).
- `"*"` among the origins is every origin but `null`, the opaque origin of a sandboxed frame or a file, which must be
  named to be allowed.
- The CORS-safelisted methods (GET, HEAD, POST) and request headers (Accept, Accept-Language, Content-Language) are
  always allowed, as the standard has it.
- A handle of one word: copies share the settings, decided once when it is made.

## Member types

| Type | Definition |
|---|---|
| [options](../cors-options.md) | the origins, the methods, the headers, credentials, the preflight's lifetime |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](cors.md) | the middleware of the options |
| [wrap](wrap.md) | the middleware around one handler |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::cors({.origins = {"https://app.example"}, .credentials = true}));
    srv.route("POST /orders", [](net::http::request, net::http::response_writer w) { w.write("ordered"); });

    auto preflight = net::http::test_request("OPTIONS", "/orders");
    preflight.set_header("Origin", "https://app.example");
    preflight.set_header("Access-Control-Request-Method", "POST");
    preflight.set_header("Access-Control-Request-Headers", "content-type");
    net::http::response_recorder rec;
    rec.serve(srv, preflight);
    println("{} {}", rec.status(), rec.header("Access-Control-Allow-Origin"));
    println("{}", rec.header("Access-Control-Allow-Headers"));
}
```

Output:

```text
204 https://app.example
content-type
```

## See also

- [cors::options](../cors-options.md)
- [csrf](../csrf/README.md): the other side, requests from another origin refused
- [middleware](../middleware.md)
- [sgcl::net::http](../README.md)
