[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [handler](README.md)

# sgcl::net::http::handler::task

```cpp
async::task<> task(request req, response_writer w) const;
```

The request served as a task of its own: for a handler to start and keep, to give to `async::spawn`, or to wait for on
a thread (`.wait()`), where [operator()](operator_call.md) gives a call that must be awaited at once. The task holds
the request, the writer and the handler's function.

## Parameters

| Parameter | Description |
|---|---|
| `req` | the request |
| `w` | its writer |

## Return value

The task, not started.

## Complexity

Constant; the handler's own when the task runs.

## Exceptions

What the copy of the handler's function throws. What the handler throws comes out of the task.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::handler h = [](net::http::request req, net::http::response_writer w) {
        w.write("hello " + req.query("name"));
    };
    net::http::response_recorder rec;
    h.task(net::http::test_request("GET", "/?name=ann"), rec.writer()).wait();
    println("{}", rec.body());
}
```

Output:

```text
hello ann
```

## See also

- [operator()](operator_call.md)
- [handler](README.md)
