[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [recovery](README.md)

# sgcl::net::http::recovery::wrap

```cpp
handler wrap(const handler& next) const;
```

The middleware around one handler, for a route of its own: what [server::use](../server/use.md) does for every request,
done for the requests of that route.

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
    net::http::recovery soft({.answer = [](const net::http::request&, net::http::response_writer& w,
                                           const string& what) {
        w.set_status(503);
        w.write("try later (" + what + ")");
    }});
    net::http::server srv;
    srv.on_error = [](const string&) {};
    srv.route("GET /x", soft.wrap([](net::http::request, net::http::response_writer) {
        throw std::runtime_error("busy");
    }));
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/x"));
    println("{} {}", rec.status(), rec.body());
}
```

Sample output:

```text
time=2026-10-06T09:41:07.312+02:00 level=ERROR msg="handler threw" method=GET path=/x error=busy
503 try later (busy)
```

## See also

- [server::use](../server/use.md)
- [recovery](README.md)
