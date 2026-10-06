[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [handler](handler/README.md) › call

# sgcl::net::http::handler::call

```cpp
#include "sgcl/net/http/server.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class handler {
    public:
        class call;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What [handler::operator()](handler/operator_call.md) gives: an awaitable that holds the request, the writer and the
handler, and runs the handler when it is awaited (`co_await next(req, w)`). Awaiting it runs the handler's step: a
plain handler, and every plain middleware before it, finishes there and the `co_await` does not suspend; a handler that
waits gives a task, which the call awaits as `co_await` awaits a task, in the frame of the task that awaits the call.
What the handler throws is thrown from the `co_await`. The call is for one `co_await` within the expression that made it:
a call kept, or awaited twice, is a mistake; a handler to keep is [task](handler/task.md).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::handler h = [](net::http::request, net::http::response_writer w) { w.write("in a call"); };
    net::http::response_recorder rec;
    auto run = [&]() -> async::task<> {
        co_await h(net::http::test_request("GET", "/"), rec.writer());
    };
    run().wait();
    println("{}", rec.body());
}
```

Output:

```text
in a call
```

## See also

- [handler](handler/README.md)
- [middleware](middleware.md)
