[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [sessions](README.md)

# sgcl::net::http::sessions::wrap

```cpp
handler wrap(const handler& next) const;
```

The middleware around one handler, for a route of its own: the requests of that route have their session, the others
none ([session](../session/session.md) of one of them is `invalid_argument`).

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
    auto store = net::http::sessions::in_memory();
    net::http::server srv;
    srv.route("GET /app/", store.wrap([](net::http::request req, net::http::response_writer w) {
        net::http::session(req).set("page", req.url().path());
        w.write("app");
    }));
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/app/home"));
    println("{} {}", rec.body(), store.size());
}
```

Output:

```text
app 1
```

## See also

- [server::use](../server/use.md)
- [sessions](README.md)
