[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [body_limit](README.md)

# sgcl::net::http::body_limit::wrap

```cpp
handler wrap(const handler& next) const;
```

The middleware around one handler, for a route of its own: the requests of that route held to the limit, the other
routes to the server's. With [server::use](../server/use.md) the limit is every request's.

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
    auto size = [](net::http::request req, net::http::response_writer w) -> async::task<> {
        auto body = co_await req.async_bytes();
        w.write(to_string(body->size()));
    };
    net::http::server srv;
    srv.route("POST /small", net::http::body_limit(4).wrap(size));
    srv.route("POST /large", size);
    for (const char* path : {"/small", "/large"}) {
        net::http::response_recorder rec;
        rec.serve(srv, net::http::test_request("POST", path, "123456"));
        println("{} {}", path, rec.status());
    }
}
```

Output:

```text
/small 413
/large 200
```

## See also

- [server::use](../server/use.md)
- [body_limit](README.md)
