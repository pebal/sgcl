[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [handler](README.md)

# sgcl::net::http::handler::operator()

```cpp
call operator()(request req, response_writer w) const noexcept;
```

Gives a [call](../handler-call.md) that serves the request when it is awaited: `co_await next(req, w)` in an around
middleware. The handler runs at the `co_await`: a plain one, and every plain middleware before it, without suspending;
one that waits is awaited in the awaiting task's frame, with no frame of the call's own. What the handler throws comes
out of the `co_await`. A call never awaited runs nothing.

## Parameters

| Parameter | Description |
|---|---|
| `req` | the request |
| `w` | its writer |

## Return value

The call, to await.

## Complexity

Constant; the handler's own when awaited.

## Exceptions

None. What the handler throws comes out of the `co_await`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use([](net::http::request req, net::http::response_writer w,
               const net::http::handler& next) -> async::task<> {
        try {
            co_await next(req, w);
        } catch (const std::exception& e) {
            w.set_status(503);
            w.write(string("caught: ") + e.what());
        }
    });
    srv.route("GET /x", [](net::http::request, net::http::response_writer) { throw std::runtime_error("no"); });
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/x"));
    println("{} {}", rec.status(), rec.body());
}
```

Output:

```text
503 caught: no
```

## See also

- [task](task.md)
- [handler](README.md)
