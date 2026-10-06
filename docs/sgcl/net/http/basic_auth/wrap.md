[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [basic_auth](README.md)

# sgcl::net::http::basic_auth::wrap

```cpp
handler wrap(const handler& next) const;
```

The middleware around one handler, for a route of its own: the requests of that route asked for the password, the
other routes open.

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
    net::http::basic_auth admin("admin", [](const string& u, const string& p) { return u == "root" && p == "x"; });
    net::http::server srv;
    srv.route("GET /admin/", admin.wrap([](net::http::request, net::http::response_writer w) { w.write("panel"); }));
    srv.route("GET /", [](net::http::request, net::http::response_writer w) { w.write("home"); });
    for (const char* path : {"/", "/admin/"}) {
        net::http::response_recorder rec;
        rec.serve(srv, net::http::test_request("GET", path));
        println("{} {}", path, rec.status());
    }
}
```

Output:

```text
/ 200
/admin/ 401
```

## See also

- [server::use](../server/use.md)
- [basic_auth](README.md)
