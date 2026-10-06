[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [digest_auth](README.md)

# sgcl::net::http::digest_auth::wrap

```cpp
handler wrap(const handler& next) const;
```

The middleware around one handler, for a route of its own: the requests of that route asked for credentials, the
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
    net::http::digest_auth files("files", [](const string& u) -> optional<string> {
        if (u == "ann") {
            return string("secret");
        }
        return nullopt;
    });
    net::http::server srv;
    srv.route("GET /private/", files.wrap([](net::http::request req, net::http::response_writer w) {
        w.write("for " + req.authenticated_user());
    }));
    srv.route("GET /", [](net::http::request, net::http::response_writer w) { w.write("open"); });
    for (const char* path : {"/", "/private/a"}) {
        net::http::response_recorder rec;
        rec.serve(srv, net::http::test_request("GET", path));
        println("{} {}", path, rec.status());
    }
}
```

Output:

```text
/ 200
/private/a 401
```

## See also

- [server::use](../server/use.md)
- [digest_auth](README.md)
