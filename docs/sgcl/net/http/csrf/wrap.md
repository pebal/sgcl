[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [csrf](README.md)

# sgcl::net::http::csrf::wrap

```cpp
handler wrap(const handler& next) const;
```

The middleware around one handler, for a route of its own: the requests of that route checked, the others not.

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
    net::http::csrf guard;
    auto ok = [](net::http::request, net::http::response_writer w) { w.write("ok"); };
    net::http::server srv;
    srv.route("POST /account", guard.wrap(ok));
    srv.route("POST /webhook", ok);   // signed by its sender, from anywhere
    for (const char* path : {"/account", "/webhook"}) {
        auto req = net::http::test_request("POST", path);
        req.set_header("Origin", "https://elsewhere.example");
        net::http::response_recorder rec;
        rec.serve(srv, req);
        println("{} {}", path, rec.status());
    }
}
```

Output:

```text
/account 403
/webhook 200
```

## See also

- [server::use](../server/use.md)
- [csrf](README.md)
