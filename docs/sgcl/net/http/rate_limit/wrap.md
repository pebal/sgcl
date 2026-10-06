[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [rate_limit](README.md)

# sgcl::net::http::rate_limit::wrap

```cpp
handler wrap(const handler& next) const;
```

The middleware around one handler, for a route of its own: the requests of that route counted, the others not. One
middleware wrapped around several routes counts them together.

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
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::rate_limit strict(0, 1);
    net::http::server srv;
    srv.route("POST /login", strict.wrap([](net::http::request, net::http::response_writer w) { w.write("tried"); }));
    srv.route("GET /", [](net::http::request, net::http::response_writer w) { w.write("home"); });
    for (const char* target : {"/login", "/login", "/"}) {
        net::http::response_recorder rec;
        rec.serve(srv, net::http::test_request(target[1] == 'l' ? "POST" : "GET", target));
        println("{} {}", target, rec.status());
    }
}
```

Output:

```text
/login 200
/login 429
/ 200
```

## See also

- [server::use](../server/use.md)
- [rate_limit](README.md)
