[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [csrf](csrf/README.md) › options

# sgcl::net::http::csrf::options

```cpp
#include "sgcl/net/http/csrf.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class csrf {
    public:
        struct options {
            tokens kind = tokens::none;
            vector<string> trusted_origins;
            string header = "X-CSRF-Token";
            string field = "csrf_token";
            string cookie = "__Host-csrf";
            function<void(const request&, response_writer&)> deny;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::csrf::options` is what a [csrf](csrf/README.md) asks of a request and how it refuses one: a plain
struct, its fields set by name (`{.kind = net::http::csrf::tokens::synchronizer}`).

## Member objects

| Member | Description |
|---|---|
| `kind` | the tokens an unsafe request must send back ([csrf::tokens](csrf-tokens.md)); `none` by default: the origin check alone |
| `trusted_origins` | origins whose requests pass the origin check, as a browser serializes them (`"https://app.example"`); none by default |
| `header` | the field a script sends the token in; `"X-CSRF-Token"` by default |
| `field` | the field of an `application/x-www-form-urlencoded` form that carries the token; `"csrf_token"` by default |
| `cookie` | the double-submit cookie's name, set `Secure`, `HttpOnly`, `SameSite=Lax`, `Path=/`; `"__Host-csrf"` by default |
| `deny` | the answer to a refused request; 403 Forbidden by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::csrf::options o;
    o.trusted_origins = {"https://admin.example"};
    o.deny = [](const net::http::request&, net::http::response_writer& w) {
        w.set_status(403);
        w.write("{\"error\":\"cross-site\"}");
    };
    net::http::server srv;
    srv.use(net::http::csrf(o));
    srv.route("POST /x", [](net::http::request, net::http::response_writer w) { w.write("ok"); });
    for (const char* origin : {"https://admin.example", "https://evil.example"}) {
        auto req = net::http::test_request("POST", "/x");
        req.set_header("Origin", origin);
        net::http::response_recorder rec;
        rec.serve(srv, req);
        println("{} {}", rec.status(), rec.body());
    }
}
```

Output:

```text
200 ok
403 {"error":"cross-site"}
```

## See also

- [csrf](csrf/README.md)
- [sgcl::net::http](README.md)
