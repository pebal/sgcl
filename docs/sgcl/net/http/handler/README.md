[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::handler

```cpp
#include "sgcl/net/http/server.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class handler;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

A handler of either kind, its type erased: a plain function of `(request, response_writer)`, a function that returns
`async::task<>`, or what a middleware's `wrap` made of one ([middleware](../middleware.md)). It is what a
[server](../server/README.md)'s [route](../server/route.md) takes as it takes the function itself, what a middleware's
`wrap` takes and returns, and what an around middleware of the program's passes a request on to: `co_await next(r, w)`.
Go's `http.Handler`, without the interface: the kind stays known, so a plain handler under plain middlewares runs as a
call.

## Rules

- A value: a copy calls the same function, and shares what it captured.
- [operator()](operator_call.md) runs nothing until awaited; [task](task.md) is the task of a handler to keep or spawn.
- A default handler answers every request 404.

## Member types

| Type | Definition |
|---|---|
| [call](../handler-call.md) | what [operator()](operator_call.md) gives: an awaitable that runs the handler |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](handler.md) | a handler of a function, or one that answers 404 |
| [operator()](operator_call.md) | the request served when awaited |
| [task](task.md) | the request served as a task |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::handler hello = [](net::http::request, net::http::response_writer w) { w.write("hello"); };
    net::http::server srv;
    srv.route("GET /hello", net::http::body_limit(1024).wrap(hello));
    srv.route("GET /plain", hello);
    for (const char* path : {"/hello", "/plain"}) {
        net::http::response_recorder rec;
        rec.serve(srv, net::http::test_request("GET", path));
        println("{} {}", path, rec.body());
    }
}
```

Output:

```text
/hello hello
/plain hello
```

## See also

- [middleware](../middleware.md)
- [server::route](../server/route.md)
- [sgcl::net::http](../README.md)
