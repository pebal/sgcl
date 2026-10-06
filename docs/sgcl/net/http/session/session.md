[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [session](README.md)

# sgcl::net::http::session::session

```cpp
explicit session(const request& req);
```

The session of the request, as the [sessions](../sessions/README.md) middleware loaded it before the handler: the one
its cookie names, or a new one. The session is the request's own: the handle made again from the same request is the
same session.

## Parameters

| Parameter | Description |
|---|---|
| `req` | the request, in a handler under a sessions middleware |

## Complexity

Constant.

## Exceptions

`invalid_argument` when no sessions middleware ran for the request.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::sessions::in_memory());
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        net::http::session a(req);
        a.set("k", "v");
        net::http::session b(req);
        w.write(b.get("k"));
    });
    net::http::server bare;
    bare.route("GET /", [](net::http::request req, net::http::response_writer w) {
        try {
            net::http::session s(req);
        } catch (const std::invalid_argument&) {
            w.write("no session here");
        }
    });
    net::http::response_recorder one, two;
    one.serve(srv, net::http::test_request("GET", "/"));
    two.serve(bare, net::http::test_request("GET", "/"));
    println("{}, {}", one.body(), two.body());
}
```

Output:

```text
v, no session here
```

## See also

- [session](README.md)
