[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [cors](cors/README.md) › options

# sgcl::net::http::cors::options

```cpp
#include "sgcl/net/http/middleware.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class cors {
    public:
        struct options {
            vector<string> origins = {"*"};
            function<bool(const string&)> allow_origin;
            vector<string> methods = {"GET", "HEAD", "PUT", "PATCH", "POST", "DELETE"};
            vector<string> headers;
            vector<string> exposed_headers;
            bool credentials = false;
            duration max_age = std::chrono::minutes(5);
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::cors::options` is what a [cors](cors/README.md) allows: a plain struct, its fields set by name
(`{.origins = {"https://app.example"}, .credentials = true}`), read when the middleware is made.

## Member objects

| Member | Description |
|---|---|
| `origins` | the origins allowed, as a browser serializes them: `"https://app.example"`, `"http://localhost:8080"`, compared exactly; `"*"` is any origin but `"null"`. `{"*"}` by default |
| `allow_origin` | a predicate of the program's, given the request's `Origin`, used instead of `origins` when set |
| `methods` | the methods a preflight allows (`Access-Control-Allow-Methods`); GET, HEAD and POST are allowed whatever the list says |
| `headers` | the request headers allowed (`Access-Control-Allow-Headers`); empty, the default: whatever a preflight asks for |
| `exposed_headers` | the response headers a script may read beyond the safelisted ones (`Access-Control-Expose-Headers`) |
| `credentials` | cookies and `Authorization` allowed (`Access-Control-Allow-Credentials: true`), the origin echoed rather than `*`; `false` by default |
| `max_age` | how long a browser keeps a preflight's answer (`Access-Control-Max-Age`, in seconds); 5 minutes by default, zero for none sent |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::cors::options o;
    o.allow_origin = [](const string& origin) { return origin.ends_with(".corp.example"); };
    o.exposed_headers = {"X-Total"};
    net::http::server srv;
    srv.use(net::http::cors(o));
    srv.route("GET /n", [](net::http::request, net::http::response_writer w) { w.write("n"); });
    auto req = net::http::test_request("GET", "/n");
    req.set_header("Origin", "https://hr.corp.example");
    net::http::response_recorder rec;
    rec.serve(srv, req);
    println("{} {}", rec.header("Access-Control-Allow-Origin"), rec.header("Access-Control-Expose-Headers"));
}
```

Output:

```text
https://hr.corp.example X-Total
```

## See also

- [cors](cors/README.md)
- [sgcl::net::http](README.md)
